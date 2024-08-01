////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.16 Initial version.
//      2024.07.30 Add Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "engine_traits.hpp"
#include "delivery_functional_callbacks.hpp"
#include "packet.hpp"
#include "primal_serializer.hpp"
#include "universal_id.hpp"
#include <pfs/netty/error.hpp>
#include <pfs/netty/host4_addr.hpp>
#include <pfs/netty/property.hpp>
#include <pfs/netty/send_result.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/startup.hpp>
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <pfs/ring_buffer.hpp>
#include <pfs/stopwatch.hpp>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <vector>

#include <pfs/log.hpp>

namespace netty {
namespace p2p {

template <typename EngineTraits
    , typename Callbacks = delivery_functional_callbacks
    , typename Serializer = primal_serializer<>
    , std::uint16_t PACKET_SIZE = packet::MAX_PACKET_SIZE>  // Meets the requirements for reliable and in-order data delivery
class delivery_engine: public Callbacks
{
    static_assert(PACKET_SIZE <= packet::MAX_PACKET_SIZE && PACKET_SIZE > packet::PACKET_HEADER_SIZE, "");

private:
    using client_poller_type = typename EngineTraits::client_poller_type;
    using server_poller_type = typename EngineTraits::server_poller_type;
    using listener_type = typename EngineTraits::listener_type;
    using reader_type = typename EngineTraits::reader_type;
    using writer_type = typename EngineTraits::writer_type;
    using output_queue_type = pfs::ring_buffer<packet, 64 * 1024>;
    using expired_peers_queue_type = std::queue<peer_id>;

public:
    using file_id_type = universal_id;
    using serializer_type = Serializer;

public:
    struct options
    {
        socket4_addr listener_saddr;

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        int listener_backlog {100};

        property_map_t listener_props;

        // Fixed C2512 on MSVC 2017
        options () {}
    };

private:
    struct reader_account
    {
        peer_id peerid;
        reader_type reader {uninitialized{}};

        // Buffer to accumulate payload from input packets
        std::vector<char> b;

        // Buffer to accumulate raw data
        std::vector<char> raw;
    };

    struct writer_account
    {
        peer_id peerid;
        writer_type writer {uninitialized{}};
        bool can_write {false};
        bool connected {false}; // Used to check complete channel

        // Regular packets output queue
        output_queue_type regular_queue;

        // File chunks output queues (mapped by file identifier).
        std::map<file_id_type, output_queue_type> chunks;

        // Serialized (raw) data to send.
        std::vector<char> raw;
    };

private:
    // Host (listener) identifier.
    peer_id _host_id;

    options _opts;

    std::unique_ptr<listener_type> _listener;
    std::unique_ptr<server_poller_type> _reader_poller;
    std::unique_ptr<client_poller_type> _writer_poller;

    std::map<typename server_poller_type::native_socket_type, reader_account> _reader_account_map;
    std::map<peer_id, writer_account> _writer_account_map;

    expired_peers_queue_type _expired_peers;

public:
    /**
     * Initializes underlying APIs and constructs delivery engine instance.
     *
     * @param host_id Unique host identifier for this instance.
     */
    delivery_engine (peer_id host_id, options && opts, netty::error * perr = nullptr)
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

        _reader_poller->on_failure = [this] (typename server_poller_type::native_socket_type sock, error const & err) {
            auto areader = locate_reader_account(sock);

            if (areader != nullptr && areader->peerid != peer_id{}) {
                Callbacks::defere_expire_peer(areader->peerid);
            } else {
                Callbacks::on_error(tr::f_("reader socket failure: socket={}, but reader account is incomplete yet", sock));
                this->on_failure(err);
            }
        };

        _reader_poller->ready_read = [this] (typename server_poller_type::native_socket_type sock) {
            process_reader_input(sock);
        };

