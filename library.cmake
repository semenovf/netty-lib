################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2022.01.14 Refactored for use `portable_target`.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty-lib CXX)

# option(NETTY__ENABLE_NANOMSG "Enable NNG (network backend) library" OFF)
option(NETTY__STATIC_ONLY "Build static only" OFF)

if (NOT TARGET pfs::common)
    portable_target(INCLUDE_PROJECT
        ${CMAKE_CURRENT_LIST_DIR}/3rdparty/pfs/common/library.cmake)
endif()

if (NETTY__STATIC_ONLY)
    portable_target(LIBRARY ${PROJECT_NAME} STATIC ALIAS pfs::netty)
else()
    portable_target(LIBRARY ${PROJECT_NAME} ALIAS pfs::netty)
endif()

portable_target(SOURCES ${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp)

if (UNIX)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_linux.cpp)
elseif (MSVC)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_win32.cpp)
else()
    message (FATAL_ERROR "Unsupported platform")
endif()

portable_target(INCLUDE_DIRS ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::common)

if (MSVC)
    portable_target(LINK ${PROJECT_NAME} PUBLIC Ws2_32 Iphlpapi)
endif(MSVC)

portable_target(EXPORTS ${PROJECT_NAME} NETTY__EXPORTS NETTY__STATIC)

# if (NETTY__ENABLE_NANOMSG)
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
# endif(NETTY__ENABLE_NANOMSG)

# if (NETTY__ENABLE_LIBZMQ)
#     set(WITH_PERF_TOOL OFF CACHE BOOL "")
#     set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
#     set(ENABLE_CPACK OFF CACHE BOOL "")
#     add_subdirectory(3rdparty/libzmq)
#
#     set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "")
#     add_subdirectory(3rdparty/cppzmq)
# endif(NETTY__ENABLE_LIBZMQ)
