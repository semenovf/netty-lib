################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2023.01.01 Initial version.
################################################################################
project(udp-demo)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} pfs::netty::static)
target_compile_definitions(${PROJECT_NAME} PUBLIC PFS__LOG_LEVEL=2)
