#pragma once

#include "network/socket.h"
#include "network/transport.h"
#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#if defined(OPENADS_WITH_TLS)

namespace openads::network {

// M12.12 — real TLS transport, vendored mbedtls 3.6 LTS (Apache 2.0).
//
// Available only when CMake is configured with `-DOPENADS_WITH_TLS=ON`.
// The default build keeps TLS disabled so the engine still ships
// without a network dependency at configure time. When enabled,
// AdsConnect60 with a `tls://host:port/<dir>` URI opens a
// TlsTransport instead of returning AE_FUNCTION_NOT_AVAILABLE.

struct TlsConfig {
    // PEM-encoded CA bundle the client uses to verify the server.
    // Empty ⇒ verification is skipped (testing / dev only).
    std::string ca_pem;
    // PEM-encoded server cert + key (server-side only).
    std::string cert_pem;
    std::string key_pem;
    // Hostname for SNI + verification (client-side).
    std::string sni_hostname;
    // Skip peer verification (insecure — dev/test only).
    bool insecure_skip_verify = false;
};

// Connect a TLS client to host:port, returning a transport ready
// for read_frame / write_frame.
util::Result<std::unique_ptr<ITransport>>
    connect_tls(const std::string& host, std::uint16_t port,
                const TlsConfig& cfg);

// NOTE — server-side TLS termination requires replacing the
// Socket-based listener with an mbedtls_net_context one (mbedtls
// 3.6 doesn't expose a way to adopt an externally-accepted fd).
// That refactor is queued for v1.0.x; for v1.0 the client side is
// the only thing that speaks tls://. Real-world deployments
// typically front the server with a TLS-terminating proxy
// (haproxy / nginx / stunnel) anyway.

} // namespace openads::network

#endif // OPENADS_WITH_TLS
