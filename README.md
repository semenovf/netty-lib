# net-lib

## Dependecies

### common-lib (mandatory)

```sh
$ git submodule add https://github.com/semenovf/common-lib.git 3rdparty/pfs/common
```

### io-lib (optional, I/O backend)

```sh
$ git submodule add https://github.com/semenovf/io-lib.git 3rdparty/pfs/io
```

### nng - [Lightweight Messaging Library](https://github.com/nanomsg/nng) (v 1.5.2) (optional, I/O backend)

```sh
$ git submodule add https://github.com/nanomsg/nng.git 3rdparty/nng
$ cd 3rdparty/nng
$ git checkout v1.5.2
```

### nanomsg (v1.1.5) (optional, I/O backend)

```sh
$ git submodule add https://github.com/nanomsg/nanomsg.git 3rdparty/nanomsg
$ cd 3rdparty/nanomsg
$ git checkout 1.1.5
```

Modify `3rdparty/nanomsg/src/CMakeLists.txt`. Find line:

```cmake
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
```
and replace it as pointed below

```cmake
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
```

or

```cmake
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/../cmake/${PROJECT_NAME}-config.cmake.in
```

See [Differences between nanomsg and ZeroMQ](https://nanomsg.org/documentation-zeromq.html)

### LibZMQ (optionaly, not for using in real application) (optional, I/O backend)

```sh
$ git submodule add https://github.com/zeromq/libzmq.git 3rdparty/libzmq
$ cd 3rdparty/libzmq
$ git checkout v4.3.4

$ git submodule add https://github.com/zeromq/cppzmq 3rdparty/cppzmq
$ cd 3rdparty/cppzmq
$ git checkout v4.8.0

```

### cereal - A C++11 library for serialization (v 1.3.0) (mandatory, serialization backend)

```sh
$ git submodule add https://github.com/USCiLab/cereal.git 3rdparty/cereal
$ cd 3rdparty/cereal
$ git checkout v1.3.0
```
