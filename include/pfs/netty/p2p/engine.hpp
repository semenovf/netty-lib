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
#include "trace.hpp"
#include "uuid.hpp"
#include "pfs/fmt.hpp"
#include "pfs/emitter.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/uuid.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include <array>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cassert>

namespace netty {
namespace p2p {

namespace {
constexpr std::chrono::milliseconds DEFAULT_DISCOVERY_INTERVAL { 5000};
constexpr std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {10000};
constexpr std::size_t               DEFAULT_BUFFER_BULK_SIZE   {   64};
constexpr std::size_t               DEFAULT_BUFFER_INC         {  256};
}

template <
      typename DiscoverySocketAPI
    , typename ReliableSocketAPI  // Meets the requirements for reliable and in-order data delivery
    , std::size_t PriorityCount = 1
    , template <typename ...Args> class Emitter = pfs::emitter_mt>
class engine
{
    static_assert(PriorityCount > 0, "PriorityCount must be greater than zero");

public:
    using packet_type = packet;

private:
    using discovery_socket_type = typename DiscoverySocketAPI::socket_type;
    using socket_type           = typename ReliableSocketAPI::socket_type;
    using poller_type           = typename ReliableSocketAPI::poller_type;
    using input_envelope_type   = input_envelope<>;
    using output_envelope_type  = output_envelope<>;
    using socket_id             = decltype(socket_type{}.id());
    using input_queue_type      = pfs::ring_buffer<packet_type, DEFAULT_BUFFER_BULK_SIZE>;
    using output_queue_type     = pfs::ring_buffer<std::pair<uuid_t, packet_type>, DEFAULT_BUFFER_BULK_SIZE>;

    struct socket_address
    {
        inet4_addr    addr;
        std::uint16_t port;
    };

    struct socket_info
    {
        uuid_t         uuid; // Valid for writers (connected sockets as opposite to accepted) only
        socket_type    sock;
        socket_address saddr;
    };

    using socket_container = std::list<socket_info>;
    using socket_iterator  = typename socket_container::iterator;

private:
    std::size_t    _packet_size = packet::MAX_PACKET_SIZE;
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

    input_queue_type _input_queue;
    std::array<output_queue_type, PriorityCount> _output_queues;

    std::vector<socket_id> _expired_sockets;

public:
    mutable Emitter<std::string const &> failure;

    /**
     * Emitted when new writer socket ready (connected).
     */
    mutable Emitter<uuid_t, inet4_addr, std::uint16_t> writer_ready;

    /**
     * Emitted when new address accepted by discoverer.
     */
    mutable Emitter<uuid_t, inet4_addr, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    mutable Emitter<uuid_t, inet4_addr, std::uint16_t> peer_expired;

    /**
     * Message received.
     */
    mutable Emitter<uuid_t, std::string> message_received;

    mutable Emitter<uuid_t, char const * /*data*/
        , std::streamsize /*len*/, int /*priority*/> send;

public:
    static bool startup ()
    {
        return DiscoverySocketAPI::startup()
            && ReliableSocketAPI::startup();
    }

    static void cleanup ()
    {
        DiscoverySocketAPI::cleanup();
        ReliableSocketAPI::cleanup();
    }

public:
    engine (uuid_t uuid)
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

        this->send.connect([this] (uuid_t uuid, char const * data
                , std::streamsize len
                , int priority) {
            this->enqueue(uuid, data, len, priority);
        });

        auto now = current_timepoint();

        _discovery.last_timepoint = now > _discovery.transmit_interval
            ? now - _discovery.transmit_interval : now;
    }

    ~engine ()
    {
        _poller.remove(_listener.id());
    }

    engine (engine const &) = delete;
    engine & operator = (engine const &) = delete;

    engine (engine &&) = default;
    engine & operator = (engine &&) = default;

    uuid_t const & uuid () const noexcept
    {
        return _uuid;
    }

    void set_packet_size (std::size_t size)
    {
        _packet_size = size;
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
                , to_string(c.discovery_address())
                , c.discovery_port()
                , _discovery.receiver.state_string());
            TRACE_1("General listener: {}:{}. Status: {}"
                , to_string(_listener_address.addr)
                , _listener_address.port
                , _listener.state_string());

            auto listener_options = _listener.dump_options();

            TRACE_2("General listener options: id: {}", _listener.id());

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
        delete_expired_sockets();
        poll();
        process_discovery();
        process_incoming_packets();
        send_outgoing_packets();
    }

    void add_discovery_target (inet4_addr const & addr, std::uint16_t port)
    {
        _discovery.targets.emplace_back(socket_address{addr, port});

        if (is_multicast(addr)) {
            auto success = _discovery.receiver.join_multicast_group(addr);

            if (success) {
                TRACE_2("Discovery receiver joined into multicast group: {}"
                    , to_string(addr));
            }
        }
    }

private:
    inline std::chrono::milliseconds current_timepoint ()
    {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::steady_clock;

        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }

