cmake_minimum_required(VERSION 3.14)
project(bt VERSION 1.0.0 LANGUAGES CXX)

# Header-only library
add_library(bt INTERFACE)
target_include_directories(bt INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(bt INTERFACE cxx_std_14)

# Options
option(BT_BUILD_TESTS "Build tests" ON)
option(BT_BUILD_EXAMPLES "Build examples" ON)

if(BT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS bt EXPORT btTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT btTargets
    FILE btConfig.cmake
    NAMESPACE bt::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bt
)
