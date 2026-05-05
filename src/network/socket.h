#pragma once

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace openads::network {

// M12.2 — minimal cross-platform TCP socket layer for the Phase 2
// server. Just enough surface to: bind+listen on an ephemeral port,
// accept a single connection, connect from a client, send + recv
// raw bytes, and close. Server multiplexing + threaded accept loop
// build on top of these primitives.

struct Socket {
    // Underlying handle. Win32 SOCKET is a UINT_PTR; POSIX uses int.
    // We carry the wider type so both implementations fit.
    std::uintptr_t handle = static_cast<std::uintptr_t>(-1);
    bool valid() const noexcept {
        return handle != static_cast<std::uintptr_t>(-1);
    }
};

struct ListenerOptions {
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;          // 0 = ephemeral
    int           backlog = 16;
};

util::Result<Socket>      listen_tcp(const ListenerOptions& opts);
util::Result<std::uint16_t> socket_local_port(const Socket& sock);
util::Result<Socket>      accept_one(Socket& listener);
util::Result<Socket>      connect_tcp(const std::string& host,
                                       std::uint16_t port);
util::Result<std::size_t> sock_send(Socket& sock,
                                     const std::uint8_t* buf,
                                     std::size_t n);
util::Result<std::size_t> sock_recv(Socket& sock,
                                     std::uint8_t* buf,
                                     std::size_t n);
void                      sock_close(Socket& sock) noexcept;

// Process-wide network init (Winsock2 WSAStartup on Windows; no-op
// on POSIX). Idempotent.
util::Result<void>        network_init();

} // namespace openads::network
