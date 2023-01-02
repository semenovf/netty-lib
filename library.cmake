################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2022.01.14 Refactored for use `portable_target`.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty CXX)

include(CheckIncludeFile)

portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty
    EXPORTS NETTY__EXPORTS
    BIND_STATIC ${PROJECT_NAME}-static
    STATIC_ALIAS pfs::netty::static
    STATIC_EXPORTS NETTY__STATIC)

portable_target(SOURCES ${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp)

if (UNIX)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_linux.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/inet_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_server.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_socket.cpp)
elseif (MSVC)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_win32.cpp)
else()
    message (FATAL_ERROR "Unsupported platform")
endif()

if (NOT TARGET pfs::common)
    portable_target(INCLUDE_PROJECT
        ${CMAKE_CURRENT_LIST_DIR}/3rdparty/pfs/common/library.cmake)
endif()

portable_target(INCLUDE_DIRS ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::common)
portable_target(LINK ${PROJECT_NAME}-static PUBLIC pfs::common)

if (MSVC)
    portable_target(COMPILE_OPTIONS ${PROJECT_NAME} "/wd4251")
    portable_target(COMPILE_OPTIONS ${PROJECT_NAME}-static "/wd4251")
    portable_target(LINK ${PROJECT_NAME} PRIVATE Ws2_32 Iphlpapi)
    portable_target(LINK ${PROJECT_NAME}-static PRIVATE Ws2_32 Iphlpapi)
endif(MSVC)

if (UNIX)
    CHECK_INCLUDE_FILE("sys/epoll.h" __has_sys_epoll)

    if (__has_sys_epoll)
        portable_target(SOURCES ${PROJECT_NAME}
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/epoll_poller.cpp)
        portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__EPOLL_ENABLED=1")
        portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__EPOLL_ENABLED=1")
    endif()
endif()
