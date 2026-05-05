#include "network/server.h"

#include "engine/table.h"
#include "session/connection.h"

#include <cstring>
#include <memory>
#include <unordered_map>

namespace openads::network {

util::Result<void> recv_exact(Socket& s, std::uint8_t* buf,
                              std::size_t n) {
    std::size_t got = 0;
    while (got < n) {
        auto r = sock_recv(s, buf + got, n - got);
        if (!r) return r.error();
        if (r.value() == 0) {
            return util::Error{5000, 0, "peer closed connection", ""};
        }
        got += r.value();
    }
    return {};
}

util::Result<Frame> read_frame(Socket& s) {
    std::uint8_t hdr[5];
    if (auto r = recv_exact(s, hdr, sizeof(hdr)); !r) return r.error();
    std::uint32_t n =
        (static_cast<std::uint32_t>(hdr[0]) << 24) |
        (static_cast<std::uint32_t>(hdr[1]) << 16) |
        (static_cast<std::uint32_t>(hdr[2]) <<  8) |
         static_cast<std::uint32_t>(hdr[3]);
    Frame f;
    f.opcode = static_cast<Opcode>(hdr[4]);
    if (n > 0) {
        f.payload.resize(n);
        if (auto r = recv_exact(s, f.payload.data(), n); !r) return r.error();
    }
    return f;
}

util::Result<void> write_frame(Socket& s, const Frame& f) {
    auto enc = encode_frame(f);
    if (!enc) return enc.error();
    auto& bytes = enc.value();
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        auto r = sock_send(s, bytes.data() + sent, bytes.size() - sent);
        if (!r) return r.error();
        if (r.value() == 0) {
            return util::Error{5000, 0, "send returned 0", ""};
        }
        sent += r.value();
    }
    return {};
}

Server::~Server() { stop(); }

util::Result<void> Server::start(const std::string& host,
                                 std::uint16_t port) {
    if (running_.load()) return {};
    ListenerOptions opts;
    opts.host = host;
    opts.port = port;
    auto l = listen_tcp(opts);
    if (!l) return l.error();
    listener_ = l.value();
    auto p = socket_local_port(listener_);
    if (!p) {
        sock_close(listener_);
        return p.error();
    }
    port_ = p.value();
    running_.store(true);
    accept_thread_ = std::thread([this]() { this->accept_loop(); });
    return {};
}

void Server::stop() noexcept {
    if (!running_.exchange(false)) return;
    // Closing the listener wakes the blocking accept() with an
    // error on both Win32 and POSIX.
    sock_close(listener_);
    if (accept_thread_.joinable()) accept_thread_.join();
    std::lock_guard<std::mutex> lk(sessions_mu_);
    for (auto& t : sessions_) {
        if (t.joinable()) t.join();
    }
    sessions_.clear();
}

void Server::accept_loop() {
    while (running_.load()) {
        auto cli = accept_one(listener_);
        if (!cli) {
            // accept failed — likely listener closed by stop().
            break;
        }
        Socket s = cli.value();
        std::lock_guard<std::mutex> lk(sessions_mu_);
        sessions_.emplace_back([this, s]() mutable {
            this->session_loop(s);
        });
    }
}