    /**
     * Split data to send into packets and enqueue them into output queue.
     */
    void enqueue (uuid_t uuid, char const * data, std::streamsize len, int priority = 0)
    {
        std::size_t prior = priority < 0
            ? 0
            : priority >= _output_queues.size()
                ? _output_queues.size() - 1
                : static_cast<std::size_t>(priority);

        split_into_packets(_packet_size, _uuid, data, len
            , [this, & uuid, prior] (packet_type && p) {
                _output_queues[prior].push(std::make_pair(uuid, std::move(p)));
            });
    }

    socket_info * locate_writer (uuid_t const & uuid)
    {
        socket_info * result {nullptr};
        auto pos = _writers.find(uuid);

        if (pos != _writers.end()) {
            result = & *pos->second;
        } else {
            this->failure(fmt::format("cannot locate writer by UUID: {}"
                , std::to_string(uuid)));
        }

        return result;
    }

    void mark_socket_as_expired (socket_id sid)
    {
        _expired_sockets.push_back(sid);
    }

    void delete_expired_sockets ()
    {
        for (auto sid: _expired_sockets) {
            close_socket(sid);
        }

        _expired_sockets.clear();
    }

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

        TRACE_2("Socket connected to: {} ({}:{}), id: {}. Status: {}"
            , to_string(pos->uuid)
            , to_string(pos->saddr.addr)
            , pos->saddr.port
            , sid
            , pos->sock.state_string());

        auto options = pos->sock.dump_options();

        TRACE_2("Connected socket options: id: {}", sid);

        for (auto const & opt_item: options) {
            TRACE_2("   * {}: {}", opt_item.first, opt_item.second);
        }

        _connecting_poller.remove(pos->sock.id());
        _poller.add(pos->sock.id(), poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);

