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
#include <pfs/numeric_cast.hpp>
#include <enet/enet.h>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace netty {
namespace enet {

static_assert(std::is_same<std::uintptr_t, enet_socket::native_type>::value
    , "`std::uintptr_t` and `enet_socket::native_type` types must be same");

const enet_socket::native_type enet_socket::kINVALID_SOCKET {0};

enet_socket::enet_socket (uninitialized)
{}

enet_socket::enet_socket ()
{
    _host = enet_host_create(nullptr // create a client host
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

enet_socket::enet_socket (native_type sock)
{
    _host = reinterpret_cast<ENetHost *>(sock);
}

enet_socket::operator bool () const noexcept
{
    return _host != nullptr;
}

enet_socket::native_type enet_socket::native () const noexcept
{
    return _host == nullptr ? kINVALID_SOCKET : reinterpret_cast<native_type>(_host);
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

int enet_socket::available (error * /*perr*/) const
{
    return pfs::numeric_cast<int>(_inpb.size());
}

int enet_socket::recv (char * data, int len, error * perr)
{
    std::size_t sz = len > 0 ? static_cast<std::size_t>(len) : 0;
    sz = (std::min)(sz, _inpb.size());

    if (sz == 0)
        return 0;

    std::memcpy(data, _inpb.data(), sz);

    if (sz == _inpb.size())
        _inpb.clear();
    else
        _inpb.erase(_inpb.begin(), _inpb.begin() + sz);

    return sz;
}

send_result enet_socket::send (char const * data, int len, error * perr)
{
    ENetPacket * packet = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
    auto rc = enet_peer_send(_peer, 0, packet);

    if (rc < 0) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("send packet failure")
        });

        return send_result{send_status::failure, 0};
    }

    // FIXME
    // One could just use enet_host_service() instead.
    enet_host_flush(_host);

    enet_packet_destroy(packet);

    return send_result{send_status::good, len};
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

    if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
        enet_peer_reset(_peer);
        _peer = nullptr;

        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::f_("connecting ENet socket failure to: {} (received disconnect event)", to_string(saddr))
        });

        return conn_status::failure;
    }

    // rc > 0 -> an event occurred, check type
    if (event.type != ENET_EVENT_TYPE_CONNECT)
        return conn_status::connecting;

    return conn_status::connected;
}

void enet_socket::disconnect (error * /*perr*/)
{
    if (_peer == nullptr)
        return;

    enet_peer_disconnect(_peer, 0);
    _peer = nullptr;
}

}} // namespace netty::enet
