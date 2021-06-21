################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version
################################################################################
cmake_minimum_required (VERSION 3.5)
project(net-lib CXX C)

if (UNIX)
    list(APPEND SOURCES src/mtu_linux.cpp)
elseif(MSVC)
else()
endif()

add_library(${PROJECT_NAME} ${SOURCES})
add_library(pfs::net ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

if (MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DDLL_EXPORTS)
endif(MSVC)
