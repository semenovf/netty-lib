################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.05 Initial version.
################################################################################
project(service-demo
    VERSION 1.0.0
    DESCRIPTION "Service demo"
    HOMEPAGE_URL "https://github.com/semenovf/netty-lib"
    LANGUAGES CXX C)

list(APPEND _sources main.cpp
    envelope.hpp
    message.hpp
    serializer.hpp
    service.hpp)

if (MSVC)
    list(APPEND _sources
        ../shared/linenoise-ng/linenoise.cpp 
        ../shared/linenoise-ng/linenoise.h
        ../shared/linenoise-ng/ConvertUTF.cpp
        ../shared/linenoise-ng/ConvertUTF.h
        ../shared/linenoise-ng/wcwidth.cpp)
else()
    list(APPEND _sources
        ../shared/linenoise/linenoise.c 
        ../shared/linenoise/linenoise.h)
endif()

add_executable(${PROJECT_NAME} ${_sources})

if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/../shared/linenoise-ng)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/../shared/linenoise)
endif()

if (TARGET pfs::netty)
    target_link_libraries(${PROJECT_NAME} pfs::netty)
else()
    target_link_libraries(${PROJECT_NAME} pfs::netty::static)
endif()
