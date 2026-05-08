// openads_serverd — standalone TCP server CLI.
//
// Wraps openads::network::Server in a long-lived process. Parses
// --host / --port / --backlog from argv, prints the bound port,
// blocks on a signal-handled exit so a Harbour client (or any
// rddads app) can reach the server over LAN.

#include "network/server.h"
#if defined(OPENADS_WITH_HTTP)
#include "tools/serverd/http_server.h"
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) { g_running.store(false); }

void usage(const char* argv0) {
    std::fprintf(stderr,
        "usage: %s [--host HOST] [--port PORT] [--backlog N]\n"
        "          [--http-port PORT] [--data DIR] [--http-user U:P]...\n"
        "  --host       bind address (default 0.0.0.0)\n"
        "  --port       TCP wire port (default 6262, 0 = ephemeral)\n"
        "  --backlog    listen() backlog (default 16)\n"
        "  --http-port  if set, expose Studio web console on this port\n"
        "  --data       data directory the HTTP console serves\n"
        "               (default = current working directory)\n"
        "  --http-user  user:password — register a Studio login\n"
        "               (repeatable; if none given, console is open)\n"
        "  --version    print version + exit\n",
        argv0);
}

} // namespace

int main(int argc, char** argv) {
    std::string host        = "0.0.0.0";
    std::uint16_t port      = 6262;
    int backlog             = 16;
    std::uint16_t http_port = 0;             // 0 = HTTP console disabled
    std::string data_dir    = ".";
    std::vector<std::pair<std::string, std::string>> http_users;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--host"      && i + 1 < argc) host    = argv[++i];
        else if (a == "--port"      && i + 1 < argc) port    = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--backlog"   && i + 1 < argc) backlog = std::atoi(argv[++i]);
        else if (a == "--http-port" && i + 1 < argc) http_port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--data"      && i + 1 < argc) data_dir = argv[++i];
        else if (a == "--http-user" && i + 1 < argc) {
            std::string up = argv[++i];
            auto colon = up.find(':');
            if (colon == std::string::npos) {
                std::fprintf(stderr,
                    "--http-user expects user:password\n");
                return 2;
            }
            http_users.emplace_back(up.substr(0, colon),
                                     up.substr(colon + 1));
        }
        else if (a == "--version") {
            std::printf("openads_serverd 1.0.0-rc1\n");
            return 0;
        } else if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage(argv[0]);
            return 2;
        }
    }

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    openads::network::Server srv;
    auto r = srv.start(host, port);
    if (!r) {
        std::fprintf(stderr, "server start failed: %s (sub=%d)\n",
                     r.error().message.c_str(), r.error().sub_code);
        // The default OpenADS / SAP-ACE wire port is 6262. Port
        // collisions with the SAP Advantage Database Server service
        // (when both run on the same host) surface as a generic
        // bind failure; flag that case explicitly so the operator
        // knows to either stop the ADS service or pick a free port.
        if (port == 6262) {
            std::fprintf(stderr,
                "hint: port 6262 is the SAP Advantage Database Server\n"
                "      default. If ADS is running on this host you'll\n"
                "      hit a bind clash. Either stop the Advantage\n"
                "      Database Server service first, or pick a free\n"
                "      port via `--port <N>` (eg. --port 6263).\n");
        } else {
            std::fprintf(stderr,
                "hint: another process is already bound to port %u.\n"
                "      Either stop it or pick a free port via\n"
                "      `--port <N>`.\n", port);
        }
        return 1;
    }
    std::printf("openads_serverd listening on %s:%u (backlog=%d)\n",
                host.c_str(), srv.port(), backlog);
    std::fflush(stdout);

#if defined(OPENADS_WITH_HTTP)
    openads::studio::HttpConsole http;
    for (auto& u : http_users) http.add_user(u.first, u.second);
    if (http_port != 0) {
        if (!http.start(host, http_port, data_dir, &srv)) {
            std::fprintf(stderr,
                "Studio HTTP console: bind to %s:%u failed\n",
                host.c_str(), http_port);
        } else {
            std::printf("Studio web console on http://%s:%u/  (data=%s)\n",
                        host.c_str(), http_port, data_dir.c_str());
            std::fflush(stdout);
        }
    }
#else
    if (http_port != 0) {
        std::fprintf(stderr,
            "--http-port set but build lacks OPENADS_WITH_HTTP=ON\n");
    }
#endif

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::printf("openads_serverd: shutdown signal received\n");
#if defined(OPENADS_WITH_HTTP)
    if (http_port != 0) http.stop();
#endif
    srv.stop();
    return 0;
}
