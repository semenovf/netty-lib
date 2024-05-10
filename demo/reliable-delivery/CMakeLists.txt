################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.01 Initial version.
################################################################################
project(reliable-delivery-demo)

add_executable(${PROJECT_NAME} main.cpp
    ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.hpp)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/sample")

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

if (TARGET pfs::debby)
    target_link_libraries(${PROJECT_NAME} pfs::debby)
else()
    target_link_libraries(${PROJECT_NAME} pfs::debby::static)
endif()
