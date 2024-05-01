################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2023.01.17 Version 2.
#      2023.02.10 Separated static and shared builds.
#      2024.04.03 Replaced the sequence of two target configurations with a foreach statement.
#      2024.05.01 Removed cereal dependency.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty_p2p CXX)

option(NETTY_P2P__BUILD_SHARED "Enable build shared library" OFF)
option(NETTY_P2P__BUILD_STATIC "Enable build static library" ON)

if (NOT PORTABLE_TARGET__CURRENT_PROJECT_DIR)
    set(PORTABLE_TARGET__CURRENT_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if (NETTY_P2P__BUILD_SHARED)
    portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty_p2p EXPORTS NETTY__EXPORTS)
    list(APPEND _netty_p2p__targets ${PROJECT_NAME})
endif()

if (NETTY_P2P__BUILD_STATIC)
    set(STATIC_PROJECT_NAME ${PROJECT_NAME}-static)
    portable_target(ADD_STATIC ${STATIC_PROJECT_NAME} ALIAS pfs::netty_p2p::static EXPORTS NETTY__STATIC)
    list(APPEND _netty_p2p__targets ${STATIC_PROJECT_NAME})
endif()

list(APPEND _netty_p2p__sources
    # ${CMAKE_CURRENT_LIST_DIR}/src/p2p/remote_file_protocol.cpp
    # ${CMAKE_CURRENT_LIST_DIR}/src/p2p/remote_file_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/posix/discovery_engine.cpp)

if (NOT TARGET pfs::ionik AND NOT TARGET pfs::ionik::static)
    if (NETTY_P2P__BUILD_SHARED)
        set(IONIK__BUILD_SHARED ON CACHE BOOL "Build `ionik` shared library")
    endif()

    if (NETTY_P2P__BUILD_STATIC)
        set(IONIK__BUILD_STATIC ON CACHE BOOL "Build `ionik` static library")
    endif()

    portable_target(INCLUDE_PROJECT ${CMAKE_CURRENT_LIST_DIR}/2ndparty/ionik/library.cmake)
endif()

foreach(_target IN LISTS _netty_p2p__targets)
    portable_target(SOURCES ${_target} ${_netty_p2p__sources})
    portable_target(INCLUDE_DIRS ${_target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

    if (_netty_p2p__definitions)
        portable_target(DEFINITIONS ${_target} PUBLIC ${_netty_p2p__definitions})
    endif()

    if (_netty_p2p__links)
        portable_target(LINK ${_target} PUBLIC ${_netty_p2p__links})
    endif()

    if (TARGET pfs::netty)
        portable_target(LINK ${_target} PUBLIC pfs::netty)
    elseif (TARGET pfs::netty::static)
        portable_target(LINK ${_target} PUBLIC pfs::netty::static)
    endif()

    if (TARGET pfs::ionik)
        portable_target(LINK ${_target} PUBLIC pfs::ionik)
    elseif (TARGET pfs::ionik::static)
        portable_target(LINK ${_target} PUBLIC pfs::ionik::static)
    endif()
endforeach()
