////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <class Impl>
class basic_origin_endpoint
{
public: // signals
    emitter_mt<> connected;
    emitter_mt<> disconnected;
    emitter_mt<std::string const & /*error*/> failure;

protected:
    basic_origin_endpoint () {}

    ~basic_origin_endpoint ()
    {
        connected.disconnect_all();
        disconnected.disconnect_all();
        failure.disconnect_all();
    }

    void connect (inet4_addr const & addr, std::uint16_t port)
    {
        return static_cast<Impl *>(this)->connect_impl(addr, port);
    }
};

template <class Impl>
class basic_peer_endpoint
{
public: // signals
    emitter_mt<> disconnected;
    emitter_mt<std::string const & /*error*/> failure;

protected:
    basic_peer_endpoint () {}

    ~basic_peer_endpoint ()
    {
        disconnected.disconnect_all();
        failure.disconnect_all();
    }
};

}}} // namespace pfs::net::p2p
