################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2023.01.17 Initial version.
################################################################################
project(discovery-demo)

add_executable(${PROJECT_NAME} main.cpp)

if (TARGET pfs::netty::p2p)
    target_link_libraries(${PROJECT_NAME} pfs::netty::p2p pfs::common)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::p2p::static pfs::common)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC PFS__LOG_LEVEL=2)
