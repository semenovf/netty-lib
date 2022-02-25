################################################################################
# Copyright (c) 2022 Vladislav Trifochkin
#
# Changelog:
#      2022.02.24 Initial version.
################################################################################
cmake_minimum_required (VERSION 3.11)

set(CEREAL_THREAD_SAFE "0" CACHE STRING "Disable thread safety for Cereal")

if (NOT NETTY_P2P__CEREAL_ROOT)
    set(NETTY_P2P__CEREAL_ROOT ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/cereal CACHE STRING "")
endif()

if (NOT TARGET cereal)
    add_library(cereal INTERFACE)

    # Use mutexes to ensure thread safety
    target_compile_definitions(cereal INTERFACE "CEREAL_THREAD_SAFE=${CEREAL_THREAD_SAFE}")
    target_include_directories(cereal INTERFACE ${NETTY_P2P__CEREAL_ROOT}/include)
endif()

