################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2025.01.16 Initial version.
################################################################################
project(meshnet-demo)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty pfs::ionik)
