#!/bin/bash

CWD=`pwd`
CEREAL_RELEASE=v1.3.2

if [ -d .git ] ; then

    git pull \
        && git submodule update --init \
        && git submodule update --remote \
        && cd 3rdparty/portable-target && git checkout master && git pull \
        && cd $CWD \
        && cd 3rdparty/pfs/common && git checkout master && git pull  \
        && cd $CWD \
        && cd 3rdparty/cereal && git checkout $CEREAL_RELEASE

fi

