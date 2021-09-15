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
project(net-lib CXX)

list(APPEND SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp)

if (UNIX)
    list(APPEND SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_linux.cpp)
elseif (MSVC)
    list(APPEND SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_win32.cpp)
else()
    message (FATAL_ERROR "Unsupported platform")
endif()

# Make object files for STATIC and SHARED targets
add_library(${PROJECT_NAME}_OBJLIB OBJECT ${SOURCES})

add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_OBJLIB>)
add_library(${PROJECT_NAME}-static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_OBJLIB>)
add_library(pfs::net ALIAS ${PROJECT_NAME})
add_library(pfs::net::static ALIAS ${PROJECT_NAME}-static)

target_include_directories(${PROJECT_NAME}_OBJLIB PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJECT_NAME}-static PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# Shared libraries need PIC
# For SHARED and MODULE libraries the POSITION_INDEPENDENT_CODE target property
# is set to ON automatically, but need for OBJECT type
set_property(TARGET ${PROJECT_NAME}_OBJLIB PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME}_OBJLIB PRIVATE pfs::common)
target_link_libraries(${PROJECT_NAME} PUBLIC pfs::common)
target_link_libraries(${PROJECT_NAME}-static PUBLIC pfs::common)

if (MSVC)
    # Important! For compatiblity between STATIC and SHARED libraries
    target_compile_definitions(${PROJECT_NAME}_OBJLIB PRIVATE -DPFS_NET_DLL_EXPORTS)

    target_compile_definitions(${PROJECT_NAME} PUBLIC -DPFS_NET_DLL_EXPORTS -DUNICODE -D_UNICODE)
    target_compile_definitions(${PROJECT_NAME}-static PUBLIC -DPFS_NET_STATIC_LIB -DUNICODE -D_UNICODE)
    target_link_libraries(${PROJECT_NAME} PRIVATE Ws2_32 Iphlpapi)
    target_link_libraries(${PROJECT_NAME}-static PRIVATE Ws2_32 Iphlpapi)
endif(MSVC)
