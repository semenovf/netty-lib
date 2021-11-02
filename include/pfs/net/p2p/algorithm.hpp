////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.20 Initial version.
//      2021.11.01 Complete basic version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "hello_packet.hpp"
#include "packet.hpp"
#include "timing.hpp"
#include "trace.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/uuid.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <list>
#include <unordered_map>
#include <utility>
#include <cassert>

namespace pfs {
namespace net {
namespace p2p {

namespace {
constexpr std::chrono::milliseconds DEFAULT_DISCOVERY_INTERVAL { 5000};
constexpr std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {10000};
}

template <
      typename DiscoveryUdpSocket
    , typename ReliableUdpSocket
    , typename Poller
    , std::size_t PacketSize>
class algorithm
{
    using discovery_socket_type = DiscoveryUdpSocket;
    using socket_type           = ReliableUdpSocket;
    using poller_type           = Poller;
    using packet_type           = packet<PacketSize>;
//     using packets_queue_type         = ring_buffer_mt<packet_type, 64>;
    using input_envelope_type   = input_envelope<>;
    using output_envelope_type  = output_envelope<>;
    using socket_id             = decltype(socket_type{}.id());

    struct socket_address
    {
        inet4_addr    addr;
        std::uint16_t port;
    };

    struct socket_info
    {
        uuid_t         uuid; // For writers (connected sockets) only
        socket_type    sock;
        socket_address saddr;
    };

    using socket_container = std::list<socket_info>;
    using socket_iterator  = typename socket_container::iterator;

private:
    uuid_t         _uuid;
    socket_type    _listener;
    socket_address _listener_address;
    std::chrono::milliseconds _poll_interval {10};

    // Discovery specific members
    struct {
        discovery_socket_type       receiver;
        discovery_socket_type       transmitter;
        std::chrono::milliseconds   last_timepoint;
        std::chrono::milliseconds   transmit_interval {DEFAULT_DISCOVERY_INTERVAL};
        std::vector<socket_address> targets;
    } _discovery;

    std::chrono::milliseconds _expiration_timeout {DEFAULT_EXPIRATION_TIMEOUT};

    // All sockets (readers / writers)
    socket_container _sockets;

    // Mapping socket identified by key (native handler) to socket iterator
    // (see _sockets)
    std::unordered_map<socket_id, socket_iterator> _index_by_socket_id;

    // Writer sockets
    std::unordered_map<uuid_t, socket_iterator> _writers;

    std::unordered_map<socket_id, std::chrono::milliseconds> _expiration_timepoints;

    // Poller to observe socket status (from connecting to connected status)
    poller_type _connecting_poller;

    // Default poller
    poller_type _poller;

    // packets_queue_type _input_queue;
    // packets_queue_type _output_queue;

public:
    pfs::emitter_mt<std::string const &> failure;

    /**
     * Emitted when new writer socket ready (connected).
     */
    emitter_mt<uuid_t, inet4_addr const &, std::uint16_t> writer_ready;

    /**
     * Emitted when new address accepted by discoverer.
     */
    emitter_mt<uuid_t, inet4_addr const &, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    emitter_mt<uuid_t, inet4_addr const &, std::uint16_t> peer_expired;

public:
    algorithm (uuid_t uuid)
        : _uuid(uuid)
        , _connecting_poller("connecting")
        , _poller("main")
    {
        _listener.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        _discovery.receiver.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        _discovery.transmitter.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        _connecting_poller.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        _poller.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        auto now = current_timepoint();

        _discovery.last_timepoint = now > _discovery.transmit_interval
            ? now - _discovery.transmit_interval : now;
    }

    ~algorithm ()
    {
        _poller.remove(_listener.id());
    }

    algorithm (algorithm const &) = delete;
    algorithm & operator = (algorithm const &) = delete;

    algorithm (algorithm &&) = default;
    algorithm & operator = (algorithm &&) = default;

    uuid_t const & uuid () const noexcept
    {
        return _uuid;
    }

