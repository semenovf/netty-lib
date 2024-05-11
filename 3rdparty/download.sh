#!/bin/bash

LOG_FILE='download.log'
GIT_DOWNLOADER="git clone"

ENET_RELEASE=v1.3.18

# HTTPS
HTTPS_SOURCES="--depth 1 -b ${ENET_RELEASE} --single-branch https://github.com/lsalzman/enet.git enet"

DEFAULT_SOURCES=${HTTPS_SOURCES}
DEFAULT_DOWNLOADER=${GIT_DOWNLOADER}

IFS=$'\n'

echo `date` > ${LOG_FILE}

for src in ${DEFAULT_SOURCES} ; do
    eval "${DEFAULT_DOWNLOADER} $src" >> ${LOG_FILE} 2>&1

    if [ $? -eq 0 ] ; then
        echo "Cloning $src: SUCCESS"
    else
        echo "Cloning $src: FAILURE"
    fi
done
