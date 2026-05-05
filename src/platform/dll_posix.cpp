#include "platform/dll.h"

#ifndef _WIN32

#include <dlfcn.h>

namespace openads::platform {

util::Result<DllHandle> dll_load(const std::string& path) {
    void* m = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m) {
        const char* err = dlerror();
        return util::Error{5000, 0,
                           err ? err : "dlopen failed", path};
    }
    DllHandle h;
    h.native = m;
    return h;
}

util::Result<void*> dll_symbol(DllHandle h, const std::string& name) {
    if (!dll_valid(h)) {
        return util::Error{5000, 0, "invalid DLL handle", name};
    }
    dlerror();
    void* p = dlsym(h.native, name.c_str());
    const char* err = dlerror();
    if (err) {
        return util::Error{5000, 0, err, name};
    }
    return p;
}

void dll_close(DllHandle h) noexcept {
    if (h.native) dlclose(h.native);
}

} // namespace openads::platform

#endif // !_WIN32
