################################################################################
# Copyright (c) 2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.04.04 Initial version.
################################################################################
project(available_net_interfaces)

add_executable(${PROJECT_NAME} main.cpp)

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE pfs::netty::static)
endif()
