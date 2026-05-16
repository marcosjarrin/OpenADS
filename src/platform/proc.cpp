#include "platform/proc.h"

#if defined(_WIN32)
#  include <windows.h>
#  include <psapi.h>
#elif defined(__APPLE__)
#  include <mach/mach.h>
#else
#  include <cstdio>
#  include <unistd.h>
#endif

namespace openads::platform {

std::uint64_t process_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<std::uint64_t>(pmc.WorkingSetSize);
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count)
            == KERN_SUCCESS)
        return static_cast<std::uint64_t>(info.resident_size);
    return 0;
#else
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0, resident = 0;
    int n = std::fscanf(f, "%ld %ld", &pages, &resident);
    std::fclose(f);
    if (n < 2) return 0;
    long pg = sysconf(_SC_PAGESIZE);
    return static_cast<std::uint64_t>(resident) *
           static_cast<std::uint64_t>(pg > 0 ? pg : 4096);
#endif
}

}  // namespace openads::platform
