////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/enet/enet_socket.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>
#include <enet/enet.h>
#include <type_traits>

namespace netty {
namespace enet {

static_assert(std::is_same<ENetSocket, enet_socket::native_type>::value
    , "`ENetSocket` and `enet_socket::native_type` types must be same");

enet_socket::enet_socket (uninitialized)
{}

enet_socket::enet_socket ()
{
    _host = enet_host_create (nullptr // create a client host
        , 1   // only allow 1 outgoing connection
        , 2   // allow up 2 channels to be used, 0 and 1
        , 0   // assume any amount of incoming bandwidth
        , 0); // assume any amount of outgoing bandwidth

    if (_host == nullptr) {
        throw error {
              errc::socket_error
            , tr::_("create ENet socket failure")
        };
    }
}

enet_socket::enet_socket (enet_socket && other)
    : _host(other._host)
    , _peer(other._peer)
{
    other._host = nullptr;
    other._peer = nullptr;
}

enet_socket & enet_socket::operator = (enet_socket && other)
{
    if (this == & other)
        return *this;

    _host = other._host;
    _peer = other._peer;
    other._host = nullptr;
    other._peer = nullptr;

    return *this;
}

enet_socket::~enet_socket ()
{
    if (_host != nullptr) {
        enet_host_destroy(_host);
        _peer = nullptr; // Peer destroyed automatically in enet_host_destroy()
        _host = nullptr;
    }
}

// // Accepted socket
// tcp_socket::tcp_socket (native_type sock, socket4_addr const & saddr)
//     : inet_socket(sock, saddr)
// {}

enet_socket::operator bool () const noexcept
{
    return _host != nullptr;
}

enet_socket::native_type enet_socket::native () const noexcept
{
    return _host == nullptr ? kINVALID_SOCKET : _host->socket;
}

socket4_addr enet_socket::saddr () const noexcept
{
    if (_host == nullptr)
        return socket4_addr{};

    // Socket connected
    if (_peer != nullptr) {
        return socket4_addr {
              inet4_addr {pfs::to_native_order(_peer->address.host)}
            , _peer->address.port
        };
    }

    return socket4_addr{};
}

int enet_socket::available (error * perr) const
{
    //TODO
    return 0;
}

int enet_socket::recv (char * data, int len, error * perr)
{
    //TODO
    return 0;
}

conn_status enet_socket::connect (socket4_addr const & saddr, error * perr)
{
    if (_host == nullptr) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("ENet socket is not initialized")
        });

        return conn_status::failure;
    }

    if (_peer != nullptr) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("ENet socket already connected")
        });

        return conn_status::failure;
    }

    ENetAddress address;

    address.host = pfs::to_network_order(static_cast<enet_uint32>(saddr.addr));
    address.port = saddr.port;

    // Initiate the connection, allocating the two channels 0 and 1
    _peer = enet_host_connect(_host, & address, 2, 0);

    if (_peer == nullptr) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("no available peers for initiating an ENet connection")
        });

        return conn_status::failure;
    }

    ENetEvent event;
    auto rc = enet_host_service(_host, & event, 0);

    // No event occurred yet
    if (rc == 0)
        return conn_status::connecting;

    if (rc < 0) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::f_("connecting ENet socket failure to: {}", to_string(saddr))
        });

        return conn_status::failure;
    }

    // rc > 0 -> an event occurred, check type
    if (event.type != ENET_EVENT_TYPE_CONNECT)
        return conn_status::connecting;

    return conn_status::connected;
}

void enet_socket::disconnect (error * perr)
{
    // TODO
}

}} // namespace netty::enet
