#!/bin/bash
################################################################################
# Copyright (c) 2021-2023 Vladislav Trifochkin
#
# Unified build script for Android
#
# Changelog:
#      2023.02.17 Initial version.
################################################################################

CMAKE_OPTIONS="${CMAKE_OPTIONS}"

if [ -z "$PROJECT_OPT_PREFIX" ] ; then
    echo "ERROR: PROJECT_OPT_PREFIX is mandatory." >&2
    exit 1
fi

if [ -z "$ANDROID_MIN_SDK_VERSION" ] ; then
    echo "ERROR: ANDROID_MIN_SDK_VERSION is not set." >&2
    exit 1
fi

if [ -z "$ANDROID_TARGET_SDK_VERSION" ] ; then
    ANDROID_TARGET_SDK_VERSION=${ANDROID_MIN_SDK_VERSION}
fi

# Android ABI (see. ANDROID_ABI в External/portable-target/cmake/AndroidToolchain.cmake)
if [ -z "$ANDROID_ABI" ] ; then
    echo "ERROR: ANDROID_ABI is not set." >&2
    exit 1
fi

if [ -z "$BUILD_GENERATOR" ] ; then
    if command -v ninja > /dev/null ; then
        BUILD_GENERATOR=Ninja
    else
        echo "WARN: Preferable build system 'ninja' not found, use default." >&2
        BUILD_GENERATOR="Unix Makefiles"
    fi
fi

if [ -n $BUILD_STRICT ] ; then
    case $BUILD_STRICT in
        [Oo][Nn])
            BUILD_STRICT=ON
            ;;
        *)
            unset BUILD_STRICT
            ;;
    esac
fi

if [ -n "$BUILD_STRICT" ] ; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -D${PROJECT_OPT_PREFIX}BUILD_STRICT=$BUILD_STRICT"
fi

if [ -n "$CXX_STANDARD" ] ; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_CXX_STANDARD=$CXX_STANDARD"
fi

if [ -n "$C_COMPILER" ] ; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_C_COMPILER=$C_COMPILER"
fi

if [ -n "$CXX_COMPILER" ] ; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_CXX_COMPILER=$CXX_COMPILER"
fi

if [ -z "$BUILD_TYPE" ] ; then
    BUILD_TYPE=Debug
fi

CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_BUILD_TYPE=$BUILD_TYPE"

BUILD_DIR=builds/${CXX_COMPILER:-default}.cxx${CXX_STANDARD:-}-${ANDROID_ABI}-${ANDROID_TARGET_SDK_VERSION}-${BUILD_DIR_SUFFIX:-}

# We are inside source directory
if [ -d .git ] ; then
    if [ -z "$SOURCE_DIR" ] ; then
        SOURCE_DIR=`pwd`
    fi
    BUILD_DIR="../$BUILD_DIR"
fi

if [ -z "$SOURCE_DIR" ] ; then
    # We are inside subdirectory (usually from scripts directory)
    if [ -d ../.git ] ; then
        SOURCE_DIR=`pwd`/..
        BUILD_DIR="../../$BUILD_DIR"
    else
        echo "ERROR: SOURCE_DIR must be specified" >&2
        exit 1
    fi
fi

mkdir -p ${BUILD_DIR} \
    && cd ${BUILD_DIR} \
    && cmake -G "${BUILD_GENERATOR}" \
        $CMAKE_OPTIONS \
        -DANDROID=ON \
        -DANDROID_MIN_SDK_VERSION=${ANDROID_MIN_SDK_VERSION} \
        -DANDROID_TARGET_SDK_VERSION=${ANDROID_TARGET_SDK_VERSION} \
        -DANDROID_ABI=${ANDROID_ABI} \
        -DCMAKE_TOOLCHAIN_FILE=${SOURCE_DIR}/3rdparty/portable-target/cmake/v2/android/AndroidToolchain.cmake \
        -Wno-dev \
        ${SOURCE_DIR} \
    && cmake --build .
