################################################################################
# Copyright (c) 2019-2021 Vladislav Trifochkin
#
# This file is part of [io-lib](https://github.com/semenovf/io-lib) library.
#
# Changelog:
#      2021.10.08 Initial version.
################################################################################
project(handshake)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} pfs::net::p2p pfs::net)
target_compile_definitions(${PROJECT_NAME} PUBLIC "-DPFS_NET_P2P_TRACE_LEVEL=3")
