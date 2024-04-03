################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2023.03.21 Initial version.
################################################################################
project(rpc-demo)

add_executable(${PROJECT_NAME} main.cpp)

if (TARGET)
    target_link_libraries(${PROJECT_NAME} pfs::netty_p2p)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty_p2p::static)
endif()

target_link_libraries(${PROJECT_NAME} pfs::common)

target_compile_definitions(${PROJECT_NAME} PUBLIC PFS__LOG_LEVEL=2)