    template <typename Configurator>
    bool configure (Configurator && c)
    {
        bool success = true;

        _discovery.transmit_interval = c.discovery_transmit_interval();
        _expiration_timeout = c.expiration_timeout();
        _poll_interval      = c.poll_interval();

        success = success && _connecting_poller.initialize();
        success = success && _poller.initialize();
        success = success && _discovery.receiver.bind(c.discovery_address()
            , c.discovery_port());

        _listener_address.addr = c.listener_address();
        _listener_address.port = c.listener_port();
        success = success && _listener.bind(_listener_address.addr
            , _listener_address.port);

        success = success && _listener.listen(c.backlog());

        if (success) {
            _poller.add(_listener.id());
        }

        if (success) {
            TRACE_2("Discovery listener backend: {}"
                , _discovery.receiver.backend_string());
            TRACE_2("General listener backend: {}"
                , _listener.backend_string());

            TRACE_1("Discovery listener: {}:{}. Status: {}"
                , std::to_string(c.discovery_address())
                , c.discovery_port()
                , _discovery.receiver.state_string());
            TRACE_1("General listener: {}:{}. Status: {}"
                , std::to_string(_listener_address.addr)
                , _listener_address.port
                , _listener.state_string());

            auto listener_options = _listener.dump_options();

            TRACE_2("General listener options:", 0);

            for (auto const & opt_item: listener_options) {
                TRACE_2("   * {}: {}", opt_item.first, opt_item.second);
            }
        }

        return success;
    }

    /**
     * @return @c false if some error occurred, otherwise return @c true.
     */
    void loop ()
    {
        poll();
        process_discovery();

        if (!_sockets.empty()) {
            TRACE_3("First socket ({}) state: {}"
                , _sockets.front().sock.id()
                , _sockets.front().sock.state_string());
        }

//         process_incoming_packets();
//         send_outgoing_packets();
//         update_statistics();
    }

    void add_discovery_target (inet4_addr const & addr, std::uint16_t port)
    {
        _discovery.targets.emplace_back(socket_address{addr, port});

        if (is_multicast(addr)) {
            auto success = _discovery.receiver.join_multicast_group(addr);

            if (success) {
                TRACE_2("Discovery receiver joined into multicast group: {}"
                    , std::to_string(addr));
            }
        }
    }

    std::streamsize send (uuid_t uuid, char const * data, std::streamsize len)
    {
        int bytes_written = 0;

        split_into_packets<packet_type::PACKET_SIZE>(_listener.uuid, data, len
        , [this, uuid, & bytes_written] {
            // TODO Implement
        });

        return bytes_written;
    }

private:
    socket_iterator index_socket (socket_info && sockinfo)
    {
        auto pos = _sockets.insert(_sockets.end(), std::move(sockinfo));
        auto res = _index_by_socket_id.insert({pos->sock.id(), pos});
        assert(res.second);

        return pos;
    }

    void process_connected (socket_id sid)
    {
        auto it = _index_by_socket_id.find(sid);
        assert(it != std::end(_index_by_socket_id));

        auto pos = it->second;
        auto status = pos->sock.state();

        assert(status == socket_type::CONNECTED);

        TRACE_3("Connected to: {} ({}:{})"
            , std::to_string(pos->uuid)
            , std::to_string(pos->saddr.addr)
            , pos->saddr.port);

        _connecting_poller.remove(pos->sock.id());
        _poller.add(pos->sock.id());

        writer_ready(pos->uuid, pos->saddr.addr, pos->saddr.port);
        update_expiration_timepoint(pos->sock.id());
    }

