################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
################################################################################
cmake_minimum_required (VERSION 3.11)
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

list(APPEND _link_libraries pfs::common Threads::Threads)

if (MSVC)
    list(APPEND _compile_definitions -DUNICODE -D_UNICODE)
    list(APPEND _link_libraries Ws2_32 Iphlpapi)
endif(MSVC)

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
