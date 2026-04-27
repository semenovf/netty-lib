################################################################################
# Copyright (c) 2026 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2026.04.24 Initial version.
################################################################################
set(TESTS ssl)

foreach (_t ${TESTS})
    set(_target tests-ssl-${_t})
    add_executable(${_target} ${_t}.cpp)
    target_link_libraries(${_target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME ${_target} COMMAND ${_target})
endforeach()

add_custom_target(copy-cert-key ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        cert.pem
        key.pem
        "$<TARGET_FILE_DIR:tests-ssl-ssl>"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)

add_dependencies(tests-ssl-ssl copy-cert-key)
