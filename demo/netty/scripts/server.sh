#!/bin/bash

. ./common.sh

CHOICE=`zenity --list \
  --height=$DIALOG_HEIGHT \
  --width=$DIALOG_WIDTH \
  --title="Select server" \
  --print-column=1 \
  --column="Code" --column="Poller" --column="Server" --column="Socket" \
  1 "UDT::epoll_poller"    "udt_server" "udt_socket" \
  2 "POSIX::select_poller" "tcp_server" "tcp_socket" \
  3 "POSIX::poll_poller"   "tcp_server" "tcp_socket" \
  4 "Linux::epoll_poller"  "tcp_server" "tcp_socket"`

case $CHOICE in
  1) $NETTY_DEMO_BIN_PATH --tcp --server --addr=$NETTY_DEMO_SERVER_ADDR --poller=udt    ;;
  2) $NETTY_DEMO_BIN_PATH --tcp --server --addr=$NETTY_DEMO_SERVER_ADDR --poller=select ;;
  3) $NETTY_DEMO_BIN_PATH --tcp --server --addr=$NETTY_DEMO_SERVER_ADDR --poller=poll   ;;
  4) $NETTY_DEMO_BIN_PATH --tcp --server --addr=$NETTY_DEMO_SERVER_ADDR --poller=epoll  ;;
esac
