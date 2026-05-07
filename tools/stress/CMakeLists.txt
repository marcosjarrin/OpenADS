add_executable(openads_stress main.cpp)
target_link_libraries(openads_stress PRIVATE openads_core)
target_include_directories(openads_stress PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

add_executable(openads_memo_stress memo_main.cpp)
target_link_libraries(openads_memo_stress PRIVATE openads_core)
target_include_directories(openads_memo_stress PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

add_executable(openads_memo_zip memo_zip_main.cpp)
target_link_libraries(openads_memo_zip PRIVATE openads_core)
target_include_directories(openads_memo_zip PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
