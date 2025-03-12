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

static_assert(sizeof(ENetPeer *) == sizeof(enet_socket::socket_id)
    , "Size of `ENetPeer *` and `enet_socket::native_type` types must be equal");

const enet_socket::socket_id enet_socket::kINVALID_SOCKET {0};

struct {
    std::uint32_t timeout_limit;
    std::uint32_t timeout_min;
    std::uint32_t timeout_max;
} NET_QUALITIES [] = {
      { ENET_PEER_TIMEOUT_LIMIT, ENET_PEER_TIMEOUT_MINIMUM, ENET_PEER_TIMEOUT_MAXIMUM }
    , {  8, 1000,  2000 }
    , { 16, 2500,  5000 }
    , { 96, 5000, 10000 }
};

void enet_socket::init (int timeout_limit, int timeout_min, int timeout_max, error * perr)
{
    _host = enet_host_create(nullptr // create a client host
        , 1   // only allow 1 outgoing connection
        , 2   // allow up 2 channels to be used, 0 and 1
        , 0   // assume any amount of incoming bandwidth
        , 0); // assume any amount of outgoing bandwidth

    if (_host == nullptr) {
        pfs::throw_or(perr, error {tr::_("create ENet socket failure")});
        return;
    }

    _timeout_limit = timeout_limit;
    _timeout_min   = timeout_min;
    _timeout_max   = timeout_max;
}

enet_socket::enet_socket ()
{}

enet_socket::enet_socket (net_quality nq, error * perr)
{
    if (nq < net_quality::defaults || nq > net_quality::poor) {
        pfs::throw_or(perr, error {
              std::make_error_code(std::errc::invalid_argument)
            , tr::_("bad network quality value")
        });

        return;
    }

    init(NET_QUALITIES[nq].timeout_limit, NET_QUALITIES[nq].timeout_min
        , NET_QUALITIES[nq].timeout_max, perr);
}

enet_socket::enet_socket (_ENetHost * host, _ENetPeer * peer) noexcept
    : _host(host)
    , _peer(peer)
    , _accepted_socket(true)
{
    _peer->data = & _inpb;
}

enet_socket::enet_socket (enet_socket && other) noexcept
    : _host(other._host)
    , _peer(other._peer)
    , _timeout_limit(other._timeout_limit)
    , _timeout_min(other._timeout_min)
    , _timeout_max(other._timeout_max)
    , _accepted_socket(other._accepted_socket)
    , _inpb(std::move(other._inpb))
{
    other._host = nullptr;
    other._peer = nullptr;

    if (_peer != nullptr)
        _peer->data = & _inpb;
}

enet_socket & enet_socket::operator = (enet_socket && other) noexcept
{
    if (this == & other)
        return *this;

    _host = other._host;
    _peer = other._peer;
    _timeout_limit = other._timeout_limit;
    _timeout_min = other._timeout_min;
    _timeout_max = other._timeout_max;
    _accepted_socket = other._accepted_socket;
    _inpb = std::move(other._inpb);
    other._host = nullptr;
    other._peer = nullptr;

    if (_peer != nullptr)
        _peer->data = & _inpb;

    return *this;
}

enet_socket::~enet_socket ()
{
    if (_host != nullptr) {
        if (!_accepted_socket)
            enet_host_destroy(_host);
    }

    _peer = nullptr; // Peer destroyed automatically in enet_host_destroy()
    _host = nullptr;
}

enet_socket::operator bool () const noexcept
{
    return _peer != nullptr;
}

enet_socket::socket_id enet_socket::id () const noexcept
{
    return _peer == nullptr ? kINVALID_SOCKET : reinterpret_cast<socket_id>(_peer);
}

socket4_addr enet_socket::saddr () const noexcept
{
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

int enet_socket::recv (char * data, int len, error * /*perr*/)
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
    if (enet_peer_has_outgoing_commands(_peer))
        return send_result{netty::send_status::again, 0};

    ENetPacket * packet = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);

    if (packet == nullptr) {
        pfs::throw_or(perr, error {
              std::make_error_code(std::errc::not_enough_memory)
            , tr::_("create packet for sending failure")
        });
        return send_result{send_status::failure, 0};
    }

    auto rc = enet_peer_send(_peer, 0, packet);

    if (rc < 0) {
        enet_packet_destroy(packet);
        pfs::throw_or(perr, error {tr::_("send packet failure")});
        return send_result{send_status::failure, 0};
    }

    // One could just use enet_host_service() instead.
    // enet_host_flush(_host);

    // Once the packet is handed over to ENet with enet_peer_send(), ENet will handle its
    // deallocation and enet_packet_destroy() should not be used upon it.
    // enet_packet_destroy(packet);

    return send_result{send_status::good, static_cast<std::uint64_t>(len)};
}

conn_status enet_socket::connect (socket4_addr const & saddr, error * perr)
{
    if (_host == nullptr) {
        pfs::throw_or(perr, error {tr::_("ENet socket is not initialized")});
        return conn_status::failure;
    }

    if (_peer != nullptr) {
        pfs::throw_or(perr, error {tr::_("ENet socket already connected")});
        return conn_status::failure;
    }

    ENetAddress address;

    address.host = pfs::to_network_order(static_cast<enet_uint32>(saddr.addr));
    address.port = saddr.port;

    // Initiate the connection, allocating the two channels 0 and 1
    _peer = enet_host_connect(_host, & address, 2, 0);

    if (_peer == nullptr) {
        pfs::throw_or(perr, error {tr::_("no available peers for initiating an ENet connection")});
        return conn_status::failure;
    }

    _peer->data = & _inpb;

    enet_peer_timeout(_peer, _timeout_limit, _timeout_min, _timeout_max);

    ENetEvent event;
    auto rc = enet_host_service(_host, & event, 0);

    // No event occurred yet
    if (rc == 0)
        return conn_status::connecting;

    if (rc < 0) {
        pfs::throw_or(perr, error {
            tr::f_("connecting ENet socket failure to: {}", to_string(saddr))
        });

        return conn_status::failure;
    }

    if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
        enet_peer_reset(_peer);
        _peer = nullptr;

        pfs::throw_or(perr, error {
            tr::f_("connecting ENet socket failure to: {} (received disconnect event)", to_string(saddr))
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
