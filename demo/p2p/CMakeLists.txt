################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.09.10 Initial version.
#      2023.01.18 New version.
################################################################################
project(p2p-demo)

add_executable(${PROJECT_NAME} main.cpp)

if (TARGET pfs::netty_p2p)
    target_link_libraries(${PROJECT_NAME} pfs::netty_p2p)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty_p2p::static)
endif()

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::static)
endif()
