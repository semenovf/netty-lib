////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "engine_traits.hpp"
#include "packet.hpp"
#include "primal_serializer.hpp"
#include "universal_id.hpp"
#include "netty/error.hpp"
#include "netty/host4_addr.hpp"
#include "netty/send_result.hpp"
#include "netty/socket4_addr.hpp"
#include "netty/startup.hpp"
#include "pfs/i18n.hpp"
#include "pfs/optional.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/stopwatch.hpp"
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <vector>

namespace netty {
namespace p2p {

template <typename EngineTraits = default_engine_traits
    , typename Serializer = primal_serializer<>
    , std::uint16_t PACKET_SIZE = packet::MAX_PACKET_SIZE>  // Meets the requirements for reliable and in-order data delivery
class delivery_engine
{
    static_assert(PACKET_SIZE <= packet::MAX_PACKET_SIZE && PACKET_SIZE > packet::PACKET_HEADER_SIZE, "");

private:
    using client_poller_type = typename EngineTraits::client_poller_type;
    using server_poller_type = typename EngineTraits::server_poller_type;
    using listener_type = typename EngineTraits::listener_type;
    using reader_type = typename EngineTraits::reader_type;
    using writer_type = typename EngineTraits::writer_type;
    using output_queue_type = pfs::ring_buffer<packet, 64 * 1024>;

public:
    using serializer_type = Serializer;

public:
    struct options
    {
        socket4_addr listener_saddr;

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        int listener_backlog {100};

        // Fixed C2512 on MSVC 2017
        options () {}
    };

private:
    struct reader_account
    {
        universal_id peer_id;
        reader_type reader;

        // Buffer to accumulate payload from input packets
        std::vector<char> b;

        // Buffer to accumulate raw data
        std::vector<char> raw;
    };

    struct writer_account
    {
        universal_id peer_id;
        writer_type writer;
        bool can_write {false};
        bool connected {false}; // Used to check complete channel

        // Regular packets output queue
        output_queue_type regular_queue;

        // File chunks output queues (mapped by file identifier).
        std::map<universal_id, output_queue_type> chunks;

        // Serialized (raw) data to send.
        std::vector<char> raw;
    };

private:
    // Host (listener) identifier.
    universal_id _host_id;

    options _opts;

    std::unique_ptr<listener_type> _listener;
    std::unique_ptr<server_poller_type> _reader_poller;
    std::unique_ptr<client_poller_type> _writer_poller;

    std::map<typename server_poller_type::native_socket_type, reader_account> _reader_account_map;
    std::map<universal_id, writer_account> _writer_account_map;

public: // Callbacks
    /**
     * Called when unrecoverable error occurred - engine became disfunctional.
     * It must be restarted.
     */
    mutable std::function<void (error const & err)> on_failure = [] (error const & err) {};

    mutable std::function<void (std::string const &)> on_error = [] (std::string const &) {};
    mutable std::function<void (std::string const &)> on_warn = [] (std::string const &) {};

    /**
     * Called when need to force peer expiration by discovery manager (using expire_peer method)
     * Avoid to call discovery manager::expire_peer() immediately.
     */
    mutable std::function<void (universal_id)> defere_expire_peer;

    /**
     * Called when new writer socket ready (connected).
     */
    mutable std::function<void (host4_addr)> writer_ready = [] (host4_addr) {};

    /**
     * Called when writer socket closed/disconnected.
     */
    mutable std::function<void (host4_addr)> writer_closed = [] (host4_addr) {};

    /**
     * Called when new reader socket ready (handshaked).
     */
    mutable std::function<void (host4_addr)> reader_ready = [] (host4_addr) {};

    /**
     * Called when reader socket closed/disconnected.
     */
    mutable std::function<void (host4_addr)> reader_closed = [] (host4_addr) {};

    /**
     * Called when channel (reader and writer available) established.
     */
    mutable std::function<void (host4_addr)> channel_established = [] (host4_addr) {};

    /**
     * Message received.
     */
    mutable std::function<void (universal_id, std::vector<char>)> data_received
        = [] (universal_id, std::vector<char>) {};

    /**
     * Called when channel closed.
     */
    mutable std::function<void (universal_id)> channel_closed = [] (universal_id) {};

