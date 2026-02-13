add_executable(bt_basic_example basic_example.cpp)
target_link_libraries(bt_basic_example PRIVATE bt)
target_compile_definitions(bt_basic_example PRIVATE BT_USE_STD_FUNCTION)
target_compile_options(bt_basic_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(bt_example bt_example.cpp)
target_link_libraries(bt_example PRIVATE bt)
target_compile_definitions(bt_example PRIVATE BT_USE_STD_FUNCTION)
target_compile_options(bt_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
