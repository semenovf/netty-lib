#!/bin/bash

. ./common.sh

if [ -z "$UDP_DEMO_UNICAST_RECEIVER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_UNICAST_RECEIVER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_MULTICAST_RECEIVER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_MULTICAST_RECEIVER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_BROADCAST_RECEIVER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_BROADCAST_RECEIVER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_RECEIVER_PORT" ] ; then
  echo "ERROR: UDP_DEMO_RECEIVER_PORT must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_LOCAL_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_LOCAL_ADDR must be specified" >&2
  exit 1
fi

CHOICE=`zenity --list \
  --height=$DIALOG_HEIGHT \
  --width=$DIALOG_WIDTH \
  --title="Select Receiver" \
  --print-column=1 \
  --column="Code" --column="Receiver" --column="Address"\
  1 "Unicast"   "${UDP_DEMO_UNICAST_RECEIVER_ADDR}:${UDP_DEMO_RECEIVER_PORT}" \
  2 "Multicast" "${UDP_DEMO_MULTICAST_RECEIVER_ADDR}:${UDP_DEMO_RECEIVER_PORT}"\
  3 "Broadcast" "${UDP_DEMO_BROADCAST_RECEIVER_ADDR}:${UDP_DEMO_RECEIVER_PORT}" \
  4 "Global broadcast" "255.255.255.255:${UDP_DEMO_RECEIVER_PORT}"`

case $CHOICE in
  1) EXEC="$UDP_DEMO_BIN_PATH --receiver --addr=$UDP_DEMO_UNICAST_RECEIVER_ADDR --port=$UDP_DEMO_RECEIVER_PORT --local-addr=$UDP_DEMO_LOCAL_ADDR" ;;
  2) EXEC="$UDP_DEMO_BIN_PATH --receiver --addr=$UDP_DEMO_MULTICAST_RECEIVER_ADDR --port=$UDP_DEMO_RECEIVER_PORT --local-addr=$UDP_DEMO_LOCAL_ADDR" ;;
  3) EXEC="$UDP_DEMO_BIN_PATH --receiver --addr=$UDP_DEMO_BROADCAST_RECEIVER_ADDR --port=$UDP_DEMO_RECEIVER_PORT --local-addr=$UDP_DEMO_LOCAL_ADDR" ;;
  4) EXEC="$UDP_DEMO_BIN_PATH --receiver --addr=255.255.255.255 --port=$UDP_DEMO_RECEIVER_PORT --local-addr=$UDP_DEMO_LOCAL_ADDR" ;;
esac

if [ -n "$EXEC" ] ; then
    echo "Executing: ${EXEC}"
    $EXEC
fi
