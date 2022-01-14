################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version
#      2022.01.14 Updated according new template.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty-lib-ALL CXX)

option(NETTY__BUILD_STRICT "Build with strict policies: C++ standard required, C++ extension is OFF etc" ON)
option(NETTY__BUILD_TESTS "Build tests" OFF)
option(NETTY__BUILD_DEMO "Build examples/demo" OFF)
option(NETTY__ENABLE_COVERAGE "Build tests with coverage support" OFF)
option(NETTY__ENABLE_PROFILER "Build with profiling support" OFF)

if (NOT PORTABLE_TARGET__ENABLED)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/3rdparty/portable-target/cmake/v2)
    include(PortableTarget)
    portable_target(SET ENABLE_TRACE ON)
endif()

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

if (NETTY__BUILD_STRICT)
    set(CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++XX rather than -std=gnu++XX
endif()

include(${CMAKE_CURRENT_LIST_DIR}/library.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/library-p2p.cmake)

if (NETTY__BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if (NETTY__BUILD_DEMO AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/demo)
    add_subdirectory(demo)
endif()

if (NETTY__ENABLE_PROFILER)
    message(STATUS "Profiler feature enabled")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "-pg")
        set(CMAKE_C_FLAGS "-pg")
    else()
        message(FATAL_ERROR "Profiler feature requires GCC")
    endif()
endif()
