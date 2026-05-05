#include "platform/dll.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace openads::platform {

util::Result<DllHandle> dll_load(const std::string& path) {
    HMODULE m = LoadLibraryA(path.c_str());
    if (!m) {
        return util::Error{5000, static_cast<int>(GetLastError()),
                           "LoadLibrary failed", path};
    }
    DllHandle h;
    h.native = reinterpret_cast<void*>(m);
    return h;
}

util::Result<void*> dll_symbol(DllHandle h, const std::string& name) {
    if (!dll_valid(h)) {
        return util::Error{5000, 0, "invalid DLL handle", name};
    }
    FARPROC p = GetProcAddress(reinterpret_cast<HMODULE>(h.native),
                               name.c_str());
    if (!p) {
        return util::Error{5000, static_cast<int>(GetLastError()),
                           "GetProcAddress failed", name};
    }
    return reinterpret_cast<void*>(p);
}

void dll_close(DllHandle h) noexcept {
    if (h.native) {
        FreeLibrary(reinterpret_cast<HMODULE>(h.native));
    }
}

} // namespace openads::platform

#endif // _WIN32
