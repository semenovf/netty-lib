################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.11.19 Initial version.
################################################################################
set(TESTS pubsub)

foreach (target ${TESTS})
    add_executable(tests-pubsub-${target} ${target}.cpp)
    target_link_libraries(tests-pubsub-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-pubsub-${target} COMMAND tests-pubsub-${target})
endforeach()
