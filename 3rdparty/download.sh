#!/bin/bash

LOG_FILE='download.log'
GIT_DOWNLOADER="git clone"

JANSSON_RELEASE=v2.14

# HTTPS
HTTPS_SOURCES="--depth 1 -b ${JANSSON_RELEASE} --single-branch https://github.com/akheron/jansson.git jansson"

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