    /**
     * Called when any file data received. These data must be passed to the file transporter
     */
    mutable std::function<void (universal_id, packet_type_enum, std::vector<char>)> file_data_received
        = [] (universal_id, packet_type_enum packettype, std::vector<char> const &) {};

    /**
     * Called to request new file chunks for sending.
     */
    mutable std::function<void (universal_id, universal_id)> request_file_chunk
        = [] (universal_id addressee, universal_id fileid) {};

public:
    /**
     * Initializes underlying APIs and constructs delivery engine instance.
     *
     * @param host_id Unique host identifier for this instance.
     */
    delivery_engine (universal_id host_id, options && opts, netty::error * perr = nullptr)
        : _host_id(host_id)
        , _opts(std::move(opts))
    {
        ////////////////////////////////////////////////////////////////////////
        // Create and configure main server poller
        ////////////////////////////////////////////////////////////////////////
        auto accept_proc = [this] (typename server_poller_type::native_socket_type listener_sock, bool & success) {
            auto * areader = accept_reader_account(listener_sock);

            if (areader == nullptr) {
                success = false;
                return areader->reader.kINVALID_SOCKET;
            } else {
                success = true;
            }

            return areader->reader.native();
        };

        _reader_poller = pfs::make_unique<server_poller_type>(std::move(accept_proc));

        _reader_poller->on_listener_failure = [this] (typename server_poller_type::native_socket_type, error const & err) {
            this->on_failure(err);
        };

        _reader_poller->on_reader_failure = [this] (typename server_poller_type::native_socket_type sock, error const & err) {
            auto areader = locate_reader_account(sock);

            if (areader != nullptr && areader->peer_id != universal_id{}) {
                defere_expire_peer(areader->peer_id);
            } else {
                on_error(tr::f_("reader socket failure: socket={}, but reader account not found", sock));
                this->on_failure(err);
            }
        };

        _reader_poller->ready_read = [this] (typename server_poller_type::native_socket_type sock) {
            process_reader_input(sock);
        };

        _reader_poller->disconnected = [this] (typename server_poller_type::native_socket_type sock) {
            auto areader = locate_reader_account(sock);

            if (areader != nullptr && areader->peer_id != universal_id{}) {
                defere_expire_peer(areader->peer_id);
            } else {
                on_error(tr::f_("reader socket disconnected: socket={}, but reader account not found", sock));
            }
        };

        ////////////////////////////////////////////////////////////////////////
        // Create and configure writer poller
        ////////////////////////////////////////////////////////////////////////
        _writer_poller = pfs::make_unique<client_poller_type>();

        _writer_poller->on_failure = [this] (typename client_poller_type::native_socket_type sock, error const & err) {
            auto awriter = locate_writer_account(sock);

            if (awriter) {
                defere_expire_peer(awriter->peer_id);
            } else {
                on_error(tr::f_("reader socket failure: socket={}, but writer account not found", sock));
                this->on_failure(err);
            }
        };

        _writer_poller->connection_refused = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                defere_expire_peer(awriter->peer_id);
            } else {
                on_error(tr::f_("connection refused: socket={}, but writer account not found", sock));
            }
        };

        _writer_poller->connected = [this] (typename client_poller_type::native_socket_type sock) {
            process_socket_connected(sock);
        };