    void connect_to_peer (uuid_t peer_uuid
        , inet4_addr const & addr
        , std::uint16_t port)
    {
        bool is_indexing_socket   = false;
        bool is_connecting_socket = false;
        bool is_connected_socket  = false;

        socket_info sockinfo;

        sockinfo.sock.failure.connect([this] (std::string const & error) {
            this->failure(error);
        });

        if (sockinfo.sock.connect(addr, port)) {
            auto status = sockinfo.sock.state();

            switch (status) {
                case socket_type::CONNECTING: {
                    is_indexing_socket   = true;
                    is_connecting_socket = true;
                    break;
                }

                case socket_type::CONNECTED: {
                    is_indexing_socket  = true;
                    is_connected_socket = true;
                    break;
                }

                default:
                    break;
            }

            if (is_indexing_socket) {
                sockinfo.uuid       = peer_uuid;
                sockinfo.saddr.addr = addr;
                sockinfo.saddr.port = port;

                auto pos = index_socket(std::move(sockinfo));

                // Doesn't matter here whether socket is full functional.
                // Reserve place to avoid possible duplication
                // in `update_peer()` method
                auto res = _writers.insert({pos->uuid, pos});
                assert(res.second);

                if (is_connecting_socket) {
                    TRACE_3("Connecting to: {} ({}:{}), id = {}. Status: {}"
                        , std::to_string(peer_uuid)
                        , std::to_string(pos->saddr.addr)
                        , pos->saddr.port
                        , pos->sock.id()
                        , pos->sock.state_string());

                    _connecting_poller.add(pos->sock.id());
                }

                if (is_connected_socket) {
                    process_connected(pos->sock.id());
                }
            }
        }
    }

    void process_acceptance ()
    {
        socket_info sockinfo;

        sockinfo.sock = _listener.accept(& sockinfo.saddr.addr
            , & sockinfo.saddr.port);

        auto pos = index_socket(std::move(sockinfo));

        TRACE_3("ACCEPT: {} ({}:{}) (status={})"
            , pos->sock.id()
            , std::to_string(pos->saddr.addr)
            , pos->saddr.port
            , pos->sock.state());

        // TODO UNCOMMENT
        //_poller.add(pos->sock.id());
    }

    void close_socket (socket_id sid, bool is_expired)
    {
        auto it = _index_by_socket_id.find(sid);
        assert(it != std::end(_index_by_socket_id));

        auto pos = it->second;

        auto uuid = pos->uuid;
        auto addr = pos->saddr.addr;
        auto port = pos->saddr.port;

        TRACE_1("CLOSE SOCKET: {} ({}:{})"
            , std::to_string(uuid)
            , std::to_string(addr)
            , port);

        _connecting_poller.remove(sid);
        _poller.remove(sid);
        _writers.erase(uuid);
        _index_by_socket_id.erase(sid);
        _sockets.erase(pos); // Socket implicitly closed in socket destructor

        if (is_expired) {
            peer_expired(uuid, addr, port);
        }
    }

    void poll ()
    {
        {
            //auto rc = _connecting_poller.wait(std::chrono::milliseconds{0});
            auto rc = _connecting_poller.wait(std::chrono::milliseconds{1000});

            TRACE_3("--- CONNECTING POLL ({}) ---", rc);

            if (rc > 0) {
                _connecting_poller.process_events(
                    [this] (socket_id sid) {
                        process_connecting(sid);
                    }
                    , [this] (socket_id sid) {
                        process_connecting(sid);
                    }
                );
            }
        }

        {
            auto rc = _poller.wait(_poll_interval);

            TRACE_3("--- POLL ({}) ---", rc);

            // Some events rised
            if (rc > 0) {
                _poller.process_events(
                    [this] (socket_id sid) {
                        if (sid == _listener.id()) {
                            process_listener_event(true);
                        } else {
                            process_socket_event(sid, true);
                        }
                    }
                    , [this] (socket_id sid) {
                        if (sid == _listener.id()) {
                            process_listener_event(false);
                        } else {
                            process_socket_event(sid, false);
                        }
                    }
                );
            }
        }
    }

    void process_connecting (socket_id sid)
    {
        auto it = _index_by_socket_id.find(sid);

        if (it != std::end(_index_by_socket_id)) {
            auto pos = it->second;
            auto status = pos->sock.state();

            if (status == socket_type::CONNECTED)
                process_connected(sid);
        }
    }

