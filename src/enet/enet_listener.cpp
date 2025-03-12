////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/enet/enet_listener.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>
#include <enet/enet.h>

namespace netty {
namespace enet {

static_assert(sizeof(ENetHost *) == sizeof(enet_listener::listener_id)
    , "Size of `ENetHost *` and `enet_socket::native_type` types must be equal");

enet_listener::enet_listener ()
{}

/*
 * With backlog = ENET_PROTOCOL_MAXIMUM_PEER_ID rised segmenatation fault when listener destroying.
 */
enet_listener::enet_listener (socket4_addr const & saddr, error * /*perr*/)
    : _saddr(saddr)
{}

enet_listener::operator bool () const noexcept
{
    return _host != nullptr;
}

bool enet_listener::listen (int backlog, error * perr)
{
    ENetAddress address;

    address.host = pfs::to_network_order(static_cast<enet_uint32>(_saddr.addr));
    address.port = _saddr.port;

    _host = enet_host_create(& address // the address to bind the server host to */
        , backlog // allow up to `backlog` clients and/or outgoing connections
        , 2       // allow up to 2 channels to be used, 0 and 1
        , 0       // assume any amount of incoming bandwidth
        , 0);     // assume any amount of outgoing bandwidth

    if (_host == nullptr) {
        pfs::throw_or(perr, error {tr::_("create ENet listener failure")});
        return false;
    }

    return true;
}

enet_listener::~enet_listener ()
{
    if (_host != nullptr)
        enet_host_destroy(_host);

    _host = nullptr;
}

enet_listener::listener_id enet_listener::id () const noexcept
{
    if (_host == nullptr)
        return enet_socket::kINVALID_SOCKET;

    return reinterpret_cast<listener_id>(_host);
}

enet_socket enet_listener::accept_nonblocking (listener_id listener_sock, error * /*perr*/)
{
    auto peer = reinterpret_cast<ENetPeer *>(listener_sock);
    auto host = peer->host;

    return enet_socket{host, peer};
}

}} // namespace netty::enet
