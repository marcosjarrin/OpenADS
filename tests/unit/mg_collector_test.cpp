#include "doctest.h"

#include "mgmt/mg_collector.h"
#include "mgmt/mg_snapshot.h"
#include "mgmt/mg_stats.h"

#include <cstring>
#include <string>

using openads::mgmt::MgCollector;
using openads::mgmt::MgSnapshot;
using openads::mgmt::MgStats;

namespace {
std::string c_str_of(const UNSIGNED8* p, std::size_t cap) {
    std::size_t n = 0;
    while (n < cap && p[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}
}  // namespace

TEST_CASE("MgCollector::install_info reports the product string") {
    MgSnapshot snap;
    MgStats    stats;
    MgCollector c(snap, stats);

    ADS_MGMT_INSTALL_INFO info = c.install_info();
    CHECK(c_str_of(info.aucVersionStr, sizeof(info.aucVersionStr))
              .rfind("OpenADS", 0) == 0);
    // OpenADS is not serial-licensed: serial reports empty.
    CHECK(c_str_of(info.aucSerialNumber,
                   sizeof(info.aucSerialNumber)).empty());
}