        this->writer_ready(pos->uuid, pos->saddr.addr, pos->saddr.port);
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
                    TRACE_3("Connecting to: {} ({}:{}), id: {}. Status: {}"
                        , to_string(peer_uuid)
                        , to_string(pos->saddr.addr)
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

        TRACE_2("Socket accepted: {}:{}, id: {}. Status: {}"
            , to_string(pos->saddr.addr)
            , pos->saddr.port
            , pos->sock.id()
            , pos->sock.state_string());

        auto options = pos->sock.dump_options();

        TRACE_2("Accepted socket options: id: {}", pos->sock.id());

        for (auto const & opt_item: options) {
            TRACE_2("   * {}: {}", opt_item.first, opt_item.second);
        }

        _poller.add(pos->sock.id(), poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
    }

    void close_socket (socket_id sid)
    {
        TRACE_1("Socket closing: id: {}", sid);

        auto it = _index_by_socket_id.find(sid);
        assert(it != std::end(_index_by_socket_id));

        auto pos = it->second;

        auto uuid = pos->uuid;
        auto addr = pos->saddr.addr;
        auto port = pos->saddr.port;

        // Remove from pollers before closing socket to avoid infinite error
        _connecting_poller.remove(sid);
        _poller.remove(sid);

        pos->sock.close();

        TRACE_1("Socket closed: {} ({}:{}), id: {}"
            , to_string(uuid)
            , to_string(addr)
            , port
            , sid);

        auto writers_erased = _writers.erase(uuid);
        _index_by_socket_id.erase(sid);
        _sockets.erase(pos);

        if (writers_erased > 0)
            this->peer_expired(uuid, addr, port);
    }

    void poll ()
    {
        {
            auto rc = _connecting_poller.wait(std::chrono::milliseconds{0});

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

            // Expected only connected sockets (writers and accepted) here.
            if (sockinfo.sock.state() != socket_type::CONNECTED) {
                TRACE_3("Mark socket as expired: id: {}. Status: {}"
                    , sid
                    , sockinfo.sock.state_string());
                mark_socket_as_expired(sid);
            }

            if (is_input_event) {
                do {
                    std::array<char, packet_type::MAX_PACKET_SIZE> buf;
                    std::streamsize rc = sockinfo.sock.recv(buf.data(), buf.size());

                    if (rc > 0) {
                        auto pos = it->second;
                        input_envelope_type in {buf.data(), buf.size()};
                        packet_type pkt;

                        in.unseal(pkt);

                        if (pkt.packetsize > packet_type::MAX_PACKET_SIZE) {
                            this->failure(fmt::format("bad packet received from: {}:{}"
                                , to_string(pos->saddr.addr)
                                , pos->saddr.port));
                        } else {
                            _input_queue.push(std::move(pkt));
                        }
                    } else {
                        break;
                    }
                } while (true);
            } else {
                //TRACE_3("PROCESS SOCKET EVENT (OUTPUT): id: {}. Status: {}"
                //    , sid
                //    , sockinfo.sock.state_string());
                ;
            }
        } else {
            this->failure(fmt::format("poll: socket not found by id: {}"
                ", may be it was closed before removing from poller"
                , sid));
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

                in.unseal(packet);
                auto success = is_valid(packet);

                if (success) {
                    if (packet.crc16 == crc16_of(packet)) {
                        // Ignore self received packets (can happen during
                        // multicast / broadcast transmission )
                        if (packet.uuid != _uuid) {
                            this->update_peer(packet.uuid, sender_addr, packet.port);
                        }
                    } else {
                        this->failure(fmt::format("bad CRC16 for HELO packet received from: {}:{}"
                            , to_string(sender_addr), sender_port));
                    }
                } else {
                    this->failure(fmt::format("bad HELO packet received from: {}:{}"
                        , to_string(sender_addr), sender_port));
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
                    this->failure(fmt::format("transmit failure to {}:{}: {}"
                        , to_string(item.addr)
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
                mark_socket_as_expired(sid);
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
            this->rookie_accepted(peer_uuid, addr, port);
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

    void process_incoming_packets ()
    {
        while (!_input_queue.empty()) {
            auto & pkt = _input_queue.front();

            // Most likely there are enough packages to complete the message
            if (pkt.partcount <= _input_queue.size()) {
                auto uuid = pkt.uuid;
                auto expected_partcount = pkt.partcount;
                decltype(pkt.partindex) expected_partindex = 1;

                std::string message;
                message.reserve(pkt.partcount * packet::MAX_PAYLOAD_SIZE);

                bool success = true;

                while (success) {
                    // Check data integrity
                    success = uuid == pkt.uuid
                        && pkt.partcount == expected_partcount
                        && pkt.partindex == expected_partindex++;

                    if (success) {
                        message.append(pkt.payload, pkt.payloadsize);

                        // Pop processed packet from queue.
                        _input_queue.pop();

                        // Message complete, break the loop
                        if (pkt.partindex == pkt.partcount)
                            break;

                        if (!_input_queue.empty())
                            pkt = _input_queue.front();
                        else
                            success = false; // Integrity violated
                    }
                };

                if (success) {
                    this->message_received(uuid, std::move(message));
                } else {
                    // Unexpected behavior!!!
                    // Corrupted/incomplete sequence of packets received.
                    // Need to skip till packet with pkt.partindex == 1.
                    while (!_input_queue.empty() && pkt.partindex != 1) {
                        _input_queue.pop();
                        pkt = _input_queue.front();
                    }

                    this->failure(fmt::format("!!! DATA INTEGRITY VIOLATED:"
                        " corrupted/incomplete sequence of"
                        " packets received from: {}"
                        , std::to_string(uuid)));
                }
            } else {
                break;
            }
        }
    }

    void send_outgoing_packets ()
    {
        std::size_t total_bytes_sent = 0;
        typename output_queue_type::value_type item;
        socket_info * last_sinfo_ptr = nullptr;
        double coeff = 1.0;

        for (std::size_t prior = 0; prior < _output_queues.size(); prior++) {

            // TODO Need more smart of coefficient calculation {{
            std::size_t count = _output_queues[prior].size() * coeff;

            if (_output_queues[prior].size() == 0) {
                continue;
            } else {
                coeff *= 0.75;
            }

            if (count == 0 && _output_queues[prior].size() > 0)
                count = 1; // _output_queues[prior].size();
            // }}

            while (count-- && !_output_queues[prior].empty()) {
                auto & item = _output_queues[prior].front();
                auto const & pkt = item.second;

                output_envelope_type out;
                out.seal(pkt);

                auto data = out.data();
                auto size = data.size();

                _output_queues[prior].pop();

                assert(size <= packet_type::MAX_PACKET_SIZE);

                auto need_locate = !last_sinfo_ptr
                    || last_sinfo_ptr->uuid != item.first;

                if (need_locate) {
                    last_sinfo_ptr = locate_writer(item.first);

                    if (!last_sinfo_ptr)
                        continue;
                }

                assert(item.first == last_sinfo_ptr->uuid);

                auto & sock = last_sinfo_ptr->sock;

                auto bytes_sent = sock.send(data.data(), size);

                if (bytes_sent > 0) {
                    total_bytes_sent += bytes_sent;
                } else if (bytes_sent < 0) {
                    auto & saddr = last_sinfo_ptr->saddr;

                    this->failure(fmt::format("sending failure to {} ({}:{}): {}"
                        , to_string(last_sinfo_ptr->uuid)
                        , to_string(saddr.addr)
                        , saddr.port
                        , sock.error_string()));
                } else {
                    // FIXME Need to handle this state (broken connection ?)
                    ;
                }
            }
        }
    }
};

}} // namespace netty::p2p