    void process_listener_event (bool is_input_event)
    {
        if (is_input_event) {
            // Accept socket (for UDT backend see udt/api.cpp:440)
            process_acceptance();
        } else {
            // There is no significant output events for listener (not yet).
            ;
        }
    }

    void process_socket_event (socket_id sid, bool is_input_event)
    {
        auto it = _index_by_socket_id.find(sid);

        if (it != std::end(_index_by_socket_id)) {
            auto & sockinfo = *it->second;

            TRACE_3("PROCESS SOCKET EVENT: {}: state={}", sid, sockinfo.sock.state());
        } else {
            failure(fmt::format("poll: socket not found by id: {}", sid));
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Discovery phase methods
    ////////////////////////////////////////////////////////////////////////////
    void process_discovery ()
    {
        process_discovery_data();
        broadcast_discovery_data();
        check_expiration();
    }

    void process_discovery_data ()
    {
        _discovery.receiver.process_incoming_data(
            [this] (inet4_addr const & sender_addr, std::uint16_t sender_port
                    , char const * data, std::streamsize size) {

                input_envelope_type in {data, size};
                hello_packet packet;

                auto success = in.unseal(packet);

                if (success) {
                    if (packet.crc16 == crc16_of(packet)) {
                        // Ignore self received packets (can happen during
                        // multicast / broadcast transmission )
                        if (packet.uuid != _uuid) {
                            this->update_peer(packet.uuid, sender_addr, packet.port);
                        }
                    } else {
                        this->failure(fmt::format("bad CRC16 for HELO packet received from: {}:{}"
                            , std::to_string(sender_addr), sender_port));
                    }
                } else {
                    this->failure(fmt::format("bad HELO packet received from: {}:{}"
                        , std::to_string(sender_addr), sender_port));
                }
            });
    }

    void broadcast_discovery_data ()
    {
        bool interval_exceeded = _discovery.last_timepoint
            + _discovery.transmit_interval <= current_timepoint();

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _uuid;
            packet.port = _listener_address.port;

            packet.crc16 = crc16_of(packet);

            output_envelope_type out;
            out.seal(packet);

            auto data = out.data();
            auto size = data.size();

            assert(size == hello_packet::PACKET_SIZE);

            for (auto const & item: _discovery.targets) {
                auto bytes_written = _discovery.transmitter.send(data.data()
                    , size, item.addr, item.port);

                if (bytes_written < 0) {
                    failure(fmt::format("transmit failure to {}:{}: {}"
                        , std::to_string(item.addr)
                        , item.port
                        , _discovery.transmitter.error_string()));
                }
            }

            _discovery.last_timepoint = current_timepoint();
        }
    }

    /**
     * Check expiration
     */
    void check_expiration ()
    {
        auto now = current_timepoint();

        for (auto it = _expiration_timepoints.begin(); it != _expiration_timepoints.end();) {
            if (it->second <= now) {
                auto sid = it->first;
                close_socket(sid, true);
                it = _expiration_timepoints.erase(it);
            } else {
                ++it;
            }
        }
    }

    void update_peer (uuid_t peer_uuid, inet4_addr const & addr, std::uint16_t port)
    {
        auto it = _writers.find(peer_uuid);

        if (it == std::end(_writers)) {
            connect_to_peer(peer_uuid, addr, port);
            rookie_accepted(peer_uuid, addr, port);
        } else {
            auto pos = it->second;
            update_expiration_timepoint(pos->sock.id());
        }
    }

    void update_expiration_timepoint (socket_id sid)
    {
        auto now = current_timepoint();
        auto expiration_timepoint = now + _expiration_timeout;

        auto it = _expiration_timepoints.find(sid);

        if (it != std::end(_expiration_timepoints))
            it->second = expiration_timepoint;
        else
            _expiration_timepoints[sid] = expiration_timepoint;
    }
};

}}} // namespace pfs::net::p2p
