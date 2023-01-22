#!/bin/bash

if [ -z "$UDP_DEMO_BIN_PATH" ] ; then
  echo "ERROR: UDP_DEMO_BIN_PATH must be specified" >&2
  exit 1
fi

if [ ! -x "$UDP_DEMO_BIN_PATH" ] ; then
  echo "ERROR: bad UDP_DEMO_BIN_PATH executable: $UDP_DEMO_BIN_PATH" >&2
  exit 1
fi

DIALOG_HEIGHT=250
DIALOG_WIDTH=450

