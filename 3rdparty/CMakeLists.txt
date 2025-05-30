################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.04.12 Initial version
################################################################################

if (NETTY__ENABLE_ENET)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS enet/.git)
        add_subdirectory(enet 3rdparty/enet)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_ENET ON)
        message(STATUS "Fetching ENet ...")
        include(FetchContent)
        FetchContent_Declare(enet
            GIT_REPOSITORY https://github.com/lsalzman/enet.git
            GIT_TAG "v1.3.18"
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/enet)
            # SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/enet
        FetchContent_MakeAvailable(enet)
        message(STATUS "Fetching ENet complete")
    endif()

    target_include_directories(enet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/enet/include)

    # This link can be removed with new ENet release (> 1.3.18)
    if (MSVC)
        cmake_policy(SET CMP0079 NEW)
        target_link_libraries(enet PRIVATE winmm ws2_32)
    endif()
endif(NETTY__ENABLE_ENET)
