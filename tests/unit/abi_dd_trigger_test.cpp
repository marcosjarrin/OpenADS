#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_minimal_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    const std::uint16_t hdr_size = 32 + 32 + 1;
    hdr[8] = hdr_size & 0xFF; hdr[9] = (hdr_size >> 8) & 0xFF;
    hdr[10] = 1 + 4; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "ID", 11);
    fd[11] = 'N'; fd[16] = 4; fd[17] = 0;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

struct TrigFixture {
    fs::path  dir;
    ADSHANDLE hConn = 0;

    TrigFixture() {
        dir = fs::temp_directory_path() / "openads_dd_trig";
        std::error_code ec;
        fs::remove_all(dir, ec);
        make_minimal_dbf(dir, "orders.dbf");

        auto add_path = (dir / "openads.add").string();
        UNSIGNED8 add_buf[260];
        std::memcpy(add_buf, add_path.c_str(), add_path.size() + 1);
        REQUIRE(AdsDDCreate(add_buf, 0, nullptr, &hConn) == 0);
        UNSIGNED8 alias[16] = "orders";
        UNSIGNED8 path[32]  = "orders.dbf";
        REQUIRE(AdsDDAddTable(hConn, alias, path, 0, 0, nullptr, nullptr) == 0);
    }

    ~TrigFixture() {
        if (hConn) AdsDisconnect(hConn);
    }
};

} // namespace

TEST_CASE("AdsDDCreateTrigger + AdsDDGetTriggerProperty round-trip") {
    TrigFixture f;

    UNSIGNED8 name[32]  = "trg_orders_insert";
    UNSIGNED8 table[16] = "orders";
    UNSIGNED8 container[64] = "triggers.dll";
    UNSIGNED8 proc[64]  = "on_insert";

    REQUIRE(AdsDDCreateTrigger(f.hConn, name, table,
                                ADS_BEFORE_INSERT,
                                0, container, proc, 10) == 0);

    // Read table alias
    char buf[128];
    UNSIGNED16 len = sizeof(buf);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_TABLE,
                                     buf, &len) == 0);
    CHECK(std::string(buf, len) == "orders");

    // Read event mask
    UNSIGNED32 event_mask = 0;
    len = sizeof(event_mask);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_EVENT,
                                     &event_mask, &len) == 0);
    CHECK(event_mask == ADS_BEFORE_INSERT);

    // Read container
    len = sizeof(buf);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_CONTAINER,
                                     buf, &len) == 0);
    CHECK(std::string(buf, len) == "triggers.dll");

    // Read procedure name
    len = sizeof(buf);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_PROC_NAME,
                                     buf, &len) == 0);
    CHECK(std::string(buf, len) == "on_insert");

    // Read enabled flag
    UNSIGNED32 enabled = 0;
    len = sizeof(enabled);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_ENABLED,
                                     &enabled, &len) == 0);
    CHECK(enabled == 1);

    // Read priority
    UNSIGNED32 priority = 0;
    len = sizeof(priority);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_PRIORITY,
                                     &priority, &len) == 0);
    CHECK(priority == 10);
}

TEST_CASE("AdsDDSetTriggerProperty — disable trigger + change proc name") {
    TrigFixture f;

    UNSIGNED8 name[32]  = "trg_upd";
    UNSIGNED8 table[16] = "orders";
    REQUIRE(AdsDDCreateTrigger(f.hConn, name, table,
                                ADS_AFTER_UPDATE, 0, nullptr, nullptr, 0) == 0);

    // Disable it
    UNSIGNED32 disabled = 0;
    REQUIRE(AdsDDSetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_ENABLED,
                                     &disabled, sizeof(disabled)) == 0);

    UNSIGNED32 enabled = 99;
    UNSIGNED16 len = sizeof(enabled);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_ENABLED,
                                     &enabled, &len) == 0);
    CHECK(enabled == 0);

    // Change proc name
    const char* new_proc = "new_handler";
    REQUIRE(AdsDDSetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_PROC_NAME,
                                     const_cast<char*>(new_proc),
                                     static_cast<UNSIGNED16>(std::strlen(new_proc))) == 0);
    char buf[64]; len = sizeof(buf);
    REQUIRE(AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_PROC_NAME,
                                     buf, &len) == 0);
    CHECK(std::string(buf, len) == "new_handler");
}

TEST_CASE("AdsDDDropTrigger — removes trigger") {
    TrigFixture f;

    UNSIGNED8 name[32]  = "trg_del";
    UNSIGNED8 table[16] = "orders";
    REQUIRE(AdsDDCreateTrigger(f.hConn, name, table,
                                ADS_BEFORE_DELETE, 0, nullptr, nullptr, 0) == 0);

    REQUIRE(AdsDDDropTrigger(f.hConn, name) == 0);

    // Getting property should now fail
    char buf[64]; UNSIGNED16 len = sizeof(buf);
    UNSIGNED32 rc = AdsDDGetTriggerProperty(f.hConn, name, ADS_DD_TRIGGER_TABLE,
                                             buf, &len);
    CHECK(rc != 0);
}

TEST_CASE("AdsDDCreateTrigger — persists across DD reopen") {
    const auto dir = fs::temp_directory_path() / "openads_dd_trig_persist";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_minimal_dbf(dir, "inv.dbf");

    auto add_path = (dir / "openads.add").string();
    UNSIGNED8 add_buf[260];
    std::memcpy(add_buf, add_path.c_str(), add_path.size() + 1);

    {
        ADSHANDLE hConn = 0;
        REQUIRE(AdsDDCreate(add_buf, 0, nullptr, &hConn) == 0);
        UNSIGNED8 alias[8] = "inv", path[16] = "inv.dbf";
        REQUIRE(AdsDDAddTable(hConn, alias, path, 0, 0, nullptr, nullptr) == 0);
        UNSIGNED8 name[32] = "trg_inv";
        UNSIGNED8 table[8] = "inv";
        UNSIGNED8 proc[32] = "audit_insert";
        REQUIRE(AdsDDCreateTrigger(hConn, name, table,
                                    ADS_AFTER_INSERT, 0, nullptr, proc, 1) == 0);
        AdsDisconnect(hConn);
    }

    {
        ADSHANDLE hConn = 0;
        REQUIRE(AdsConnect60(add_buf, ADS_LOCAL_SERVER, nullptr, nullptr, 0, &hConn) == 0);
        UNSIGNED8 name[32] = "trg_inv";
        char buf[64]; UNSIGNED16 len = sizeof(buf);
        REQUIRE(AdsDDGetTriggerProperty(hConn, name, ADS_DD_TRIGGER_PROC_NAME,
                                         buf, &len) == 0);
        CHECK(std::string(buf, len) == "audit_insert");
        AdsDisconnect(hConn);
    }
}

TEST_CASE("AdsDDDropTrigger — unknown trigger returns error") {
    TrigFixture f;
    UNSIGNED8 name[32] = "nonexistent";
    CHECK(AdsDDDropTrigger(f.hConn, name) != 0);
}
