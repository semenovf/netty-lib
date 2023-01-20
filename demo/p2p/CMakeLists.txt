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
target_link_libraries(${PROJECT_NAME} pfs::netty::p2p pfs::netty)
target_compile_definitions(${PROJECT_NAME} PUBLIC PFS__LOG_LEVEL=3)
