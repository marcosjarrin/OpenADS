#pragma once

#if defined(OPENADS_WITH_HTTP)

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace httplib {
class Server;
}

namespace openads::studio {

// studio.web.0.1 — embedded HTTP web console hosted inside the
// openads_serverd CLI. The Phase 2 wire (`tcp://` / `tls://`)
// keeps running on its own port; this class adds a parallel HTTP
// listener that:
//
//   * serves a single-page admin UI (HTML / CSS / JS) embedded
//     in the binary at compile time;
//   * exposes a small REST surface under `/api/` that wraps the
//     same `Ads*` C ABI surface the rest of the engine uses.
//
// The HTTP listener runs the database operations through a
// short-lived ABI connection per request (M12.7 pattern). State
// (open tables / cursors) lives only for the duration of the
// response.

class HttpConsole {
public:
    HttpConsole();
    ~HttpConsole();
    HttpConsole(const HttpConsole&) = delete;
    HttpConsole& operator=(const HttpConsole&) = delete;

    // `data_dir` is the on-disk directory the engine connects to
    // for every request. `host` / `port` follow the same defaults
    // as the wire server.
    bool start(const std::string& host,
               std::uint16_t      port,
               const std::string& data_dir);
    void stop();
    bool running() const noexcept { return running_.load(); }

private:
    std::unique_ptr<httplib::Server> srv_;
    std::thread                      thread_;
    std::atomic<bool>                running_{false};
    std::string                      data_dir_;
};

} // namespace openads::studio

#endif // OPENADS_WITH_HTTP