        _writer_poller->disconnected = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                defere_expire_peer(awriter->peer_id);
            } else {
                on_error(tr::f_("connection disconnected: socket={}, but writer account not found", sock));
            }
        };

        // Not need, writer sockets for write only
        _writer_poller->ready_read = [] (typename client_poller_type::native_socket_type) {};

        _writer_poller->can_write = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                awriter->can_write = true;
            } else {
                on_error(tr::f_("writer can write: socket={}, but writer account not found", sock));
            }
        };

        // Call befor any network operations
        startup();

        _listener = pfs::make_unique<listener_type>(_opts.listener_saddr);

        netty::error err;
        _reader_poller->add_listener(*_listener, & err);

        if (err) {
            pfs::throw_or(perr, std::move(err));
            return;
        }

        _listener->listen(_opts.listener_backlog, & err);

        if (err) {
            pfs::throw_or(perr, std::move(err));
            return;
        }
    }

    ~delivery_engine ()
    {
        release_peers();
        _listener.reset();
        _reader_poller.reset();
        _writer_poller.reset();
        cleanup();
    }

    delivery_engine (delivery_engine const &) = delete;
    delivery_engine & operator = (delivery_engine const &) = delete;

    delivery_engine (delivery_engine &&) = default;
    delivery_engine & operator = (delivery_engine &&) = default;

    universal_id host_id () const noexcept
    {
        return _host_id;
    }

    void connect (host4_addr haddr)
    {
        connect_writer(std::move(haddr));
    }

    void release_peer (universal_id peer_id)
    {
        release_reader(peer_id);
        release_writer(peer_id);
        channel_closed(peer_id);
    }

    void release_peers ()
    {
        std::vector<universal_id> peers;
        std::accumulate(_writer_account_map.cbegin(), _writer_account_map.cend()
            , & peers, [] (std::vector<universal_id> * peers, decltype(*_writer_account_map.cbegin()) x) {
                peers->push_back(x.first);
                return peers;
            });

        for (auto const & peer_id: peers)
            release_peer(peer_id);
    }

    // FIXME
    // void set_open_outcome_file (std::function<file (std::string const & /*path*/)> && f)
    // {
    //     _transporter->open_outcome_file = std::move(f);
    // }

private:
    pfs::stopwatch<std::milli> _stopwatch;

public:
    int step (std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
    {
        int n1 = 0;
        int n2 = 0;
        auto zero_millis = std::chrono::milliseconds{0};

        if (timeout > zero_millis) {
            _stopwatch.start();
            send_outgoing_packets();
            _stopwatch.stop();

            timeout -= std::chrono::milliseconds{_stopwatch.count()};

            if(timeout < zero_millis)
                timeout = zero_millis;

            _stopwatch.start();
            n1 = _reader_poller->poll(timeout);
            _stopwatch.stop();

            timeout -= std::chrono::milliseconds{_stopwatch.count()};

            if(timeout < zero_millis)
                timeout = zero_millis;

            n2 = _writer_poller->poll(timeout);
        } else {
            send_outgoing_packets();
            n1 = _reader_poller->poll(zero_millis);
            n2 = _writer_poller->poll(zero_millis);
        }

        if (n1 < 0)
            n1 = 0;

        if (n2 < 0)
            n2 = 0;

        return n1 + n2;
    }

    /**
     * Splits data to send into packets and enqueue them into output queue.
     *
     * @param addressee_id Addressee unique identifier.
     * @param data Data to send.
     * @param len  Data length.
     *
     * @return Unique message identifier.
     */
    bool enqueue (universal_id addressee, char const * data, int len)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data, len);
    }

    bool enqueue (universal_id addressee, std::string const & data)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data.data(), data.size());
    }

    bool enqueue (universal_id addressee, std::vector<char> const & data)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data.data(), data.size());
    }

    /**
     * Process file upload stopped event from file transporter
     */
    void file_upload_stopped (universal_id addressee, universal_id fileid)
    {
        auto awriter = locate_writer_account(addressee);

        if (awriter == nullptr) {
            on_error(tr::f_("file upload stopped/finished, but writer not found: addressee={}, fileid={}"
                , addressee, fileid));
            return;
        }

        awriter->chunks.erase(fileid);
    }

    void file_upload_complete (universal_id addressee, universal_id fileid)
    {
        file_upload_stopped(addressee, fileid);
    }

    void file_ready_send (universal_id addressee, universal_id fileid, packet_type_enum packettype
        , typename serializer_type::output_archive_type data)
    {
        switch (packettype) {
            // Commands
            case packet_type_enum::file_credentials:
            case packet_type_enum::file_request:
            case packet_type_enum::file_stop:
            case packet_type_enum::file_state:
                LOG_TRACE_3("Enqueue file control packets to: {}", addressee);
                enqueue_packets(addressee, packettype, data.data(), pfs::numeric_cast<int>(data.size()));
                break;
            // Data
            case packet_type_enum::file_begin:
            case packet_type_enum::file_end:
            case packet_type_enum::file_chunk:
                enqueue_file_chunk(addressee, fileid, packettype, data.data(), pfs::numeric_cast<int>(data.size()));
                break;
            default:
                return;
        }
    }

