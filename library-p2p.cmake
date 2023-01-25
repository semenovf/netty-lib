################################################################################
# Copyright (c) 2021-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2023.01.17 Version 2.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty-p2p CXX)

option(NETTY_P2P__ENABLE_CEREAL "Enable cereal library (serialization backend)" ON)
option(NETTY_P2P__ENABLE_CEREAL_THREAD_SAFETY "Enable cereal library thread safety" OFF)

portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty::p2p
    EXPORTS NETTY__EXPORTS
    BIND_STATIC ${PROJECT_NAME}-static
    STATIC_ALIAS pfs::netty::p2p::static
    STATIC_EXPORTS NETTY__STATIC)

portable_target(INCLUDE_DIRS ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
portable_target(LINK ${PROJECT_NAME} PUBLIC pfs::netty)
portable_target(LINK ${PROJECT_NAME}-static PRIVATE pfs::netty::static)

if (NETTY_P2P__ENABLE_CEREAL)
    if (NOT TARGET cereal)
        portable_target(INCLUDE_PROJECT ${CMAKE_CURRENT_LIST_DIR}/cmake/Cereal.cmake)
    endif()

    portable_target(LINK ${PROJECT_NAME} PUBLIC cereal)
    portable_target(LINK ${PROJECT_NAME}-static PUBLIC cereal)
endif(NETTY_P2P__ENABLE_CEREAL)

portable_target(SOURCES ${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/file_transporter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/p2p/posix/discovery_engine.cpp)

#if (MSVC)
    #portable_target(COMPILE_OPTIONS ${PROJECT_NAME} "/wd4251")
    #portable_target(COMPILE_OPTIONS ${PROJECT_NAME}-static "/wd4251")

    #portable_target(COMPILE_OPTIONS ${PROJECT_NAME} "/wd26812")
    #portable_target(COMPILE_OPTIONS ${PROJECT_NAME}-static "/wd26812")
#endif(MSVC)

portable_target(DEFINITIONS ${PROJECT_NAME} PRIVATE PFS__LOG_LEVEL=2)
portable_target(DEFINITIONS ${PROJECT_NAME}-static PRIVATE PFS__LOG_LEVEL=2)

