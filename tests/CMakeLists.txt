################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.10.05 Initial version.
################################################################################
project(net-lib-TESTS CXX C)

if (ENABLE_COVERAGE)
    message(STATUS "Coverage feature enabled")
    if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        message(FATAL_ERROR "Coverage feature requires GCC")
    endif()

    set(CMAKE_CXX_FLAGS "-g -O0 --coverage")
    set(CMAKE_C_FLAGS "-g -O0 --coverage")
endif()

set(TEST_NAMES
    algorithm
    envelope
    packet_serializer)

foreach (name ${TEST_NAMES})
    if (${name}_SOURCES)
        add_executable(${name} ${${name}_SOURCES} ${name}.cpp)
    else ()
        add_executable(${name} ${name}.cpp)
    endif()

    # https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#Instrumentation-Options
    #
    # --coverage
    #       This option is used to compile and link code instrumented for
    #       coverage analysis. The option is a synonym for -fprofile-arcs
    #       -ftest-coverage (when compiling) and -lgcov (when linking).
    if (ENABLE_COVERAGE)
        target_compile_options(${name} PRIVATE -g -O0 --coverage)
        target_link_libraries(${name} PRIVATE -g -O0 --coverage)
    endif()

    target_link_libraries(${name} PRIVATE pfs::net::p2p)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

if (ENABLE_COVERAGE)
    include(${CMAKE_SOURCE_DIR}/cmake/Coverage.cmake)
    coverage_target("'/usr/*';'*/doctest.h'")
endif()
