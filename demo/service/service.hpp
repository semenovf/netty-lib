////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "serializer.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "netty/service.hpp"
#include "netty/poller_types.hpp"
#include "netty/server_poller.hpp"
#include "netty/posix/tcp_server.hpp"
#include "netty/posix/tcp_socket.hpp"
#include <map>

#if NETTY__EPOLL_ENABLED
using client_poller_t = netty::client_epoll_poller_type;
using server_poller_t = netty::server_epoll_poller_type;
#elif NETTY__POLL_ENABLED
using client_poller_t = netty::client_poll_poller_type;
using server_poller_t = netty::server_poll_poller_type;
#endif

using message_serializer_impl_t = message_serializer_impl<pfs::endian::native>;
using deserializer_t = message_serializer_impl_t::istream_type;
using serializer_t   = message_serializer_impl_t::ostream_type;

using service_t = netty::service<server_poller_t, client_poller_t
    , netty::posix::tcp_server, netty::posix::tcp_socket
    , input_envelope<deserializer_t>
    , output_envelope<serializer_t>>;

struct client_connection_context
{
    service_t::client * client;
};

struct server_connection_context
{
    service_t::server * server;
    service_t::server::native_socket_type sock;
};

using client_message_processor_t = message_processor<client_connection_context, deserializer_t>;
using server_message_processor_t = message_processor<server_connection_context, deserializer_t>;
using message_serializer_t = message_serializer<serializer_t>;