namespace {

inline std::uint32_t read_u32_le(const std::uint8_t* p) {
    return  static_cast<std::uint32_t>(p[0])        |
           (static_cast<std::uint32_t>(p[1]) <<  8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void write_u32_le(std::uint32_t v, std::vector<std::uint8_t>& out) {
    out.push_back(static_cast<std::uint8_t>( v        & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >>  8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

} // namespace

void Server::session_loop(Socket s) {
    // M12.4 — per-session state. Connection is opened by the
    // Connect frame; OpenTable allocates a session-scoped 32-bit
    // table id keyed into engine handles.
    std::unique_ptr<openads::session::Connection> sess_conn;
    std::unordered_map<std::uint32_t, openads::session::Handle> tbls;
    std::uint32_t next_id = 1;

    auto err = [](const char* msg) {
        Frame f;
        f.opcode = Opcode::Error;
        std::string e(msg);
        f.payload.assign(e.begin(), e.end());
        return f;
    };

    while (true) {
        auto fr = read_frame(s);
        if (!fr) break;                       // connection closed
        const Frame& f = fr.value();
        Frame reply;
        switch (f.opcode) {
            case Opcode::Hello: {
                reply.opcode = Opcode::HelloAck;
                std::string v = "openads/0.3.2";
                reply.payload.assign(v.begin(), v.end());
                break;
            }
            case Opcode::Connect: {
                std::string dir(reinterpret_cast<const char*>(
                                    f.payload.data()),
                                f.payload.size());
                auto co = openads::session::Connection::open(dir);
                if (!co) { reply = err("Connect: connection open failed"); break; }
                sess_conn = std::make_unique<openads::session::Connection>(
                    std::move(co).value());
                reply.opcode = Opcode::ConnectAck;
                std::string p = "connected:" + dir;
                reply.payload.assign(p.begin(), p.end());
                break;
            }
            case Opcode::Disconnect: {
                sock_close(s);
                return;
            }
            case Opcode::OpenTable: {
                if (!sess_conn) { reply = err("OpenTable: not connected"); break; }
                std::string rel(reinterpret_cast<const char*>(
                                    f.payload.data()),
                                f.payload.size());
                auto th = sess_conn->open_table(rel,
                    openads::engine::TableType::Cdx,
                    openads::engine::OpenMode::Shared);
                if (!th) { reply = err("OpenTable: open failed"); break; }
                std::uint32_t id = next_id++;
                tbls.emplace(id, th.value());
                reply.opcode = Opcode::OpenTableAck;
                write_u32_le(id, reply.payload);
                break;
            }
            case Opcode::CloseTable: {
                if (f.payload.size() < 4) { reply = err("CloseTable: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                auto it = tbls.find(id);
                if (it != tbls.end()) {
                    sess_conn->close_table(it->second);
                    tbls.erase(it);
                }
                reply.opcode = Opcode::CloseTableAck;
                break;
            }
            case Opcode::GotoTop: {
                if (f.payload.size() < 4) { reply = err("GotoTop: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                auto it = tbls.find(id);
                if (it == tbls.end() || !sess_conn) {
                    reply = err("GotoTop: bad table id"); break;
                }
                auto* tbl = sess_conn->lookup_table(it->second);
                if (!tbl) { reply = err("GotoTop: lookup failed"); break; }
                (void)tbl->goto_top();
                reply.opcode = Opcode::GotoTopAck;
                break;
            }
            case Opcode::Skip: {
                if (f.payload.size() < 8) { reply = err("Skip: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                std::int32_t step = static_cast<std::int32_t>(
                    read_u32_le(f.payload.data() + 4));
                auto it = tbls.find(id);
                if (it == tbls.end() || !sess_conn) {
                    reply = err("Skip: bad table id"); break;
                }
                auto* tbl = sess_conn->lookup_table(it->second);
                if (!tbl) { reply = err("Skip: lookup failed"); break; }
                (void)tbl->skip(step);
                reply.opcode = Opcode::SkipAck;
                break;
            }
            case Opcode::GetField: {
                if (f.payload.size() < 5) { reply = err("GetField: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                std::string fname(reinterpret_cast<const char*>(
                                      f.payload.data() + 4),
                                  f.payload.size() - 4);
                auto it = tbls.find(id);
                if (it == tbls.end() || !sess_conn) {
                    reply = err("GetField: bad table id"); break;
                }
                auto* tbl = sess_conn->lookup_table(it->second);
                if (!tbl) { reply = err("GetField: lookup failed"); break; }
                std::int32_t fi = tbl->field_index(fname);
                if (fi < 0) { reply = err("GetField: column not found"); break; }
                auto v = tbl->read_field(static_cast<std::uint16_t>(fi));
                if (!v) { reply = err("GetField: read failed"); break; }
                reply.opcode = Opcode::GetFieldAck;
                auto& sval = v.value().as_string;
                reply.payload.assign(sval.begin(), sval.end());
                break;
            }
            case Opcode::GetRecordCount: {
                if (f.payload.size() < 4) { reply = err("GetRecordCount: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                auto it = tbls.find(id);
                if (it == tbls.end() || !sess_conn) {
                    reply = err("GetRecordCount: bad table id"); break;
                }
                auto* tbl = sess_conn->lookup_table(it->second);
                if (!tbl) { reply = err("GetRecordCount: lookup failed"); break; }
                std::uint32_t rc = tbl->record_count();
                reply.opcode = Opcode::GetRecordCountAck;
                write_u32_le(rc, reply.payload);
                break;
            }
            case Opcode::AtEOF: {
                if (f.payload.size() < 4) { reply = err("AtEOF: bad payload"); break; }
                std::uint32_t id = read_u32_le(f.payload.data());
                auto it = tbls.find(id);
                if (it == tbls.end() || !sess_conn) {
                    reply = err("AtEOF: bad table id"); break;
                }
                auto* tbl = sess_conn->lookup_table(it->second);
                if (!tbl) { reply = err("AtEOF: lookup failed"); break; }
                reply.opcode = Opcode::AtEOFAck;
                reply.payload.push_back(tbl->eof() ? 1 : 0);
                break;
            }
            default: {
                reply = err("unsupported opcode");
                break;
            }
        }
        if (auto wr = write_frame(s, reply); !wr) break;
    }
    sock_close(s);
}

} // namespace openads::network
