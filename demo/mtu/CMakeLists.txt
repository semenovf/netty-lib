################################################################################
# Copyright (c) 2019-2022 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.18 Initial version
################################################################################
project(mtu)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty)
