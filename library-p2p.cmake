################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
################################################################################
cmake_minimum_required (VERSION 3.5)
project(net-p2p-lib CXX)

option(ENABLE_QT5 "Enable Qt5 library (network backend)" ON)
option(ENABLE_QT6 "Enable Qt6 library (network backend)" OFF)
option(ENABLE_CEREAL "Enable cereal library (serialization backend)" ON)
option(ENABLE_CEREAL_THREAD_SAFETY "Enable cereal library thread safety" OFF)

if (ENABLE_QT5)
    find_package(Qt5 COMPONENTS Core Network REQUIRED)

    set(QT5_CORE_ENABLED ON CACHE BOOL "Qt5 Core enabled")
    set(QT5_NETWORK_ENABLED ON CACHE BOOL "Qt5 Network enabled")
endif(ENABLE_QT5)

if (ENABLE_CEREAL)
    set(CEREAL_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/cereal)
    add_library(cereal INTERFACE)

    # Use mutexes to ensure thread safety
    if (ENABLE_CEREAL_THREAD_SAFETY)
        target_compile_definitions(cereal PUBLIC "-DCEREAL_THREAD_SAFE=1")
    endif(ENABLE_CEREAL_THREAD_SAFETY)

    target_include_directories(cereal INTERFACE ${CEREAL_ROOT}/include)
    set(CEREAL_ENABLED ON CACHE BOOL "Cereal serialization library enabled")
endif(ENABLE_CEREAL)

list(APPEND _link_libraries pfs::net)

if (CEREAL_ENABLED)
    list(APPEND _compile_definitions "-DCEREAL_ENABLED=1")
    list(APPEND _link_libraries cereal)
endif()

if (QT5_CORE_ENABLED)
    list(APPEND _compile_definitions "-DQT5_CORE_ENABLED=1")
    list(APPEND _link_libraries Qt5::Core)
endif(QT5_CORE_ENABLED)

if (QT5_NETWORK_ENABLED)
    list(APPEND _compile_definitions "-DQT5_NETWORK_ENABLED=1")
    list(APPEND _link_libraries Qt5::Network)
endif(QT5_NETWORK_ENABLED)

# Make object files for STATIC and SHARED targets
add_library(${PROJECT_NAME}_OBJLIB OBJECT ${SOURCES})

add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_OBJLIB>)
add_library(${PROJECT_NAME}-static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_OBJLIB>)
add_library(pfs::net::p2p ALIAS ${PROJECT_NAME})
add_library(pfs::net::p2p::static ALIAS ${PROJECT_NAME}-static)

target_include_directories(${PROJECT_NAME}_OBJLIB PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJECT_NAME}-static PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# Shared libraries need PIC
# For SHARED and MODULE libraries the POSITION_INDEPENDENT_CODE target property
# is set to ON automatically, but need for OBJECT type
set_property(TARGET ${PROJECT_NAME}_OBJLIB PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME}_OBJLIB PRIVATE ${_link_libraries})
target_link_libraries(${PROJECT_NAME} PUBLIC ${_link_libraries})
target_link_libraries(${PROJECT_NAME}-static PUBLIC ${_link_libraries})

target_compile_definitions(${PROJECT_NAME}_OBJLIB PRIVATE ${_compile_definitions})
target_compile_definitions(${PROJECT_NAME} PUBLIC ${_compile_definitions})
target_compile_definitions(${PROJECT_NAME}-static PUBLIC ${_compile_definitions})

if (MSVC)
    # Important! For compatiblity between STATIC and SHARED libraries
    target_compile_definitions(${PROJECT_NAME}_OBJLIB PRIVATE -DPFS_NET_LIB_EXPORTS)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DPFS_NET_LIB_EXPORTS)
    target_compile_definitions(${PROJECT_NAME}-static PUBLIC -DPFS_NET_LIB_STATIC)
endif(MSVC)
