add_library(openads_core STATIC
    util/log.cpp
    platform/path.cpp
    platform/time.cpp
    platform/thread.cpp
)

if(WIN32)
    target_sources(openads_core PRIVATE
        platform/file_win32.cpp
        platform/lock_win32.cpp
        platform/mmap_win32.cpp
    )
else()
    target_sources(openads_core PRIVATE
        platform/file_posix.cpp
        platform/lock_posix.cpp
        platform/mmap_posix.cpp
    )
endif()

target_include_directories(openads_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX)
    target_link_libraries(openads_core PUBLIC pthread)
endif()
