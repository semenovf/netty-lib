////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace pfs {
namespace net {
namespace p2p {

template <typename Derived>
class basic_discoverer
{
public:
    struct options
    {
        // Address to bind listener ('*' is any address)
        std::string   listener_addr4 {"*"};
        std::uint16_t listener_port {42424};
        std::string   listener_interface {"*"};

        // Addresses "*" or "255.255.255.255" are broadcast.
        // Addresses starting from 224 through 239 are multicast.
        // Addresses in other range is unicast.
        std::string peer_addr4 {"*"};
    };

public: // signals
    pfs::emitter_mt<inet4_addr const & /*sender*/
        , bool /*is_sender_local*/
        , std::string const & /*data*/> incoming_data_received;

    pfs::emitter_mt<std::string const & /*error*/> failure;

protected:
    basic_discoverer () {}
    ~basic_discoverer () {}

public:
    bool set_options (options && opts)
    {
        return static_cast<Derived *>(this)->set_options_impl(std::move(opts));
    }

    bool start ()
    {
        return static_cast<Derived *>(this)->start_impl();
    }

    void stop ()
    {
        static_cast<Derived *>(this)->stop_impl();
    }

    bool started () const noexcept
    {
        return static_cast<Derived *>(this)->started_impl();
    }

    /**
     * This slot emits the HELO packet.
     * Can be invoked by timer e.g.
     */
    void radiocast (std::string const & data)
    {
        static_cast<Derived *>(this)->radiocast_impl(data);
    }
};

}}} // namespace pfs::net::p2p