private:
    reader_account * accept_reader_account (typename server_poller_type::native_socket_type listener_sock)
    {
        netty::error err;
        auto reader = _listener->accept_nonblocking(listener_sock, & err);

        if (err) {
            on_error(tr::f_("accept connection failure: {}", err.what()));
            return nullptr;
        }

        auto & areader = _reader_account_map[reader.native()];
        areader.reader = std::move(reader);
        areader.raw.clear();
        areader.raw.reserve(64 * 1024);
        areader.b.clear();

        return & areader;
    }

    reader_account * locate_reader_account (typename server_poller_type::native_socket_type sock)
    {
        auto pos = _reader_account_map.find(sock);

        if (pos == _reader_account_map.end()) {
            on_warn(tr::f_("no reader account located by socket: {}", sock));
            return nullptr;
        }

        return & pos->second;
    }

    reader_account * locate_reader_account (universal_id peer_id)
    {
        for (auto & r: _reader_account_map) {
            if (r.second.peer_id == peer_id)
                return & r.second;
        }

        on_warn(tr::f_("no reader account located by peer ID: {}", peer_id));
        return nullptr;
    }

    writer_account * locate_writer_account (typename client_poller_type::native_socket_type sock)
    {
        for (auto & w: _writer_account_map) {
            if (w.second.writer.native() == sock)
                return & w.second;
        }

        on_warn(tr::f_("no writer account located by socket: {}", sock));
        return nullptr;
    }

    writer_account * locate_writer_account (universal_id peer_id)
    {
        auto pos = _writer_account_map.find(peer_id);

        if (pos == _writer_account_map.end()) {
            on_warn(tr::f_("no writer account located by peer ID: {}", peer_id));
            return nullptr;
        }

        return & pos->second;
    }

    void release_reader (universal_id peer_id)
    {
        auto areader = locate_reader_account(peer_id);

        if (areader == nullptr) {
            on_error(tr::f_("no reader found for release: {}", peer_id));
            return;
        };

        auto saddr = areader->reader.saddr();

        if (_reader_poller)
            _reader_poller->remove(areader->reader);

        _reader_account_map.erase(areader->reader.native());

        reader_closed(host4_addr{peer_id, std::move(saddr)});
    }

    writer_account & acquire_writer_account (universal_id peer_id)
    {
        auto & awriter = _writer_account_map[peer_id];
        awriter.peer_id   = peer_id;
        awriter.can_write = false;
        awriter.connected = false;
        awriter.regular_queue.clear();
        awriter.raw.clear();
        awriter.raw.reserve(PACKET_SIZE * 10);
        awriter.chunks.clear();

        return awriter;
    }

    void connect_writer (host4_addr haddr)
    {
        netty::error err;
        writer_type writer;

        auto conn_state = writer.connect(haddr.saddr);
        _writer_poller->add(writer, conn_state, & err);

        if (!err) {
            auto & awriter = acquire_writer_account(haddr.host_id);
            awriter.writer = std::move(writer);
        } else {
            on_error(tr::f_("connecting writer failure: {}: {}, writer ignored"
                , to_string(haddr), err.what()));
        }
    }

    // Release writer by peer identifier
    void release_writer (universal_id peer_id)
    {
        auto awriter = locate_writer_account(peer_id);

        if (awriter == nullptr) {
            on_error(tr::f_("no writer found for release: {}", peer_id));
            return;
        }

        auto saddr = awriter->writer.saddr();

        if (_writer_poller)
            _writer_poller->remove(awriter->writer);

        _writer_account_map.erase(peer_id);

        writer_closed(host4_addr{peer_id, std::move(saddr)});
    }

    void check_complete_channel (universal_id peer_id)
    {
        int complete = 0;

        auto areader = locate_reader_account(peer_id);

        if (areader != nullptr && areader->peer_id != universal_id{})
            complete++;

        auto awriter = locate_writer_account(peer_id);

        if (awriter != nullptr && awriter->connected)
            complete++;

        if (complete == 2)
            channel_established(host4_addr{peer_id, awriter->writer.saddr()});
    }

    inline void enqueue_packets_helper (output_queue_type & q, universal_id addressee
        , packet_type_enum packettype, char const * data, int len)
    {
        netty::p2p::enqueue_packets<output_queue_type>(q, _host_id, addressee
            , packettype, PACKET_SIZE, data, len);
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    bool enqueue_packets (universal_id addressee, packet_type_enum packettype
        , char const * data, int len)
    {
        auto * awriter = locate_writer_account(addressee);

        if (awriter == nullptr)
            return false;

        enqueue_packets_helper(awriter->regular_queue, addressee, packettype, data, len);
        return true;
    }

    bool enqueue_file_chunk (universal_id addressee, universal_id fileid, packet_type_enum packettype
        , char const * data, int len)
    {
        auto * awriter = locate_writer_account(addressee);

        if (awriter == nullptr)
            return false;

        auto pos = awriter->chunks.find(fileid);

        if (pos == awriter->chunks.end()) {
            auto res = awriter->chunks.emplace(fileid, output_queue_type{});
            pos = res.first;
        }

        enqueue_packets_helper(pos->second, addressee, packettype, data, len);
        return true;
    }

    void process_socket_connected (typename client_poller_type::native_socket_type sock)
    {
        auto awriter = locate_writer_account(sock);

        if (awriter == nullptr) {
            on_error(tr::f_("socket connected, but writer not found: socket={}", sock));
            return;
        }

        _writer_poller->wait_for_write(awriter->writer);
        awriter->connected = true;

        writer_ready(host4_addr{awriter->peer_id, awriter->writer.saddr()});

        // Only addresser need by the receiver
        enqueue_packets(awriter->peer_id, packet_type_enum::hello, "HELO", 4);

        check_complete_channel(awriter->peer_id);
    }

    void process_reader_input (typename server_poller_type::native_socket_type sock)
    {
        auto areader = locate_reader_account(sock);

        if (!areader)
            return;

        auto & buffer = areader->raw;

        while (buffer.size() < PACKET_SIZE) {
            auto available = areader->reader.available();

            auto offset = buffer.size();
            buffer.resize(offset + available);

            auto n = areader->reader.recv(buffer.data() + offset, available);

            if (n < 0) {
                on_error(tr::f_("receive data failure from: {}", to_string(areader->reader.saddr())));
                defere_expire_peer(areader->peer_id);
                return;
            }

            buffer.resize(offset + n);

            if (n == 0)
                break;

            if (n < PACKET_SIZE)
                break;
        }

        auto available = buffer.size();

        if (available < PACKET_SIZE)
            return;

        auto buffer_pos = buffer.begin();

        do {
            typename Serializer::istream_type in {& *buffer_pos, PACKET_SIZE};
            available -= PACKET_SIZE;
            buffer_pos += PACKET_SIZE;

            static_assert(sizeof(packet_type_enum) <= sizeof(char), "");

            auto packettype = static_cast<packet_type_enum>(*in.peek());
            decltype(packet::packetsize) packetsize {0};

            if (!is_valid(packettype)) {
                on_error(tr::f_("unexpected packet type ({}) received from: {}, ignored."
                    , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                    , to_string(areader->reader.saddr())));

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                defere_expire_peer(areader->peer_id);

                buffer_pos = buffer.end();
                break;
            }

            packet pkt;
            in >> pkt;
            packetsize = pkt.packetsize;

            if (packetsize != PACKET_SIZE) {
                on_error(tr::f_("unexpected packet size ({}) received from: {}, expected: {}"
                    , PACKET_SIZE
                    , to_string(areader->reader.saddr())
                    , packetsize));

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                defere_expire_peer(areader->peer_id);

                buffer_pos = buffer.end();
                break;
            }

            // Start new sequence (message)
            if (pkt.partindex == 1)
                areader->b.clear();

            if (pkt.payloadsize > 0)
                areader->b.insert(areader->b.end(), pkt.payload, pkt.payload + pkt.payloadsize);

            // Message complete
            if (pkt.partindex == pkt.partcount) {
                auto peer_id = pkt.addresser;

                switch (packettype) {
                    case packet_type_enum::regular:
                        this->data_received(peer_id, std::move(areader->b));
                        break;

                    case packet_type_enum::hello: {
                        // Complete reader account
                        areader->peer_id = peer_id;
                        reader_ready(host4_addr{peer_id, areader->reader.saddr()});
                        check_complete_channel(peer_id);
                        break;
                    }

                    case packet_type_enum::file_credentials:
                    case packet_type_enum::file_request:
                    case packet_type_enum::file_stop:
                    case packet_type_enum::file_chunk:
                    case packet_type_enum::file_begin:
                    case packet_type_enum::file_end:
                    case packet_type_enum::file_state:
                        file_data_received(peer_id, packettype, std::move(areader->b));
                        break;
                }
            }
        } while (available >= PACKET_SIZE);

        if (available == 0)
            buffer.clear();
        else
            buffer.erase(buffer.begin(), buffer_pos);
    }

    /**
     * Serializes packets to send.
     *
     * @param raw Buffer to store packets as raw bytes before sending.
     * @param output_queue Queue that stores output packets.
     * @param limit Number of messages/chunks to store as contiguous sequence
     *        of bytes.
     */
    void serialize_outgoing_packets (std::vector<char> & raw, output_queue_type & q, int limit)
    {
        typename Serializer::ostream_type out;

        while (limit && !q.empty()) {
            auto const & pkt = q.front();

            if (pkt.partindex == pkt.partcount)
                --limit;

            out.reset(); // Empty envelope
            out << pkt;  // Pack new data

            auto data = out.data();
            auto size = out.size();
            auto offset = raw.size();
            raw.resize(offset + size);
            std::memcpy(raw.data() + offset, data, size);
            q.pop();
        }
    }

    void send_outgoing_data (writer_account & awriter)
    {
        std::size_t total_bytes_sent = 0;

        error err;
        bool break_sending = false;

        // TODO Implement throttling
        // std::this_thread::sleep_for(std::chrono::milliseconds{10});

        while (!break_sending && !awriter.raw.empty()) {
            int nbytes_to_send = PACKET_SIZE * 10;

            if (nbytes_to_send > awriter.raw.size())
                nbytes_to_send = static_cast<int>(awriter.raw.size());

            auto sendresult = awriter.writer.send(awriter.raw.data(), nbytes_to_send, & err);

            switch (sendresult.state) {
                case netty::send_status::failure:
                    on_error(tr::f_("send failure to {}: {}", to_string(awriter.writer.saddr())
                        , err.what()));

                    // Expire peer
                    defere_expire_peer(awriter.peer_id);
                    break_sending = true;
                    break;

                case netty::send_status::network:
                    on_error(tr::f_("send failure to {}: network failure: {}"
                        , to_string(awriter.writer.saddr()), err.what()));

                    // Expire peer
                    defere_expire_peer(awriter.peer_id);
                    break_sending = true;
                    break;

                case netty::send_status::again:
                case netty::send_status::overflow:
                    if (awriter.can_write != false) {
                        awriter.can_write = false;
                        _writer_poller->wait_for_write(awriter.writer);
                    }

                    break_sending = true;
                    break;

                case netty::send_status::good:
                    if (sendresult.n > 0) {
                        total_bytes_sent += sendresult.n;
                        awriter.raw.erase(awriter.raw.begin(), awriter.raw.begin() + sendresult.n);
                    }

                    break;
            }
        }
    }

    void send_outgoing_packets ()
    {
        for (auto & w: _writer_account_map) {
            auto & awriter = w.second;

            if (!awriter.can_write)
                continue;

            // Serialize (bufferize) packets to send
            if (awriter.raw.size() < PACKET_SIZE) {

                // Serialize non-file_chunk (priority) packets.
                if (!awriter.regular_queue.empty())
                    serialize_outgoing_packets(awriter.raw, awriter.regular_queue, 10);

                if (!awriter.chunks.empty()) {
                    auto pos  = awriter.chunks.begin();
                    auto last = awriter.chunks.end();

                    while (pos != last) {
                        output_queue_type & chunks_output_queue = pos->second;

                        if (!chunks_output_queue.empty()) {
                            serialize_outgoing_packets(awriter.raw, chunks_output_queue, 10);
                            ++pos;
                        } else {
                            request_file_chunk(awriter.peer_id, pos->first);
                            ++pos;
                        }
                    }
                }
            }

            // Send serialized (bufferized) data
            if (!awriter.raw.empty())
                send_outgoing_data(awriter);
        }
    }
};

}} // namespace netty::p2p

