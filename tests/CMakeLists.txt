include(FetchContent)

set(CATCH2_VERSION v2.13.10)
FetchContent_Declare(
    Catch2
    URL https://ghfast.top/https://github.com/catchorg/Catch2/archive/refs/tags/${CATCH2_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(Catch2)

add_executable(bt_tests
    test_main.cpp
    test_status.cpp
    test_node.cpp
    test_action.cpp
    test_sequence.cpp
    test_selector.cpp
    test_parallel.cpp
    test_inverter.cpp
    test_behavior_tree.cpp
    test_factory.cpp
    test_validate.cpp
    test_edge_cases.cpp
)

target_link_libraries(bt_tests PRIVATE bt Catch2::Catch2)
target_compile_definitions(bt_tests PRIVATE BT_USE_STD_FUNCTION)
target_compile_options(bt_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_test(NAME bt_tests COMMAND bt_tests)
