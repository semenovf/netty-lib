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
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <pfs/netty/service.hpp>
#include <pfs/netty/poller_types.hpp>
#include <pfs/netty/server_poller.hpp>

#if defined(NETTY__SELECT_ENABLED) || defined(NETTY__POLL_ENABLED) || defined(NETTY__EPOLL_ENABLED)
#   include <pfs/netty/posix/tcp_listener.hpp>
#   include <pfs/netty/posix/tcp_socket.hpp>
#endif

#if NETTY__UDT_ENABLED
#   include <pfs/netty/udt/udt_listener.hpp>
#   include <pfs/netty/udt/udt_socket.hpp>
#endif

#if NETTY__ENET_ENABLED
#   include <pfs/netty/enet/enet_listener.hpp>
#   include <pfs/netty/enet/enet_socket.hpp>
#endif

enum class PollerEnum {
      Unknown
#if NETTY__SELECT_ENABLED
    , Select
#endif

#if NETTY__POLL_ENABLED
    , Poll
#endif

#if NETTY__EPOLL_ENABLED
    , EPoll
#endif

#if NETTY__UDT_ENABLED
    , UDT
#endif

#if NETTY__ENET_ENABLED
    , ENet
#endif

}; // PollerEnum

using message_serializer_impl_t = message_serializer_impl<pfs::endian::native>;
using deserializer_t = message_serializer_impl_t::istream_type;
using serializer_t   = message_serializer_impl_t::ostream_type;
using message_serializer_t = message_serializer<serializer_t>;
using input_envelope_t = input_envelope<deserializer_t>;
using output_envelope_t = output_envelope<serializer_t>;

template <PollerEnum P>
struct service_traits;

#define CONTEXT_DECLARATIONS                                    \
    struct client_connection_context                            \
    {                                                           \
        using traits_type = service_traits;                     \
        typename service_type::requester * requester;           \
    };                                                          \
                                                                \
    struct server_connection_context                            \
    {                                                           \
        using traits_type = service_traits;                     \
        typename service_type::respondent * respondent;         \
        typename service_type::respondent::native_socket_type sock; \
    };                                                          \
                                                                \
    using client_message_processor_type = message_processor<client_connection_context, deserializer_t>; \
    using server_message_processor_type = message_processor<server_connection_context, deserializer_t>;


#if NETTY__SELECT_ENABLED
template <>
struct service_traits<PollerEnum::Select>
{
    using service_type = netty::service<
          netty::server_select_poller_type
        , netty::client_select_poller_type
        , netty::posix::tcp_listener, netty::posix::tcp_socket
        , input_envelope_t
        , output_envelope_t>;

    CONTEXT_DECLARATIONS
};
#endif

#if NETTY__POLL_ENABLED
template <>
struct service_traits<PollerEnum::Poll>
{
    using service_type = netty::service<
          netty::server_poll_poller_type
        , netty::client_poll_poller_type
        , netty::posix::tcp_listener, netty::posix::tcp_socket
        , input_envelope_t
        , output_envelope_t>;

    CONTEXT_DECLARATIONS
};
#endif

#if NETTY__EPOLL_ENABLED
template <>
struct service_traits<PollerEnum::EPoll>
{
    using service_type = netty::service<
          netty::server_epoll_poller_type
        , netty::client_epoll_poller_type
        , netty::posix::tcp_listener, netty::posix::tcp_socket
        , input_envelope_t
        , output_envelope_t>;

    CONTEXT_DECLARATIONS
};
#endif

#if NETTY__UDT_ENABLED
template <>
struct service_traits<PollerEnum::UDT>
{
    using service_type = netty::service<
          netty::server_udt_poller_type
        , netty::client_udt_poller_type
        , netty::udt::udt_listener, netty::udt::udt_socket
        , input_envelope_t
        , output_envelope_t>;

    CONTEXT_DECLARATIONS
};
#endif

#if NETTY__ENET_ENABLED
template <>
struct service_traits<PollerEnum::ENet>
{
    using service_type = netty::service<
          netty::server_enet_poller_type
        , netty::client_enet_poller_type
        , netty::enet::enet_listener, netty::enet::enet_socket
        , input_envelope_t
        , output_envelope_t>;

    CONTEXT_DECLARATIONS
};
#endif
