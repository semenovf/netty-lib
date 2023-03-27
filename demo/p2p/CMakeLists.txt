################################################################################
# Copyright (c) 2019-2021 Vladislav Trifochkin
#
# This file is part of [io-lib](https://github.com/semenovf/io-lib) library.
#
# Changelog:
#      2021.09.10 Initial version.
#      2021.11.01 New version.
################################################################################
project(p2p-demo)

add_executable(${PROJECT_NAME} main.cpp)

if (TARGET pfs::netty::p2p)
    target_link_libraries(${PROJECT_NAME} pfs::netty::p2p)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::p2p::static)
endif()

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::static)
endif()
