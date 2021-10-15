////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "endpoint.hpp"
#include "pfs/uuid.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <chrono>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <typename Impl, typename Endpoint>
class basic_listener
{
public:
    struct options
    {
        // Address to bind listener (inet4_addr{} is any address)
        inet4_addr listener_addr4;
        std::uint16_t listener_port {0};
        std::string listener_interface {"*"};
    };

public: // signals
    pfs::emitter_mt<std::shared_ptr<Endpoint>> accepted;
    pfs::emitter_mt<std::shared_ptr<Endpoint>> disconnected;
    pfs::emitter_mt<std::shared_ptr<Endpoint>
        , std::string const &> endpoint_failure;

    pfs::emitter_mt<std::string const &> failure;

protected:
    basic_listener () {}

    ~basic_listener ()
    {
        accepted.disconnect_all();
        disconnected.disconnect_all();
        endpoint_failure.disconnect_all();
        failure.disconnect_all();
    }

    basic_listener (basic_listener const &) = delete;
    basic_listener & operator = (basic_listener const &) = delete;

    basic_listener (basic_listener &&) = default;
    basic_listener & operator = (basic_listener &&) = default;

public:
    bool set_options (options && opts)
    {
        return static_cast<Impl *>(this)->set_options_impl(std::move(opts));
    }

    bool start ()
    {
        return static_cast<Impl *>(this)->start_impl();
    }

    void stop ()
    {
        static_cast<Impl *>(this)->stop_impl();
    }

    bool started () const noexcept
    {
        return static_cast<Impl const *>(this)->started_impl();
    }

    inet4_addr address () const noexcept
    {
        return static_cast<Impl const *>(this)->address_impl();
    }

    std::uint16_t port () const noexcept
    {
        return static_cast<Impl const *>(this)->port_impl();
    }
};

}}} // namespace pfs::net::p2p
