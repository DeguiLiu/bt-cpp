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

find_package(Threads REQUIRED)
add_executable(bt_async_example async_example.cpp)
target_link_libraries(bt_async_example PRIVATE bt Threads::Threads)
target_compile_options(bt_async_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(bt_threadpool_example threadpool_example.cpp)
target_link_libraries(bt_threadpool_example PRIVATE bt thread_pool Threads::Threads)
target_compile_options(bt_threadpool_example PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(bt_benchmark benchmark_example.cpp)
target_link_libraries(bt_benchmark PRIVATE bt)
target_compile_definitions(bt_benchmark PRIVATE BT_USE_STD_FUNCTION)
target_compile_options(bt_benchmark PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O2>
)
