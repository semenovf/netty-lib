################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.11 Initial version.
################################################################################
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/enet 3rdparty/enet)

if (TARGET enet)
    target_include_directories(enet PUBLIC ${CMAKE_CURRENT_LIST_DIR}/enet/include)
endif()
