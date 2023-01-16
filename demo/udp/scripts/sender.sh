#!/bin/bash

. ./common.sh

if [ -z "$UDP_DEMO_UNICAST_SENDER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_UNICAST_SENDER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_MULTICAST_SENDER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_MULTICAST_SENDER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_BROADCAST_SENDER_ADDR" ] ; then
  echo "ERROR: UDP_DEMO_BROADCAST_SENDER_ADDR must be specified" >&2
  exit 1
fi

if [ -z "$UDP_DEMO_SENDER_PORT" ] ; then
  echo "ERROR: UDP_DEMO_RECEIVER_PORT must be specified" >&2
  exit 1
fi

CHOICE=`zenity --list \
  --height=$DIALOG_HEIGHT \
  --width=$DIALOG_WIDTH \
  --title="Select Sender" \
  --print-column=1 \
  --column="Code" --column="Sender" --column="Address"\
  1 "Unicast"   "${UDP_DEMO_UNICAST_SENDER_ADDR}:${UDP_DEMO_SENDER_PORT}" \
  2 "Multicast" "${UDP_DEMO_MULTICAST_SENDER_ADDR}:${UDP_DEMO_SENDER_PORT}"\
  3 "Broadcast" "${UDP_DEMO_BROADCAST_SENDER_ADDR}:${UDP_DEMO_SENDER_PORT}" \
  4 "Global broadcast" "255.255.255.255:${UDP_DEMO_SENDER_PORT}"`

case $CHOICE in
  1) $UDP_DEMO_BIN_PATH --sender --addr=$UDP_DEMO_UNICAST_SENDER_ADDR --port=$UDP_DEMO_SENDER_PORT  ;;
  2) $UDP_DEMO_BIN_PATH --sender --addr=$UDP_DEMO_MULTICAST_SENDER_ADDR --port=$UDP_DEMO_SENDER_PORT ;;
  3) $UDP_DEMO_BIN_PATH --sender --addr=$UDP_DEMO_BROADCAST_SENDER_ADDR --port=$UDP_DEMO_SENDER_PORT ;;
  4) $UDP_DEMO_BIN_PATH --sender --addr=255.255.255.255 --port=$UDP_DEMO_SENDER_PORT ;;
esac
