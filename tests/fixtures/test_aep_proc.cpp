// M11.4 — side DLL exposing a stored procedure for AEP host tests.
// Two procs:
//   sum_proc     — parses two integers from "a\x1fb", writes sum
//   echo_proc    — copies args verbatim to out_buf
//   error_proc   — returns 42 (non-zero) without filling out_buf

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#  define OPENADS_TEST_EXPORT extern "C" __declspec(dllexport)
#else
#  define OPENADS_TEST_EXPORT extern "C" __attribute__((visibility("default")))
#endif

OPENADS_TEST_EXPORT int sum_proc(const char* args,
                                 char* out_buf, std::size_t out_cap) {
    if (!args || !out_buf || out_cap == 0) return 1;
    std::string s(args);
    auto sep = s.find('\x1f');
    long a = std::strtol(s.substr(0, sep).c_str(), nullptr, 10);
    long b = (sep == std::string::npos) ? 0
             : std::strtol(s.substr(sep + 1).c_str(), nullptr, 10);
    long sum = a + b;
    int n = std::snprintf(out_buf, out_cap, "%ld", sum);
    if (n < 0) return 2;
    return 0;
}

OPENADS_TEST_EXPORT int echo_proc(const char* args,
                                  char* out_buf, std::size_t out_cap) {
    if (!args || !out_buf || out_cap == 0) return 1;
    std::size_t n = std::strlen(args);
    if (n >= out_cap) n = out_cap - 1;
    std::memcpy(out_buf, args, n);
    out_buf[n] = '\0';
    return 0;
}

OPENADS_TEST_EXPORT int error_proc(const char* /*args*/,
                                   char* /*out_buf*/,
                                   std::size_t /*out_cap*/) {
    return 42;
}
