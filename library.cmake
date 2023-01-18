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

option(NETTY__ENABLE_UDT "Enable modified UDT library (reliable UDP implementation)" ON)
option(NETTY__UDT_PATCHED "Enable modified UDT library with patches" ON)
option(NETTY__ENABLE_QT5 "Enable Qt5 library (network backend)" OFF)

portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty
    EXPORTS NETTY__EXPORTS
    BIND_STATIC ${PROJECT_NAME}-static
    STATIC_ALIAS pfs::netty::static
    STATIC_EXPORTS NETTY__STATIC)

portable_target(SOURCES ${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/socket4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/startup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/inet_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_receiver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_sender.cpp)

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

CHECK_INCLUDE_FILE("poll.h" __has_poll)

if (__has_poll)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/regular_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/poll_poller.cpp)
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__POLL_ENABLED=1")
    portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__POLL_ENABLED=1")
endif()

CHECK_INCLUDE_FILE("sys/select.h" __has_sys_select)

if (__has_sys_select)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/regular_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/select_poller.cpp)
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__SELECT_ENABLED=1")
    portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__SELECT_ENABLED=1")
endif()

if (UNIX)
    CHECK_INCLUDE_FILE("sys/epoll.h" __has_sys_epoll)

    if (__has_sys_epoll)
        portable_target(SOURCES ${PROJECT_NAME}
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/listener_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/connecting_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/epoll_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/regular_poller.cpp)
        portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__EPOLL_ENABLED=1")
        portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__EPOLL_ENABLED=1")
    endif()
endif()

if (NETTY__ENABLE_UDT)
    set(NETTY__UDT_ENABLED ON CACHE BOOL "UDT (modified) enabled")
    set(NETTY__UDT_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/udt/newlib")

    # UDT sources
    portable_target(SOURCES ${PROJECT_NAME}
        ${NETTY__UDT_ROOT}/api.cpp
        ${NETTY__UDT_ROOT}/buffer.cpp
        ${NETTY__UDT_ROOT}/cache.cpp
        ${NETTY__UDT_ROOT}/ccc.cpp
        ${NETTY__UDT_ROOT}/channel.cpp
        ${NETTY__UDT_ROOT}/common.cpp
        ${NETTY__UDT_ROOT}/core.cpp
        ${NETTY__UDT_ROOT}/epoll.cpp
        ${NETTY__UDT_ROOT}/list.cpp
        ${NETTY__UDT_ROOT}/md5.cpp
        ${NETTY__UDT_ROOT}/packet.cpp
        ${NETTY__UDT_ROOT}/queue.cpp
        ${NETTY__UDT_ROOT}/window.cpp)

    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/epoll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/regular_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_server.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/debug_CCC.cpp)

    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__UDT_ENABLED=1")
    portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__UDT_ENABLED=1")

    if (NETTY__UDT_PATCHED)
        portable_target(DEFINITIONS ${PROJECT_NAME} PRIVATE "NETTY__UDT_PATCHED=1")
        portable_target(DEFINITIONS ${PROJECT_NAME}-static PRIVATE "NETTY__UDT_PATCHED=1")
    endif(NETTY__UDT_PATCHED)
endif(NETTY__ENABLE_UDT)

if (NETTY_P2P__ENABLE_QT5)
    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_recever.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_sender.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_socket.cpp)

    portable_target(LINK_QT5_COMPONENTS ${PROJECT_NAME} PUBLIC Core Network)
    portable_target(LINK_QT5_COMPONENTS ${PROJECT_NAME}-static Core Network)
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY__QT5_ENABLED=1")
    portable_target(DEFINITIONS ${PROJECT_NAME}-static PUBLIC "NETTY__QT5_ENABLED=1")
endif(NETTY_P2P__ENABLE_QT5)