        _reader_poller->disconnected = [this] (typename server_poller_type::native_socket_type sock) {
            auto areader = locate_reader_account(sock);

            if (areader != nullptr && areader->peerid != peer_id{}) {
                Callbacks::defere_expire_peer(areader->peerid);
            } else {
                Callbacks::on_warn(tr::f_("reader disconnected: socket={}, but reader account is incomplete yet", sock));
            }
        };

        _reader_poller->removed = [this] (typename server_poller_type::native_socket_type sock) {
            // Actually destroy socket here
            _reader_account_map.erase(sock);
        };

        ////////////////////////////////////////////////////////////////////////
        // Create and configure writer poller
        ////////////////////////////////////////////////////////////////////////
        _writer_poller = pfs::make_unique<client_poller_type>();

        _writer_poller->on_failure = [this] (typename client_poller_type::native_socket_type sock, error const & err) {
            auto awriter = locate_writer_account(sock);

            if (awriter) {
                Callbacks::defere_expire_peer(awriter->peerid);
            } else {
                Callbacks::on_error(tr::f_("writer socket failure: socket={}, but writer account not found", sock));
                this->on_failure(err);
            }
        };

        _writer_poller->connection_refused = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                Callbacks::on_error(tr::f_("connection refused: {}, expire peer", awriter->peerid));
                Callbacks::defere_expire_peer(awriter->peerid);
            } else {
                Callbacks::on_error(tr::f_("connection refused: socket={}, but writer account not found", sock));
            }
        };

        _writer_poller->connected = [this] (typename client_poller_type::native_socket_type sock) {
            process_socket_connected(sock);
        };

        _writer_poller->disconnected = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                Callbacks::defere_expire_peer(awriter->peerid);
            } else {
                Callbacks::on_error(tr::f_("connection disconnected: socket={}, but writer account not found", sock));
            }
        };

        // Not need, writer sockets for write only
        _writer_poller->ready_read = [] (typename client_poller_type::native_socket_type) {};

        _writer_poller->can_write = [this] (typename client_poller_type::native_socket_type sock) {
            auto awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                awriter->can_write = true;
            } else {
                Callbacks::on_error(tr::f_("writer can write: socket={}, but writer account not found", sock));
            }
        };

        _writer_poller->removed = [this] (typename client_poller_type::native_socket_type sock) {
            // Actually destroy socket here
            writer_account * awriter = locate_writer_account(sock);

            if (awriter != nullptr) {
                _writer_account_map.erase(awriter->peerid);
            } else {
                Callbacks::on_error(tr::f_("no writer account found by socket for release: socket={}", sock));
            }
        };

        // Call before any network operations
        startup();

        netty::error err;

        _listener = pfs::make_unique<listener_type>(_opts.listener_saddr
            /*, _opts.listener_backlog*/, _opts.listener_props, & err);

        if (!err)
            _reader_poller->add_listener(*_listener, & err);

        if (!err)
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

    /**
     * Call this method before main loop to complete engine configuration.
     */
    void ready ()
    {}

    peer_id host_id () const noexcept
    {
        return _host_id;
    }

    void connect (host4_addr haddr)
    {
        connect_writer(std::move(haddr));
    }

    void expire_peer (peer_id peerid)
    {
        _expired_peers.push(peerid);
    }

private:
    void release_expired_peers ()
    {
        while (!_expired_peers.empty()) {
            auto peerid = _expired_peers.front();
            _expired_peers.pop();
            release_peer(peerid);
        }
    }

    void release_peer (peer_id peerid)
    {
        release_reader(peerid);
        release_writer(peerid);
        Callbacks::channel_closed(peerid);
    }

    void release_peers ()
    {
        std::vector<peer_id> peers;
        std::accumulate(_writer_account_map.cbegin(), _writer_account_map.cend()
            , & peers, [] (std::vector<peer_id> * peers, decltype(*_writer_account_map.cbegin()) x) {
                peers->push_back(x.first);
                return peers;
            });

        for (auto const & peerid: peers)
            release_peer(peerid);
    }

    // FIXME
    // void set_open_outcome_file (std::function<file (std::string const & /*path*/)> && f)
    // {
    //     _transporter->open_outcome_file = std::move(f);
    // }

