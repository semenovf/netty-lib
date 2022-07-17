#!/bin/bash

CWD=`pwd`
CEREAL_RELEASE=v1.3.2

if [ -e .git ] ; then

    git checkout master && git pull origin master \
        && git submodule update --init --recursive \
        && git submodule update --init --recursive --remote -- 3rdparty/portable-target \
        && git submodule update --init --recursive --remote -- 3rdparty/pfs/common \
        && cd 3rdparty/cereal && git checkout $CEREAL_RELEASE

fi

