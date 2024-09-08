################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.11 Initial version.
#      2024.09.08 Initial version.
################################################################################
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/enet 3rdparty/enet)

if (TARGET enet)
    target_include_directories(enet PUBLIC ${CMAKE_CURRENT_LIST_DIR}/enet/include)

    # This link can be removed with new ENet release (> 1.3.18)
    if (MSVC)
        cmake_policy(SET CMP0079 NEW)
        target_link_libraries(enet PRIVATE winmm ws2_32)
    endif()
endif()
