add_executable(openads_unit_tests
    unit/doctest_main.cpp
    unit/util_result_test.cpp
    unit/util_span_test.cpp
    unit/util_log_test.cpp
    unit/platform_file_test.cpp
    unit/platform_lock_test.cpp
    unit/platform_mmap_test.cpp
    unit/platform_path_test.cpp
    unit/platform_time_test.cpp
    unit/platform_thread_test.cpp
)

target_include_directories(openads_unit_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_include_directories(openads_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(openads_unit_tests PRIVATE openads_core)

add_test(NAME openads_unit_tests COMMAND openads_unit_tests)
