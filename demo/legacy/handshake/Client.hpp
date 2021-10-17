////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/fmt.hpp"
#include "pfs/uuid.hpp"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/handshaker.hpp"
#include "pfs/net/p2p/qt5/listener.hpp"
#include "pfs/net/p2p/qt5/speaker.hpp"
#include "pfs/net/p2p/qt5/endpoint.hpp"
// #include <list>
// #include <functional>

namespace p2p = pfs::net::p2p;

using inet4_addr_t = pfs::net::inet4_addr;
namespace p2p = pfs::net::p2p;

class Client
{
    using uuid_type            = pfs::uuid_t;
    using listener_type        = p2p::qt5::listener;
    using speaker_type         = p2p::qt5::speaker;
    using shared_endpoint_type = p2p::qt5::shared_endpoint;
    using endpoints_map_type   = p2p::endpoints_map<p2p::qt5::endpoint>;
    using output_envelope_type = p2p::output_envelope<>;
    using input_envelope_type  = p2p::input_envelope<>;
    using handshaker_type      = p2p::handshaker<shared_endpoint_type
                                    , output_envelope_type
                                    , input_envelope_type>;

private:
    uuid_type          _uuid;
    listener_type      _listener;
    speaker_type       _speaker;
    endpoints_map_type _outgoing_connections;
    endpoints_map_type _incoming_connections;

public:
    Client (uuid_type uuid)
        : _uuid(uuid)
    {
        _listener.accepted.connect([this] (shared_endpoint_type ep) {
            // TODO Implement handshake handling hear
            //
            //_incoming_connections.insert(ep);
        });

        _listener.disconnected.connect([this] (shared_endpoint_type ep) {
            if (ep->uuid() != pfs::uuid_t{}) {
                _incoming_connections.fetch_and_erase(ep->uuid());
            }
        });

        _listener.endpoint_failure.connect([] (shared_endpoint_type ep
            , std::string const & error) {
            fmt::print(stderr, "Endpoint failure\n");
        });

        _listener.failure.connect([] (std::string const & error) {
            fmt::print(stderr, "Listener failure: {}\n", error);
        });

        _speaker.connected.connect([this] (shared_endpoint_type ep) {
            _outgoing_connections.insert(ep);
        });

        _speaker.disconnected.connect([this] (shared_endpoint_type ep) {
            _outgoing_connections.fetch_and_erase(ep->uuid());
        });

        _speaker.endpoint_failure.connect([this] (shared_endpoint_type ep
                , std::string const & error) {
            fmt::print(stderr, "Error: {} ({}:{}): {}\n"
                , std::to_string(ep->uuid())
                , std::to_string(ep->peer_address())
                , ep->peer_port()
                , error);

            if (ep->connected())
                ep->disconnect();
            else
                _outgoing_connections.fetch_and_erase(ep->uuid());
        });
    }

    uuid_type uuid () const noexcept
    {
        return _uuid;
    }

    bool start_listener (inet4_addr_t const & addr, std::uint16_t port)
    {
        listener_type::options options;
        options.listener_addr4 = addr;
        options.listener_port = port;

        if (!_listener.set_options(std::move(options)))
            return false;

        return _listener.start();
    }

    void connect (inet4_addr_t const & addr, std::uint16_t port)
    {
        _speaker.connect(_uuid, addr, port);
    }

//     void start_handshake (shared_endpoint_type ep)
//     {
//         for (auto pchannel: _channels)
//             pchannel->start_handshake(_uuid);
//     }

//     void process_input ()
//     {
//         while (!_channels.empty()) {
//             for (auto it = _channels.begin(); it != _channels.end();) {
//                 if ((*it)->finished()) {
//                     it = _channels.erase(it);
//                 } else {
//                     (*it)->process_input();
//                     ++it;
//                 }
//             }
//         }
//     }
};

