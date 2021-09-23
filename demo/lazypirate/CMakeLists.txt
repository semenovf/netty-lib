add_executable(lazypirate main.cpp client.cpp server.cpp)
target_include_directories(lazypirate PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/libzmq/include
    ${CMAKE_SOURCE_DIR}/3rdparty/cppzmq)
target_link_libraries(lazypirate pfs::net cppzmq libzmq)
