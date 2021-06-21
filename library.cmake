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

if (UNIX)
    # Avoid ld error:
    # ------------------------------------------------------------------------------------------
    # relocation R_X86_64_PC32 against symbol `_ZGVZN3pfs3net18get_error_categoryEvE8instance'
    # can not be used when making a shared object; recompile with -fPIC
    # ------------------------------------------------------------------------------------------
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif(UNIX)
