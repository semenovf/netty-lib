#!/bin/bash

if [ -z "$NETTY_DEMO_BIN_PATH" ] ; then
  echo "ERROR: NETTY_DEMO_BIN_PATH must be specified" >&2
  exit 1
fi

if [ -z "$NETTY_DEMO_SERVER_ADDR" ] ; then
  echo "ERROR: NETTY_DEMO_SERVER_ADDR must be specified" >&2
  exit 1
fi

if [ ! -x "$NETTY_DEMO_BIN_PATH" ] ; then
  echo "ERROR: bad NETTY_DEMO_BIN_PATH executable: $NETTY_DEMO_BIN_PATH" >&2
  exit 1
fi

DIALOG_HEIGHT=250
DIALOG_WIDTH=450

