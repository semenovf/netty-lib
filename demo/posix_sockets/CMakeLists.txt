################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2023.01.01 Initial version.
################################################################################
project(posix_sockets)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} pfs::netty::static)
