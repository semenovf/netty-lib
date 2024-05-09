################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.05 Initial version.
################################################################################
project(service-demo
    VERSION 1.0.0
    DESCRIPTION "Service demo"
    HOMEPAGE_URL "https://github.com/semenovf/netty-lib"
    LANGUAGES CXX C)

add_executable(${PROJECT_NAME} main.cpp
    envelope.hpp
    message.hpp
    serializer.hpp
    service.hpp
    linenoise/linenoise.c linenoise/linenoise.h)

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::static)
endif()
