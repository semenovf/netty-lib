################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.04.12 Initial version
################################################################################

if (NOT TARGET pfs::common)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS common/.git)
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
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ionik/.git)
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

if (NETTY__BUILD_TESTS)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_SOURCE_DIR}/2ndparty/lorem/.git)
        add_subdirectory(${CMAKE_SOURCE_DIR}/2ndparty/lorem 2ndparty/lorem)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_LOREM ON)
        message(STATUS "Fetching lorem ...")
        include(FetchContent)
        FetchContent_Declare(lorem
            GIT_REPOSITORY https://github.com/semenovf/lorem-lib.git
            GIT_TAG master
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/2ndparty/lorem
            SUBBUILD_DIR ${CMAKE_BINARY_DIR}/2ndparty/lorem)
        FetchContent_MakeAvailable(lorem)
        message(STATUS "Fetching lorem complete")
    endif()
endif()
