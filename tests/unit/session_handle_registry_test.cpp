#include "doctest.h"
#include "session/handle_registry.h"

using openads::session::HandleKind;
using openads::session::HandleRegistry;

TEST_CASE("HandleRegistry assigns distinct, non-zero handles") {
    HandleRegistry reg;
    int a = 1, b = 2;
    auto h1 = reg.register_object(HandleKind::Connection, &a);
    auto h2 = reg.register_object(HandleKind::Connection, &b);
    CHECK(h1 != 0);
    CHECK(h2 != 0);
    CHECK(h1 != h2);
}

TEST_CASE("HandleRegistry resolves and respects the kind tag") {
    HandleRegistry reg;
    int conn_obj = 0;
    int tbl_obj  = 0;
    auto hc = reg.register_object(HandleKind::Connection, &conn_obj);
    auto ht = reg.register_object(HandleKind::Table,      &tbl_obj);

    CHECK(reg.lookup<int>(hc, HandleKind::Connection) == &conn_obj);
    CHECK(reg.lookup<int>(ht, HandleKind::Table)      == &tbl_obj);

    // Wrong-kind lookup returns nullptr.
    CHECK(reg.lookup<int>(hc, HandleKind::Table) == nullptr);
}

TEST_CASE("HandleRegistry returns nullptr for unknown handles") {
    HandleRegistry reg;
    CHECK(reg.lookup<int>(0,        HandleKind::Connection) == nullptr);
    CHECK(reg.lookup<int>(99999999, HandleKind::Connection) == nullptr);
}

TEST_CASE("HandleRegistry releases handles") {
    HandleRegistry reg;
    int o = 0;
    auto h = reg.register_object(HandleKind::Table, &o);
    REQUIRE(reg.lookup<int>(h, HandleKind::Table) != nullptr);
    reg.release(h);
    CHECK(reg.lookup<int>(h, HandleKind::Table) == nullptr);
}
