################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version
################################################################################
cmake_minimum_required (VERSION 3.5)
project(net-lib-ALL CXX C)

option(BUILD_STRICT "Build with strict policies: C++ standard required, C++ extension is OFF etc" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DEMO "Build examples/demo" OFF)
option(ENABLE_COVERAGE "Build tests with coverage support" OFF)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

if (BUILD_STRICT)
    set(CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++XX rather than -std=gnu++XX
endif()

#include(${CMAKE_CURRENT_LIST_DIR}/3rdparty/pfs/common/library.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/library.cmake)

if (BUILD_TESTS AND EXISTS tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_DEMO)
    add_subdirectory(demo)
endif()
