################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2023.05.11 Initial version.
################################################################################
project(enet-basic-demo
    VERSION 1.0.0
    DESCRIPTION "Basic ENet demo"
    HOMEPAGE_URL "https://github.com/semenovf/netty-lib"
    LANGUAGES CXX C)

if (TARGET enet)
    add_executable(${PROJECT_NAME} main.cpp linenoise/linenoise.c linenoise/linenoise.h)

    if (TARGET pfs::netty)
        target_link_libraries(${PROJECT_NAME} pfs::netty)
    else()
        target_link_libraries(${PROJECT_NAME} pfs::netty::static)
    endif()

    target_link_libraries(${PROJECT_NAME} enet)

    target_compile_definitions(${PROJECT_NAME} PUBLIC PFS__LOG_LEVEL=2)
endif()
