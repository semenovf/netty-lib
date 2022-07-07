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

option(NETTY__ENABLE_EXCEPTIONS "Enable exceptions" ON)

if (NETTY__ENABLE_EXCEPTIONS)
    set(PFS__ENABLE_EXCEPTIONS ON CACHE INTERNAL "")
endif()

if (NOT TARGET pfs::common)
    portable_target(INCLUDE_PROJECT
        ${CMAKE_CURRENT_LIST_DIR}/3rdparty/pfs/common/library.cmake)
endif()

portable_target(LIBRARY ${PROJECT_NAME} ALIAS pfs::netty)

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
    portable_target(COMPILE_OPTIONS ${PROJECT_NAME} "/wd4251")
    portable_target(LINK ${PROJECT_NAME} PUBLIC Ws2_32 Iphlpapi)
endif(MSVC)

portable_target(EXPORTS ${PROJECT_NAME} NETTY__EXPORTS NETTY__STATIC)
