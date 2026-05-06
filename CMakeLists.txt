cmake_minimum_required(VERSION 3.20)

project(OpenADS
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Open-source ADS-compatible engine"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(OPENADS_BUILD_TESTS "Build OpenADS unit tests" ON)
option(OPENADS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# M12.12 — real TLS transport via vendored mbedtls (Apache-2.0).
# Off by default so existing builds don't pull a network dependency
# at configure time. When ON, FetchContent downloads mbedtls 3.6 LTS
# at configure time and links it into openads_core; AdsConnect60
# with a `tls://` URI then opens a TlsTransport instead of returning
# AE_FUNCTION_NOT_AVAILABLE.
option(OPENADS_WITH_TLS "Enable TLS transport (vendors mbedtls 3.6 LTS via FetchContent)" OFF)

# Pull in mbedtls FIRST so our strict /WX -Werror flags don't bleed
# into the upstream sources (C4200 zero-sized arrays etc). Each
# OpenADS target adds the strict flags target-locally below via
# `apply_openads_warnings(<target>)`.
if(OPENADS_WITH_TLS)
    include(FetchContent)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG        v3.6.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(mbedtls)
    message(STATUS "OpenADS: TLS enabled via vendored mbedtls 3.6.2")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if(OPENADS_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(OPENADS_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

add_subdirectory(src)
add_subdirectory(tools/serverd)
add_subdirectory(tools/bench)

if(OPENADS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
