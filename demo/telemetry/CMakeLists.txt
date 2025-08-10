################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.07.29 Initial version.
################################################################################
project(telemetry)

# if (TARGET msgpack-cxx AND TARGET cppzmq-static AND TARGET libzmq)
#     add_executable(${PROJECT_NAME}-zmq-msgpack main.cpp
#         msgpack_serializer.cpp
#         zmq_subscriber.cpp)
#     target_link_libraries(${PROJECT_NAME}-zmq-msgpack PRIVATE pfs::common pfs::netty msgpack-cxx cppzmq-static libzmq)
#     target_compile_definitions(${PROJECT_NAME}-zmq-msgpack PRIVATE "TELEMETRY_ZMQ_MSGPACK_BACKEND=1")
# endif()

# TODO Need to implement Qt5/6 based producer/consumer
# if (NOT QT_VERSION_MAJOR)
#     if (Qt6_VERSION_MAJOR)
#         set(QT_VERSION_MAJOR 6)
#     elif (Qt5_VERSION_MAJOR)
#         set(QT_VERSION_MAJOR 5)
#     else()
#         find_package(QT NAMES Qt6 Qt5)
#
#         if (NOT QT_VERSION_MAJOR)
#             message(WARNING "Unable to determine QT_VERSION_MAJOR, Qt support disabled")
#         endif()
#     endif()
# endif()
#
# if (QT_VERSION_MAJOR)
#     find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network)
#
#     if (Qt${QT_VERSION_MAJOR}Network_FOUND)
#         message(STATUS "Qt ${QT_VERSION_MAJOR} Network component found.")
#
#         add_executable(${PROJECT_NAME}-qt main.cpp
#             qt_publisher.cpp
#             qt_serializer.cpp
#             qt_subscriber.cpp)
#         target_link_libraries(${PROJECT_NAME}-qt PRIVATE pfs::common pfs::netty
#             Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)
#         target_compile_definitions(${PROJECT_NAME}-qt PRIVATE "TELEMETRY_QT_BACKEND=1")
#     endif()
# endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE pfs::common pfs::netty)
