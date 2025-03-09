################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2021.06.21 Initial version
#       2022.01.14 Updated according new template.
#       2024.04.03 Fixed `portable-target` path.
#       2024.12.07 Up to C++14 standard.
#                  Min CMake version is 3.19 (CMakePresets).
################################################################################
cmake_minimum_required (VERSION 3.19)
project(netty-ALL CXX C)

option(NETTY__BUILD_STRICT "Build with strict policies: C++ standard required, C++ extension is OFF etc" ON)
option(NETTY__BUILD_TESTS "Build tests" OFF)
option(NETTY__BUILD_DEMO "Build examples/demo" OFF)

# Netty library specific options
option(NETTY__BUILD_SHARED "Enable build shared library" OFF)
option(NETTY__ENABLE_P2P "Enabled P2P support" OFF) # FIXME set to ON
option(NETTY__ENABLE_UDT "Enable modified UDT library (reliable UDP implementation)" OFF)
option(NETTY__UDT_PATCHED "Enable modified UDT library with patches" ON)
option(NETTY__ENABLE_ENET "Enable ENet library (reliable UDP implementation)" OFF)
option(NETTY__ENABLE_UTILS "Enable utility classes: netlink monitor, enumerate network interfaces etc" ON)
option(NETTY__DISABLE_FETCH_CONTENT "Disable fetch content if sources of dependencies already exists in the working tree (checks .git subdirectory)" OFF)

if (NETTY__BUILD_STRICT)
    if (NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 14)
    endif()

    set(CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++XX rather than -std=gnu++XX
endif()

get_property(_runtime_output_directory GLOBAL PROPERTY RUNTIME_OUTPUT_DIRECTORY)
get_property(_library_output_directory GLOBAL PROPERTY LIBRARY_OUTPUT_DIRECTORY)
get_property(_archive_output_directory GLOBAL PROPERTY ARCHIVE_OUTPUT_DIRECTORY)

if (NOT _runtime_output_directory)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
endif()

if (NOT _library_output_directory)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
endif()

if (NOT _archive_output_directory)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/library.cmake)

if (NETTY__BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if (NETTY__BUILD_DEMO AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/demo)
    add_subdirectory(demo)
endif()

include(GNUInstallDirs)

install(TARGETS netty
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
