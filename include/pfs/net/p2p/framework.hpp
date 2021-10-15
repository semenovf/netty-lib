////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "hello_packet.hpp"
#include "peer_manager.hpp"
#include "utils.hpp"
#include "pfs/emitter.hpp"
#include "pfs/uuid.hpp"
#include <chrono>

namespace pfs {
namespace net {
namespace p2p {

template <typename TimerPool
    , typename Discoverer
    , typename Observer
    , typename Reader
    , typename Writer>
class framework
{
    using packet_size_type     = std::uint16_t;
    using port_type            = std::uint16_t;
    using timer_pool_type      = TimerPool;
    using discoverer_type      = Discoverer;
    using observer_type        = Observer;
    using reader_type          = Reader;
    using writer_type          = Writer;
    using packet_type          = typename reader_type::packet_type;

private:
    static constexpr packet_size_type DEFAULT_PACKET_SIZE = 512;

    uuid_t           _uuid;
    packet_size_type _packet_size {DEFAULT_PACKET_SIZE};
    timer_pool_type  _timer_pool;
    discoverer_type  _discoverer;
    observer_type    _observer;
    reader_type      _reader;
    writer_type      _writer;
    peer_manager     _peer_manager;

public:
    pfs::emitter_mt<std::string const &> failure;

private:
    void discovery_radiocast ()
    {
        _discoverer.radiocast(_uuid, _reader.port());
    }

    void on_hello_received (inet4_addr const & sender
        , hello_packet const & hello)
    {
        _observer.update(hello.uuid, sender, hello.port
            , _discoverer.expiration_timeout());
    }

    void discovery_observe ()
    {
        _observer.check_expiration();
    }

    void on_rookie_accepted (pfs::uuid_t peer_uuid
            , inet4_addr const & rookie
            , port_type port)
    {
        TRACE_1("Rookie accepted: Hello, {} ({}:{})"
            , std::to_string(peer_uuid)
            , std::to_string(rookie)
            , port);

        _peer_manager.rookie_accepted(peer_uuid, rookie, port);
    }

    void discovery_on_nearest_expiration_changed (std::chrono::milliseconds timepoint)
    {
        auto now = current_timepoint();

        if (now < timepoint) {
            _timer_pool.start_observer_timer(timepoint - now);
        }
    }

    void discovery_on_expired (pfs::uuid_t peer_uuid, inet4_addr const & addr, port_type port)
    {
        TRACE_1("Goodbye, {} ({}:{})"
            , std::to_string(peer_uuid)
            , std::to_string(addr)
            , port);

        // TODO Implement
    }

    void on_packet_received (packet_type const & pkt)
    {
        // TODO Implement
        // TODO Implement delivery manager
    }

//     void on_accepted (shared_endpoint_type ep)
//     {
//         // Peer UUID is unknown yet, should be recevied while handshaking
//         ep->set_uuid(_uuid);
//
//         TRACE_1("Endpoint accepted: ? ({}:{})"
//             , std::to_string(ep->peer_address())
//             , ep->peer_port());
//
//         // TODO Implement handshaking and insert into _incoming_endpoints
//     }
//
//     void on_connected (shared_endpoint_type ep)
//     {
//         // Peer UUID set already while connecting
//         ep->set_uuid(_uuid);
//
//         TRACE_1("Endpoint connected: {} ({}:{})"
//             , std::to_string(ep->peer_uuid())
//             , std::to_string(ep->peer_address())
//             , ep->peer_port());
//
//         _outgoing_endpoints.insert_or_replace(ep);
//     }
//
//     void on_peer_disconnected (shared_endpoint_type ep)
//     {
//         TRACE_1("Peer endpoint disconnected: {} ({}:{})"
//             , std::to_string(ep->peer_uuid())
//             , std::to_string(ep->peer_address())
//             , ep->peer_port());
//
//         // TODO Implement
//     }
//
//     void on_disconnected (shared_endpoint_type ep)
//     {
//         TRACE_1("Endpoint disconnected: {} ({}:{})"
//             , std::to_string(ep->peer_uuid())
//             , std::to_string(ep->peer_address())
//             , ep->peer_port());
//
//         // TODO Implement
//     }

public:
    framework (uuid_t uuid
        , std::uint16_t packet_size = DEFAULT_PACKET_SIZE)
        : _uuid(uuid)
        , _packet_size(packet_size)
    {
        _timer_pool.discovery_timer_timeout.connect(*this, & framework::discovery_radiocast);
        _timer_pool.observer_timer_timeout.connect(*this, & framework::discovery_observe);

        _discoverer.packet_received.connect(*this, & framework::on_hello_received);
        _discoverer.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        _observer.rookie_accepted.connect(*this, & framework::on_rookie_accepted);
        _observer.expired.connect(*this, & framework::discovery_on_expired);
        _observer.nearest_expiration_changed.connect(*this, & framework::discovery_on_nearest_expiration_changed);

        _reader.packet_received.connect(*this, & framework::on_packet_received);
        _reader.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        // FIXME
//         _writer.connected.connect(*this, & framework::on_connected);
//         _writer.disconnected.connect(*this, & framework::on_disconnected);
//         _writer.endpoint_failure.connect([this] (shared_endpoint_type ep, std::string const & error) {
//             this->endpoint_failure(ep, error);
//         });
    }

    framework (framework const &) = delete;
    framework & operator = (framework const &) = delete;

    framework (framework &&) = delete;
    framework & operator = (framework &&) = delete;

    ~framework () = default;

    bool configure (typename discoverer_type::options && discoverer_opts
        , typename reader_type::options && reader_opts)
    {
        return _discoverer.set_options(std::move(discoverer_opts))
            && _reader.set_options(std::move(reader_opts));
    }

    bool start ()
    {
        if (!_reader.start())
            return false;

        if (!_discoverer.start())
            return false;

        if (!_reader.started())
            return false;

        _timer_pool.start_discovery_timer(_discoverer.interval());

        return true;
    }
};

}}} // namespace pfs::net::p2p