private:
    pfs::stopwatch<std::milli> _stopwatch;

public:
    int step (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
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
            n1 = _reader_poller->poll(timeout, perr);
            _stopwatch.stop();

            timeout -= std::chrono::milliseconds{_stopwatch.count()};

            if(timeout < zero_millis)
                timeout = zero_millis;

            n2 = _writer_poller->poll(timeout, perr);

            if (!_expired_peers.empty())
                release_expired_peers();
        } else {
            send_outgoing_packets();
            n1 = _reader_poller->poll(zero_millis, perr);
            n2 = _writer_poller->poll(zero_millis, perr);

            if (!_expired_peers.empty())
                release_expired_peers();
        }

        if (n1 < 0)
            n1 = 0;

        if (n2 < 0)
            n2 = 0;

        return n1 + n2;
    }

    std::chrono::microseconds step_timing (std::chrono::milliseconds poll_timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        pfs::stopwatch<std::micro> stopwatch;
        stopwatch.start();
        step(poll_timeout, perr);
        stopwatch.stop();
        return std::chrono::microseconds{stopwatch.count()};
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
    bool enqueue (peer_id addressee, char const * data, int len)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data, len);
    }

    bool enqueue (peer_id addressee, std::string const & data)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data.data(), data.size());
    }

    bool enqueue (peer_id addressee, std::vector<char> const & data)
    {
        return enqueue_packets(addressee, packet_type_enum::regular, data.data(), data.size());
    }

    /**
     * Process file upload stopped event from file transporter
     */
    void file_upload_stopped (peer_id addressee, file_id_type fileid)
    {
        auto awriter = locate_writer_account(addressee);

        if (awriter == nullptr) {
            Callbacks::on_error(tr::f_("file upload stopped/finished, but writer not found: addressee={}, fileid={}"
                , addressee, fileid));
            return;
        }

        awriter->chunks.erase(fileid);
    }

    void file_upload_complete (peer_id addressee, file_id_type fileid)
    {
        file_upload_stopped(addressee, fileid);
    }

    void file_ready_send (peer_id addressee, file_id_type fileid, packet_type_enum packettype
        , typename serializer_type::output_archive_type data)
    {
        switch (packettype) {
            // Commands
            case packet_type_enum::file_credentials:
            case packet_type_enum::file_request:
            case packet_type_enum::file_stop:
            case packet_type_enum::file_state:
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

    /**
     * Iterates over writers applying @a f (peer_id) to each writer.
     */
    template <typename F>
    void broadcast (F && f)
    {
        for (auto & pair: _writer_account_map) {
            f(pair.first);
        }
    }

private:
    reader_account * accept_reader_account (typename server_poller_type::native_socket_type listener_sock)
    {
        netty::error err;
        auto reader = _listener->accept_nonblocking(listener_sock, & err);

        if (err) {
            Callbacks::on_error(tr::f_("accept connection failure: {}", err.what()));
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

        if (pos == _reader_account_map.end())
            return nullptr;

        return & pos->second;
    }

    reader_account * locate_reader_account (peer_id peerid)
    {
        for (auto & r: _reader_account_map) {
            if (r.second.peerid == peerid)
                return & r.second;
        }

        return nullptr;
    }

    writer_account * locate_writer_account (typename client_poller_type::native_socket_type sock)
    {
        for (auto & w: _writer_account_map) {
            if (w.second.writer.native() == sock)
                return & w.second;
        }

        return nullptr;
    }

    writer_account * locate_writer_account (peer_id peerid)
    {
        auto pos = _writer_account_map.find(peerid);

        if (pos == _writer_account_map.end())
            return nullptr;

        return & pos->second;
    }

    void release_reader (peer_id peerid)
    {
        auto areader = locate_reader_account(peerid);

        if (areader == nullptr) {
            Callbacks::on_error(tr::f_("no reader account found for release: {}", peerid));
            return;
        };

        auto saddr = areader->reader.saddr();

        if (_reader_poller)
            _reader_poller->remove(areader->reader);

        // Moved to _reader_poller removed() callback
        // _reader_account_map.erase(areader->reader.native());

        Callbacks::reader_closed(host4_addr{peerid, std::move(saddr)});
    }

    writer_account & acquire_writer_account (peer_id peerid)
    {
        auto & awriter = _writer_account_map[peerid];
        awriter.peerid = peerid;
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
            Callbacks::on_error(tr::f_("connecting writer failure: {}: {}, writer ignored"
                , to_string(haddr), err.what()));
        }
    }

    // Release writer by peer identifier
    void release_writer (peer_id peerid)
    {
        auto awriter = locate_writer_account(peerid);

        if (awriter == nullptr) {
            Callbacks::on_error(tr::f_("no writer found for release: {}", peerid));
            return;
        }

        auto saddr = awriter->writer.saddr();

        if (_writer_poller)
            _writer_poller->remove(awriter->writer);

        // Moved to _writer_poller removed() callback
        // _writer_account_map.erase(peerid);

        Callbacks::writer_closed(host4_addr{peerid, std::move(saddr)});
    }

    void check_complete_channel (peer_id peerid)
    {
        int complete = 0;

        auto areader = locate_reader_account(peerid);

        if (areader != nullptr && areader->peerid != peer_id{})
            complete++;

        auto awriter = locate_writer_account(peerid);

        if (awriter != nullptr && awriter->connected)
            complete++;

        if (complete == 2)
            Callbacks::channel_established(host4_addr{peerid, awriter->writer.saddr()});
    }

    inline void enqueue_packets_helper (output_queue_type & q, packet_type_enum packettype
        , char const * data, int len)
    {
        netty::p2p::enqueue_packets<output_queue_type>(q, _host_id, packettype, PACKET_SIZE, data, len);
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    bool enqueue_packets (peer_id addressee, packet_type_enum packettype
        , char const * data, int len)
    {
        auto * awriter = locate_writer_account(addressee);

        if (awriter == nullptr) {
            Callbacks::on_error(tr::f_("no writer account found for enqueue packets: {}", addressee));
            return false;
        }

        enqueue_packets_helper(awriter->regular_queue, packettype, data, len);
        return true;
    }

    bool enqueue_file_chunk (peer_id addressee, file_id_type fileid, packet_type_enum packettype
        , char const * data, int len)
    {
        auto * awriter = locate_writer_account(addressee);

        if (awriter == nullptr) {
            Callbacks::on_error(tr::f_("no writer account found for enqueue file chunk: {}", addressee));
            return false;
        }

        auto pos = awriter->chunks.find(fileid);

        if (pos == awriter->chunks.end()) {
            auto res = awriter->chunks.emplace(fileid, output_queue_type{});
            pos = res.first;
        }

        enqueue_packets_helper(pos->second, packettype, data, len);
        return true;
    }

    void process_socket_connected (typename client_poller_type::native_socket_type sock)
    {
        auto awriter = locate_writer_account(sock);

        if (awriter == nullptr) {
            Callbacks::on_error(tr::f_("socket connected, but writer not found: socket={}", sock));
            return;
        }

        _writer_poller->wait_for_write(awriter->writer);
        awriter->connected = true;

        Callbacks::writer_ready(host4_addr{awriter->peerid, awriter->writer.saddr()});

        // Only addresser need by the receiver
        enqueue_packets(awriter->peerid, packet_type_enum::hello, "HELO", 4);

        check_complete_channel(awriter->peerid);
    }

    void process_reader_input (typename server_poller_type::native_socket_type sock)
    {
        auto areader = locate_reader_account(sock);

        if (areader == nullptr) {
            Callbacks::on_error(tr::f_("no reader account located by socket for process input: {}", sock));
            return;
        }

        auto & inpb = areader->raw;

        // Read all received data and put it into input buffer.
        for (;;) {
            error err;
            auto offset = inpb.size();
            inpb.resize(offset + PACKET_SIZE);

            auto n = areader->reader.recv(inpb.data() + offset, PACKET_SIZE, & err);

            if (n < 0) {
                Callbacks::on_error(tr::f_("receive data failure ({}) from: {}"
                    , err.what(), to_string(areader->reader.saddr())));
                Callbacks::defere_expire_peer(areader->peerid);
                return;
            }

            inpb.resize(offset + n);

            if (n < PACKET_SIZE)
                break;
        }

        auto available = inpb.size();

        if (available < PACKET_SIZE)
            return;

        auto inpb_pos = inpb.begin();

        do {
            typename Serializer::istream_type in {& *inpb_pos, PACKET_SIZE};
            available -= PACKET_SIZE;
            inpb_pos += PACKET_SIZE;

            static_assert(sizeof(packet_type_enum) <= sizeof(char), "");

            auto packettype = static_cast<packet_type_enum>(*in.peek());
            std::uint16_t packetsize {0};

            if (!is_valid(packettype)) {
                Callbacks::on_error(tr::f_("unexpected packet type ({}) received from: {}, ignored."
                    , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                    , to_string(areader->reader.saddr())));

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                Callbacks::defere_expire_peer(areader->peerid);

                inpb_pos = inpb.end();
                break;
            }

            packet pkt;
            in >> pkt;
            packetsize = pkt.packetsize;

            if (packetsize != PACKET_SIZE) {
                Callbacks::on_error(tr::f_("unexpected packet size ({}) received from: {}, expected: {}"
                    , PACKET_SIZE
                    , to_string(areader->reader.saddr())
                    , packetsize));

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                Callbacks::defere_expire_peer(areader->peerid);

                inpb_pos = inpb.end();
                break;
            }

            // Start new sequence (message)
            if (pkt.partindex == 1)
                areader->b.clear();

            if (pkt.payloadsize > 0)
                areader->b.insert(areader->b.end(), pkt.payload, pkt.payload + pkt.payloadsize);

            // Message complete
            if (pkt.partindex == pkt.partcount) {
                auto peerid = pkt.addresser;

                switch (packettype) {
                    case packet_type_enum::regular:
                        Callbacks::data_received(peerid, std::move(areader->b));
                        break;

                    case packet_type_enum::hello: {
                        // Complete reader account
                        areader->peerid = peerid;
                        Callbacks::reader_ready(host4_addr{peerid, areader->reader.saddr()});
                        check_complete_channel(peerid);
                        break;
                    }

                    case packet_type_enum::file_credentials:
                    case packet_type_enum::file_request:
                    case packet_type_enum::file_stop:
                    case packet_type_enum::file_chunk:
                    case packet_type_enum::file_begin:
                    case packet_type_enum::file_end:
                    case packet_type_enum::file_state:
                        Callbacks::file_data_received(peerid, packettype, std::move(areader->b));
                        break;
                }
            }
        } while (available >= PACKET_SIZE);

        if (available == 0)
            inpb.clear();
        else
            inpb.erase(inpb.begin(), inpb_pos);
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
                    Callbacks::on_error(tr::f_("send failure to {}: {}", to_string(awriter.writer.saddr())
                        , err.what()));

                    // Expire peer
                    Callbacks::defere_expire_peer(awriter.peerid);
                    break_sending = true;
                    break;

                case netty::send_status::network:
                    Callbacks::on_error(tr::f_("send failure to {}: network failure: {}"
                        , to_string(awriter.writer.saddr()), err.what()));

                    // Expire peer
                    Callbacks::defere_expire_peer(awriter.peerid);
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
                            Callbacks::request_file_chunk(awriter.peerid, pos->first);
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

