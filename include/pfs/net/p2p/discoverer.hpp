////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "hello_packet.hpp"
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
        inet4_addr    listener_addr4 {};
        std::uint16_t listener_port {42424};
        std::string   listener_interface {"*"};

        // Addresses "*" or "255.255.255.255" are broadcast.
        // Addresses starting from 224 through 239 are multicast.
        // Addresses in other range is unicast.
        inet4_addr peer_addr4 {};

        std::chrono::milliseconds interval {1000};
        std::chrono::milliseconds expiration_timeout {5000};
    };

public: // signals
    pfs::emitter_mt<inet4_addr const &, hello_packet const &> packet_received;
    pfs::emitter_mt<std::string const &> failure;

protected:
    basic_discoverer () {}
    ~basic_discoverer () {}

    basic_discoverer (basic_discoverer const &) = delete;
    basic_discoverer & operator = (basic_discoverer const &) = delete;

    basic_discoverer (basic_discoverer &&) = default;
    basic_discoverer & operator = (basic_discoverer &&) = default;

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
        return static_cast<Derived const *>(this)->started_impl();
    }

    /**
     * This slot emits the HELO packet.
     * Can be invoked by timer e.g.
     */
    void radiocast (uuid_t uuid, std::uint16_t port)
    {
        static_cast<Derived *>(this)->radiocast_impl(uuid, port);
    }

    std::chrono::milliseconds interval () const noexcept
    {
        return static_cast<Derived const *>(this)->interval_impl();
    }

    std::chrono::milliseconds expiration_timeout () const noexcept
    {
        return static_cast<Derived const *>(this)->expiration_timeout_impl();
    }
};

}}} // namespace pfs::net::p2p
