#pragma once

#include "util/result.h"

#include <cstddef>
#include <string>

namespace openads::platform {

// M11.4 — minimal cross-platform DLL loader for the AEP host.
struct DllHandle {
    void* native = nullptr;
};

inline bool dll_valid(DllHandle h) noexcept { return h.native != nullptr; }

util::Result<DllHandle> dll_load(const std::string& path);
util::Result<void*>     dll_symbol(DllHandle h, const std::string& name);
void                    dll_close(DllHandle h) noexcept;

} // namespace openads::platform
