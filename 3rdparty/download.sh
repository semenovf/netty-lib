#!/bin/bash

LOG_FILE='download.log'
GIT_DOWNLOADER="git clone"

CEREAL_RELEASE=master

# HTTPS
HTTPS_SOURCES="--depth 1 -b ${CEREAL_RELEASE} --single-branch https://github.com/USCiLab/cereal.git cereal"

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
