################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.11.19 Initial version.
################################################################################
set(TESTS frame input_controller writer_queue pubsub)

foreach (target ${TESTS})
    add_executable(tests-pubsub-${target} ${target}.cpp)
    target_link_libraries(tests-pubsub-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-pubsub-${target} COMMAND tests-pubsub-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-pubsub-${target}-qt ${target}.cpp)
        target_link_libraries(tests-pubsub-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-pubsub-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
        add_test(NAME tests-pubsub-${target}-qt COMMAND tests-pubsub-${target}-qt)
    endif()
endforeach()

if (NETTY__ENABLE_ENCRYPTED_SOCKETS)
    add_custom_target(copy-pubsub-cert-key ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ../ssl/cert.pem
            ../ssl/key.pem
            ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM)

    foreach (_name pubsub)
        set(_target "tests-pubsub-${_name}-ssl")
        add_executable(${_target} ${_name}.cpp)
        target_link_libraries(${_target} PRIVATE pfs::netty pfs::lorem)
        target_compile_definitions(${_target} PRIVATE "NETTY__TEST_ENCRYPTED_SOCKETS=1")
        add_test(NAME ${_target} COMMAND ${_target})

        add_dependencies(${_target} copy-pubsub-cert-key)
    endforeach()
endif()
