#include "tools/serverd/http_server.h"

#if defined(OPENADS_WITH_HTTP)

#include "tools/serverd/spa_index.h"

#include "openads/ace.h"
#include "openads/error.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using nlohmann::json;
namespace fs = std::filesystem;

namespace openads::studio {

namespace {

// Open a fresh ABI connection per request. Stateless web requests
// + per-request connection is the simplest correctness model:
// the engine's existing locking handles concurrent connections.
struct AbiSession {
    ADSHANDLE conn = 0;
    explicit AbiSession(const std::string& dir) {
        std::vector<UNSIGNED8> buf(dir.size() + 1);
        std::memcpy(buf.data(), dir.c_str(), dir.size() + 1);
        AdsConnect60(buf.data(), ADS_LOCAL_SERVER,
                     nullptr, nullptr, 0, &conn);
    }
    ~AbiSession() {
        if (conn != 0) AdsDisconnect(conn);
    }
    bool ok() const noexcept { return conn != 0; }
};

json json_error(const std::string& msg, int http_code) {
    return json{{"error", msg}, {"http_code", http_code}};
}

// List `*.dbf` files in the data dir.
std::vector<std::string> list_dbf_files(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (ext == ".dbf") out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Run a SQL string + materialise up to `limit` rows as a row-major
// table { cols, rows }.
json run_sql(const std::string& dir, const std::string& sql,
             std::uint32_t limit) {
    AbiSession sess(dir);
    if (!sess.ok()) return json_error("could not open data dir: " + dir, 500);

    ADSHANDLE hStmt = 0;
    AdsCreateSQLStatement(sess.conn, &hStmt);
    std::vector<UNSIGNED8> sqlbuf(sql.size() + 1);
    std::memcpy(sqlbuf.data(), sql.c_str(), sql.size() + 1);
    ADSHANDLE hCur = 0;
    UNSIGNED32 rrc = AdsExecuteSQLDirect(hStmt, sqlbuf.data(), &hCur);
    if (rrc != 0) {
        AdsCloseSQLStatement(hStmt);
        char emsg[512] = {0};
        UNSIGNED16 elen = sizeof(emsg);
        UNSIGNED32 ecode = 0;
        AdsGetLastError(&ecode,
                        reinterpret_cast<UNSIGNED8*>(emsg), &elen);
        return json_error(std::string("AdsExecuteSQLDirect failed (") +
                          std::to_string(rrc) + "): " + std::string(emsg, elen),
                          400);
    }
    json out{{"cols", json::array()}, {"rows", json::array()},
             {"rows_returned", 0}};
    if (hCur == 0) {
        AdsCloseSQLStatement(hStmt);
        return out;
    }
    UNSIGNED16 nfields = 0;
    AdsGetNumFields(hCur, &nfields);
    std::vector<std::string> col_names;
    col_names.reserve(nfields);
    for (UNSIGNED16 i = 1; i <= nfields; ++i) {
        UNSIGNED8 nm[128] = {0};
        UNSIGNED16 cap = sizeof(nm);
        AdsGetFieldName(hCur, i, nm, &cap);
        col_names.emplace_back(reinterpret_cast<char*>(nm), cap);
        out["cols"].push_back(col_names.back());
    }
    AdsGotoTop(hCur);
    UNSIGNED16 atend = 0;
    AdsAtEOF(hCur, &atend);
    std::uint32_t walked = 0;
    while (atend == 0 && walked < limit) {
        json row = json::array();
        for (auto& cn : col_names) {
            UNSIGNED8 fbuf[64] = {0};
            std::size_t n = std::min<std::size_t>(cn.size(), sizeof(fbuf) - 1);
            std::memcpy(fbuf, cn.data(), n);
            UNSIGNED8 vbuf[4096] = {0};
            UNSIGNED32 vcap = sizeof(vbuf);
            UNSIGNED32 fr = AdsGetField(hCur, fbuf, vbuf, &vcap, 0);
            if (fr != 0) vcap = 0;
            // Trim trailing spaces (DBF blank-pads fixed-width).
            while (vcap > 0 && vbuf[vcap - 1] == ' ') --vcap;
            row.push_back(std::string(reinterpret_cast<char*>(vbuf), vcap));
        }
        out["rows"].push_back(std::move(row));
        ++walked;
        AdsSkip(hCur, 1);
        AdsAtEOF(hCur, &atend);
    }
    out["rows_returned"] = walked;
    AdsCloseTable(hCur);
    AdsCloseSQLStatement(hStmt);
    return out;
}

} // namespace

HttpConsole::HttpConsole() : srv_(std::make_unique<httplib::Server>()) {}
HttpConsole::~HttpConsole() { stop(); }

bool HttpConsole::start(const std::string& host,
                         std::uint16_t      port,
                         const std::string& data_dir) {
    data_dir_ = data_dir;
    auto& srv = *srv_;

    srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(kSpaIndexHtml, "text/html; charset=utf-8");
    });

    srv.Get("/api/health", [this](const httplib::Request&,
                                   httplib::Response& res) {
        json j{{"status", "ok"},
               {"engine", "openads"},
               {"data_dir", data_dir_}};
        res.set_content(j.dump(), "application/json");
    });

    srv.Get("/api/tables",
            [this](const httplib::Request&, httplib::Response& res) {
        auto tables = list_dbf_files(data_dir_);
        json j{{"data_dir", data_dir_},
               {"tables",   tables}};
        res.set_content(j.dump(), "application/json");
    });

    srv.Post("/api/sql",
             [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(json_error("invalid JSON body", 400).dump(),
                            "application/json");
            return;
        }
        std::string sql = body.value("sql", "");
        std::uint32_t limit = body.value("limit", 200u);
        if (sql.empty()) {
            res.status = 400;
            res.set_content(json_error("missing 'sql' field", 400).dump(),
                            "application/json");
            return;
        }
        json j = run_sql(data_dir_, sql, limit);
        if (j.contains("error")) res.status = j.value("http_code", 500);
        res.set_content(j.dump(), "application/json");
    });

    if (!srv.bind_to_port(host, port)) return false;
    running_.store(true);
    thread_ = std::thread([this]() {
        srv_->listen_after_bind();
        running_.store(false);
    });
    return true;
}

void HttpConsole::stop() {
    if (!running_.load() && !thread_.joinable()) return;
    if (srv_) srv_->stop();
    if (thread_.joinable()) thread_.join();
    running_.store(false);
}

} // namespace openads::studio

#endif // OPENADS_WITH_HTTP
