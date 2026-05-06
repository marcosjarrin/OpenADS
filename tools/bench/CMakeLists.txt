add_executable(openads_bench main.cpp)
target_link_libraries(openads_bench PRIVATE openads_core)
target_include_directories(openads_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
