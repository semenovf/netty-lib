################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.18 Initial version
################################################################################
project(mtu)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE pfs::net)
