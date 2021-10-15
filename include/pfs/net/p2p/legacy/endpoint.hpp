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
#include "pfs/uuid.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <string>

namespace pfs {
namespace net {
namespace p2p {

enum class endpoint_state
{
      disconnected // The endpoint is not connected.
    , hostlookup   // The endpoint is performing a host name lookup.
    , connecting   // The endpoint has started establishing a connection.
    , connected    // A connection is established.
    , bound        // The endpoint is bound to an address and port.
    , closing      // The socket is about to close (data may still be waiting to be written).
};

template <class Impl>
class basic_endpoint
{
public:
    using seq_number_type = std::uint32_t;

protected:
    uuid_t _uuid;
    uuid_t _peer_uuid;

    // Initialized at connected state
    inet4_addr    _addr;
    std::uint16_t _port {0};

    seq_number_type _self_sn {0};
    seq_number_type _peer_sn {0};

public: // signals
    pfs::emitter_mt<> ready_read;

protected:
    basic_endpoint (inet4_addr const & addr, std::uint16_t port)
        : _addr(addr)
        , _port(port)
    {}

    ~basic_endpoint () = default;

    basic_endpoint (basic_endpoint const &) = delete;
    basic_endpoint & operator = (basic_endpoint const &) = delete;
    basic_endpoint (basic_endpoint &&) = default;
    basic_endpoint & operator = (basic_endpoint &&) = default;

public:
    uuid_t uuid () const noexcept
    {
        return _uuid;
    }

    void set_uuid (uuid_t uuid)
    {
        _uuid = uuid;
    }

    uuid_t peer_uuid () const noexcept
    {
        return _peer_uuid;
    }

    void set_peer_uuid (uuid_t uuid)
    {
        _peer_uuid = uuid;
    }

    void set_seq_numbers (seq_number_type self_sn, seq_number_type peer_sn)
    {
        _self_sn = self_sn;
        _peer_sn = peer_sn;
    }

    inet4_addr peer_address () const noexcept
    {
        return _addr;
    }

    std::uint16_t peer_port () const noexcept
    {
        return _port;
    }

    std::int32_t send (char const * data, std::int32_t size)
    {
        return static_cast<Impl const *>(this)->send_impl(data, size);
    }

    std::int32_t recv (char * data, std::int32_t size)
    {
        return static_cast<Impl const *>(this)->recv_impl(data, size);
    }

    endpoint_state state () const
    {
        return static_cast<Impl const *>(this)->state_impl();
    }

    bool connected () const
    {
        return state() == endpoint_state::connected;
    }

    bool disconnected () const
    {
        return state() == endpoint_state::disconnected;
    }

    void disconnect ()
    {
        static_cast<Impl *>(this)->disconnect_impl();
    }
};

}}} // namespace pfs::net::p2p
