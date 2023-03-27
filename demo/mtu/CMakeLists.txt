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

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty::static)
endif()
