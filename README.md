# netty-lib

## Clone
```
$ git clone https://github.com/semenovf/netty-lib.git
```

 ## Build on Linux
```
FIXME
```

## Build on Windows
```
FIXME
```

## Dependecies

### portable-target (mandatory, build scripts)

```sh
$ git submodule add https://github.com/semenovf/portable-target.git 3rdparty/portable-target
```

### common-lib (mandatory)

```sh
$ git submodule add https://github.com/semenovf/common-lib.git 3rdparty/pfs/common
```

### cereal - A C++11 library for serialization (v 1.3.0) (mandatory, serialization backend)

```sh
$ git submodule add https://github.com/USCiLab/cereal.git 3rdparty/cereal
$ cd 3rdparty/cereal
$ git checkout v1.3.0
```
