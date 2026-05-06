#include "doctest.h"
#include "platform/file.h"
#include "platform/lock.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using openads::platform::ByteLock;
using openads::platform::File;
using openads::platform::LockKind;
using openads::platform::OpenMode;

TEST_CASE("ByteLock acquires and releases an exclusive range") {
    const auto p = fs::temp_directory_path() / "openads_test_lock_excl";
    fs::remove(p);
    {
        auto fres = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(fres.has_value());
        File f = std::move(fres).value();

        auto lock = ByteLock::acquire(f, 1000, 1, LockKind::Exclusive);
        REQUIRE(lock.has_value());
        // Releasing on scope exit; explicit release must also work:
        auto rel = std::move(lock).value().release();
        CHECK(rel.has_value());
    }
    fs::remove(p);
}

TEST_CASE("ByteLock shared lock allows another shared lock") {
    const auto p = fs::temp_directory_path() / "openads_test_lock_shared";
    fs::remove(p);
    {
        auto a = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(a.has_value());
        File fa = std::move(a).value();

        auto b = File::open(p.string(), OpenMode::OpenExisting);
        REQUIRE(b.has_value());
        File fb = std::move(b).value();

        auto la = ByteLock::acquire(fa, 5000, 1, LockKind::Shared);
        REQUIRE(la.has_value());
        auto lb = ByteLock::acquire(fb, 5000, 1, LockKind::Shared);
        REQUIRE(lb.has_value());
    }
    fs::remove(p);
}

TEST_CASE("ByteLock exclusive lock blocks a second exclusive lock (try)") {
#if defined(__APPLE__)
    // macOS BSD fcntl(F_SETLK) is process-scoped + lacks
    // F_OFD_SETLK, so two fds in the same process can't contend
    // through this primitive. The engine's lock_mgr layers a
    // userspace map on top to deliver the same Win32 / Linux
    // contract; this primitive-level test stays skipped on macOS.
    MESSAGE("skipped on macOS: fcntl(F_SETLK) is process-scoped");
    return;
#else
    const auto p = fs::temp_directory_path() / "openads_test_lock_block";
    fs::remove(p);
    {
        auto a = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(a.has_value());
        File fa = std::move(a).value();

        auto b = File::open(p.string(), OpenMode::OpenExisting);
        REQUIRE(b.has_value());
        File fb = std::move(b).value();

        auto la = ByteLock::acquire(fa, 7000, 1, LockKind::Exclusive);
        REQUIRE(la.has_value());

        auto lb = ByteLock::try_acquire(fb, 7000, 1, LockKind::Exclusive);
        CHECK_FALSE(lb.has_value());
    }
    fs::remove(p);
#endif
}
