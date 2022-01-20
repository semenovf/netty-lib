################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
################################################################################
cmake_minimum_required (VERSION 3.5)
project(netty-p2p-lib CXX)

option(NETTY_P2P__STATIC_ONLY "Build static only" OFF)
option(NETTY_P2P__ENABLE_QT5 "Enable Qt5 library (network backend)" OFF)
option(NETTY_P2P__ENABLE_QT6 "Enable Qt6 library (network backend)" OFF)
option(NETTY_P2P__ENABLE_UDT "Enable UDT library (reliable UDP implementation)" OFF)
option(NETTY_P2P__ENABLE_NEW_UDT "Enable modified UDT library (reliable UDP implementation)" ON)
option(NETTY_P2P__ENABLE_CEREAL "Enable cereal library (serialization backend)" ON)
option(NETTY_P2P__ENABLE_CEREAL_THREAD_SAFETY "Enable cereal library thread safety" OFF)

if (NETTY_P2P__STATIC_ONLY)
    portable_target(LIBRARY ${PROJECT_NAME} STATIC ALIAS pfs::netty::p2p)
else()
    portable_target(LIBRARY ${PROJECT_NAME} ALIAS pfs::netty::p2p)
endif()

if (NETTY_P2P__ENABLE_QT5)
    find_package(Qt5 COMPONENTS Core Network REQUIRED)

    portable_target(LINK_QT5_COMPONENTS ${PROJECT_NAME} PRIVATE Core Network)
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__QT5_CORE_ENABLED=1")
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__QT5_NETWORK_ENABLED=1")
endif(NETTY_P2P__ENABLE_QT5)

if (NETTY_P2P__ENABLE_UDT)
    set(NETTY_P2P__UDT_ENABLED ON CACHE BOOL "UDT enabled")
    set(NETTY_P2P__UDT_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/udt/lib")
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__UDT_ENABLED=1")
endif(NETTY_P2P__ENABLE_UDT)

if (NETTY_P2P__ENABLE_NEW_UDT)
    set(NETTY_P2P__UDT_ENABLED ON CACHE BOOL "modified UDT enabled")
    set(NETTY_P2P__UDT_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/udt/newlib")
    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__NEW_UDT_ENABLED=1")
endif(NETTY_P2P__ENABLE_NEW_UDT)

if (NETTY_P2P__ENABLE_CEREAL)
    set(NETTY_P2P__CEREAL_ROOT ${CMAKE_CURRENT_LIST_DIR}/3rdparty/cereal)
    add_library(cereal INTERFACE)

    # Use mutexes to ensure thread safety
    if (NETTY_P2P__ENABLE_CEREAL_THREAD_SAFETY)
        target_compile_definitions(cereal PUBLIC "CEREAL_THREAD_SAFE=1")
    endif(NETTY_P2P__ENABLE_CEREAL_THREAD_SAFETY)

    target_include_directories(cereal INTERFACE ${NETTY_P2P__CEREAL_ROOT}/include)

    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__CEREAL_ENABLED=1")
    portable_target(LINK ${PROJECT_NAME} PUBLIC cereal)
endif(NETTY_P2P__ENABLE_CEREAL)

portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::netty)

if (NETTY_P2P__UDT_ENABLED OR NETTY_P2P__ENABLE_NEW_UDT)
    # UDT sources
    portable_target(SOURCES ${PROJECT_NAME}
        ${NETTY_P2P__UDT_ROOT}/api.cpp
        ${NETTY_P2P__UDT_ROOT}/buffer.cpp
        ${NETTY_P2P__UDT_ROOT}/cache.cpp
        ${NETTY_P2P__UDT_ROOT}/ccc.cpp
        ${NETTY_P2P__UDT_ROOT}/channel.cpp
        ${NETTY_P2P__UDT_ROOT}/common.cpp
        ${NETTY_P2P__UDT_ROOT}/core.cpp
        ${NETTY_P2P__UDT_ROOT}/epoll.cpp
        ${NETTY_P2P__UDT_ROOT}/list.cpp
        ${NETTY_P2P__UDT_ROOT}/md5.cpp
        ${NETTY_P2P__UDT_ROOT}/packet.cpp
        ${NETTY_P2P__UDT_ROOT}/queue.cpp
        ${NETTY_P2P__UDT_ROOT}/window.cpp)

    portable_target(SOURCES ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udp_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/debug_CCC.cpp)

    portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC "NETTY_P2P__RELIABLE_TRANSPORT_ENABLED=1")
endif()

portable_target(EXPORTS ${PROJECT_NAME} NETTY__EXPORTS NETTY__STATIC)
