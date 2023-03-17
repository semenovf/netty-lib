################################################################################
# Copyright (c) 2021-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2023.01.17 Version 2.
#      2023.02.10 Separated static and shared builds.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty-p2p CXX)

option(NETTY_P2P__BUILD_SHARED "Enable build shared library" OFF)
option(NETTY_P2P__BUILD_STATIC "Enable build static library" ON)
option(NETTY_P2P__ENABLE_CEREAL "Enable cereal library (serialization backend)" ON)
option(NETTY_P2P__ENABLE_CEREAL_THREAD_SAFETY "Enable cereal library thread safety" OFF)

if (NOT PORTABLE_TARGET__CURRENT_PROJECT_DIR)
    set(PORTABLE_TARGET__CURRENT_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if (NETTY_P2P__BUILD_SHARED)
    portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty::p2p EXPORTS NETTY__EXPORTS)
endif()

if (NETTY_P2P__BUILD_STATIC)
    set(STATIC_PROJECT_NAME ${PROJECT_NAME}-static)
    portable_target(ADD_STATIC ${STATIC_PROJECT_NAME} ALIAS pfs::netty::p2p::static EXPORTS NETTY__STATIC)
endif()

if (NETTY_P2P__ENABLE_CEREAL)
    if (NOT TARGET cereal)
        portable_target(INCLUDE_PROJECT ${CMAKE_CURRENT_LIST_DIR}/cmake/Cereal.cmake)
    endif()

    if (NETTY_P2P__BUILD_SHARED)
        portable_target(LINK ${PROJECT_NAME} PUBLIC cereal)
    endif()

    if (NETTY_P2P__BUILD_STATIC)
        portable_target(LINK ${STATIC_PROJECT_NAME} PUBLIC cereal)
    endif()
endif(NETTY_P2P__ENABLE_CEREAL)

list(APPEND _netty_p2p__sources
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/file.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/file_transporter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/posix/discovery_engine.cpp)

if (NETTY_P2P__BUILD_SHARED)
    portable_target(SOURCES ${PROJECT_NAME} ${_netty_p2p__sources})
    portable_target(INCLUDE_DIRS ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

    if (TARGET pfs::netty)
        portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::netty)
    elseif (TARGET pfs::netty::static)
        portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::netty::static)
    endif()
endif()

if (NETTY_P2P__BUILD_STATIC)
    portable_target(SOURCES ${STATIC_PROJECT_NAME} ${_netty_p2p__sources})
    portable_target(INCLUDE_DIRS ${STATIC_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

    if (TARGET pfs::netty::static)
        portable_target(LINK ${STATIC_PROJECT_NAME} PUBLIC pfs::netty::static)
    elseif (TARGET pfs::netty)
        portable_target(LINK ${STATIC_PROJECT_NAME} PUBLIC pfs::netty)
    endif()
endif()
