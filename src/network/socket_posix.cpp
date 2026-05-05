#include "network/socket.h"

#ifndef _WIN32

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace openads::network {

util::Result<void> network_init() { return {}; }

util::Result<Socket> listen_tcp(const ListenerOptions& opts) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return util::Error{5000, errno, "socket() failed", ""};
    }
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(opts.port);
    inet_pton(AF_INET, opts.host.c_str(), &addr.sin_addr);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        int e = errno; ::close(s);
        return util::Error{5000, e, "bind() failed", opts.host};
    }
    if (::listen(s, opts.backlog) < 0) {
        int e = errno; ::close(s);
        return util::Error{5000, e, "listen() failed", ""};
    }
    Socket out;
    out.handle = static_cast<std::uintptr_t>(s);
    return out;
}

util::Result<std::uint16_t> socket_local_port(const Socket& sock) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(static_cast<int>(sock.handle),
                    reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        return util::Error{5000, errno, "getsockname failed", ""};
    }
    return static_cast<std::uint16_t>(ntohs(addr.sin_port));
}

util::Result<Socket> accept_one(Socket& listener) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int c = ::accept(static_cast<int>(listener.handle),
                     reinterpret_cast<sockaddr*>(&addr), &len);
    if (c < 0) {
        return util::Error{5000, errno, "accept() failed", ""};
    }
    Socket out;
    out.handle = static_cast<std::uintptr_t>(c);
    return out;
}

util::Result<Socket> connect_tcp(const std::string& host,
                                  std::uint16_t port) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return util::Error{5000, errno, "socket() failed", ""};
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        int e = errno; ::close(s);
        return util::Error{5000, e, "connect() failed", host};
    }
    Socket out;
    out.handle = static_cast<std::uintptr_t>(s);
    return out;
}

util::Result<std::size_t> sock_send(Socket& sock,
                                     const std::uint8_t* buf,
                                     std::size_t n) {
    ssize_t sent = ::send(static_cast<int>(sock.handle), buf, n, 0);
    if (sent < 0) {
        return util::Error{5000, errno, "send() failed", ""};
    }
    return static_cast<std::size_t>(sent);
}

util::Result<std::size_t> sock_recv(Socket& sock,
                                     std::uint8_t* buf,
                                     std::size_t n) {
    ssize_t got = ::recv(static_cast<int>(sock.handle), buf, n, 0);
    if (got < 0) {
        return util::Error{5000, errno, "recv() failed", ""};
    }
    return static_cast<std::size_t>(got);
}

void sock_close(Socket& sock) noexcept {
    if (sock.valid()) {
        ::close(static_cast<int>(sock.handle));
        sock.handle = static_cast<std::uintptr_t>(-1);
    }
}

} // namespace openads::network

#endif // !_WIN32
