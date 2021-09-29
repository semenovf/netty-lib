################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version
################################################################################
cmake_minimum_required (VERSION 3.5)
project(net-lib-ALL CXX)

option(BUILD_STRICT "Build with strict policies: C++ standard required, C++ extension is OFF etc" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_DEMO "Build examples/demo" ON)
option(ENABLE_COVERAGE "Build tests with coverage support" OFF)
# option(ENABLE_NANOMSG "Enable NNG (network backend) library" OFF)
# option(ENABLE_LIBZMQ "Enable ZeroMQ library (network backend)" OFF)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

if (BUILD_STRICT)
    set(CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++XX rather than -std=gnu++XX
endif()

# if (ENABLE_NANOMSG)
#     set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared library")
#     set(NNG_TESTS OFF CACHE BOOL "Build and run tests.")
#     set(NNG_TOOLS OFF CACHE BOOL "Build extra tools.")
#     set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "Enable building nngcat utility.")
#     set(NNG_ENABLE_COVERAGE OFF CACHE BOOL "Enable coverage reporting.")
#     set(NNG_ELIDE_DEPRECATED OFF CACHE BOOL "Elide deprecated functionality.")
#     set(NNG_ENABLE_STATS OFF CACHE BOOL "Enable statistics.")
#
#     # Protocols.
#     set(NNG_PROTO_BUS0 ON CACHE BOOL "Enable BUSv0 protocol.")
#     set(NNG_PROTO_PAIR0 ON CACHE BOOL "Enable PAIRv0 protocol.")
#     set(NNG_PROTO_PAIR1 ON CACHE BOOL "Enable PAIRv1 protocol.")
#     set(NNG_PROTO_PUSH0 ON CACHE BOOL "Enable PUSHv0 protocol.")
#     set(NNG_PROTO_PULL0 ON CACHE BOOL "Enable PULLv0 protocol.")
#     set(NNG_PROTO_PUB0 ON CACHE BOOL "Enable PUBv0 protocol.")
#     set(NNG_PROTO_SUB0 ON CACHE BOOL "Enable SUBv0 protocol.")
#     set(NNG_PROTO_REQ0 ON CACHE BOOL "Enable REQv0 protocol.")
#     set(NNG_PROTO_REP0 ON CACHE BOOL "Enable REPv0 protocol.")
#     set(NNG_PROTO_RESPONDENT0 ON CACHE BOOL "Enable RESPONDENTv0 protocol.")
#     set(NNG_PROTO_SURVEYOR0 ON CACHE BOOL "Enable SURVEYORv0 protocol.")
#
#     # TLS support.
#     # Enabling TLS is required to enable support for the TLS transport
#     # and WSS. It does require a 3rd party TLS engine to be selected.
#     set(NNG_ENABLE_TLS OFF CACHE BOOL "Enable TLS support.")
#
#     # HTTP API support.
#     set(NNG_ENABLE_HTTP OFF CACHE BOOL "Enable HTTP API.")
#
#     # Transport Options.
#     set(NNG_TRANSPORT_INPROC OFF CACHE BOOL "Enable inproc transport.")
#     set(NNG_TRANSPORT_IPC OFF CACHE BOOL "Enable IPC transport.")
#     set(NNG_TRANSPORT_TCP ON CACHE BOOL "Enable TCP transport.")
#     set(NNG_TRANSPORT_TLS OFF CACHE BOOL "Enable TLS transport.")
#     set(NNG_TRANSPORT_WS OFF CACHE BOOL "Enable WebSocket transport.")
#     set(NNG_TRANSPORT_ZEROTIER OFF CACHE BOOL "Enable ZeroTier transport (requires libzerotiercore).")
#
#     add_subdirectory(3rdparty/nng)
# endif(ENABLE_NANOMSG)

# if (ENABLE_LIBZMQ)
#     set(WITH_PERF_TOOL OFF CACHE BOOL "")
#     set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
#     set(ENABLE_CPACK OFF CACHE BOOL "")
#     add_subdirectory(3rdparty/libzmq)
#
#     set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "")
#     add_subdirectory(3rdparty/cppzmq)
# endif(ENABLE_LIBZMQ)

include(${CMAKE_CURRENT_LIST_DIR}/3rdparty/pfs/common/library.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/library.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/library-p2p.cmake)

if (BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_DEMO)
    add_subdirectory(demo)
endif()
