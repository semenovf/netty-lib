################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.04.12 Initial version
################################################################################

if (NOT TARGET pfs::common)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/common/.git)
        add_subdirectory(common)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_COMMON ON)
        message(STATUS "Fetching common ...")
        include(FetchContent)
        FetchContent_Declare(common
            GIT_REPOSITORY https://github.com/semenovf/common-lib.git
            GIT_TAG master
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
            # SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/2ndparty/common)
        FetchContent_MakeAvailable(common)
        message(STATUS "Fetching common complete")
    endif()
endif()

if (NOT TARGET pfs::ionik)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/ionik/.git)
        add_subdirectory(ionik)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_IONIK ON)
        message(STATUS "Fetching pfs::ionik ...")
        include(FetchContent)
        FetchContent_Declare(ionik
            GIT_REPOSITORY https://github.com/semenovf/ionik-lib.git
            GIT_TAG master
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ionik)
        FetchContent_MakeAvailable(ionik)
        message(STATUS "Fetching pfs::ionik complete")
    endif()
endif()

if ((NETTY__BUILD_TESTS OR NETTY__BUILD_DEMO) AND NOT TARGET pfs::lorem)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/lorem/.git)
        add_subdirectory(lorem)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_LOREM ON)
        message(STATUS "Fetching pfs::lorem ...")
        include(FetchContent)
        FetchContent_Declare(lorem
            GIT_REPOSITORY https://github.com/semenovf/lorem-lib.git
            GIT_TAG master
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lorem)
        FetchContent_MakeAvailable(lorem)
        message(STATUS "Fetching pfs::lorem complete")
    endif()
endif()
