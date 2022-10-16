////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.20 Initial version.
//      2021.11.01 Complete basic version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/log.hpp"
#include "pfs/memory.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <array>
#include <bitset>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace netty {
namespace p2p {

namespace {
constexpr std::int32_t DEFAULT_OVERFLOW_LIMIT {1024};
constexpr std::chrono::milliseconds DEFAULT_OVERFLOW_TIMEOUT { 100};
constexpr std::size_t  DEFAULT_BUFFER_BULK_SIZE {64 * 1024};
constexpr char const * UNSUITABLE_VALUE = tr::noop_("Unsuitable value for option: {}");
constexpr char const * BAD_VALUE        = tr::noop_("Bad value for option: {}");
}

/**
 * @brief P2P delivery engine.
 *
 * @tparam DiscoverySocketBackend Discovery socket backend.
 * @tparam SocketsBackend Socket backend.
 * @tparam PACKET_SIZE Maximum packet size (must be less or equal to
 *         @c packet::MAX_PACKET_SIZE and greater than @c packet::PACKET_HEADER_SIZE).
 */
template <
      typename DiscoveryEngineBackend
    , typename SocketsApiBackend
    , typename FileTransporterBackend
    , std::uint16_t PACKET_SIZE = packet::MAX_PACKET_SIZE>  // Meets the requirements for reliable and in-order data delivery
class engine
{
    static_assert(PACKET_SIZE <= packet::MAX_PACKET_SIZE
        && PACKET_SIZE > packet::PACKET_HEADER_SIZE, "");

public:
    using entity_id = std::uint64_t; // Zero value is invalid entity
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

    static constexpr entity_id INVALID_ENTITY {0};

    enum class option_enum: std::int16_t
    {
        ////////////////////////////////////////////////////////////////////////
        // Engine options
        ////////////////////////////////////////////////////////////////////////
        // Limit to size for output queue
          overflow_limit     // std::int32_t

        ////////////////////////////////////////////////////////////////////////
        // Socket API options
        ////////////////////////////////////////////////////////////////////////
        , listener_address   // socket4_addr

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        , listener_backlog   // std::int32_t

        , poll_interval      // std::chrono::milliseconds

        ////////////////////////////////////////////////////////////////////////
        // Discovery options
        ////////////////////////////////////////////////////////////////////////
        , discovery_address  // socket4_addr

        // Discovery transmit interval
        , transmit_interval  // std::chrono::milliseconds

        ////////////////////////////////////////////////////////////////////////
        // File transporter options
        ////////////////////////////////////////////////////////////////////////
        // Directory to store received files
        , download_directory // fs::path

        // Automatically remove transient files on error
        , remove_transient_files_on_error // bool

        , file_chunk_size    // std::int32_t
        , max_file_size      // std::int32_t

        // The dowload progress granularity (from 0 to 100) determines the
        // frequency of download progress notification. 0 means notification for
        // all download progress and 100 means notification only when the
        // download is complete.
        , download_progress_granularity // std::int32_t (from 0 to 100)
    };

private:
    std::unique_ptr<DiscoveryEngineBackend> _discovery;
    std::unique_ptr<SocketsApiBackend>      _socketsapi;
    std::unique_ptr<FileTransporterBackend> _transporter;

private:
    struct oqueue_item
    {
        entity_id    id;
        universal_id addressee;
        packet       pkt;
    };

    // Terms
    // partial loss of efficiency
    // limited efficiency
    enum class efficiency {
          good = 0
        , file_output  // Loss of the file output operations.
    };

    using socket_type = typename SocketsApiBackend::socket_type;
    using socket_id   = typename SocketsApiBackend::socket_id;

    using oqueue_type = pfs::ring_buffer<oqueue_item, DEFAULT_BUFFER_BULK_SIZE>;

private:
    struct options
    {
        std::int32_t overflow_limit {DEFAULT_OVERFLOW_LIMIT};
    } _opts;

    std::bitset<4> _efficiency_loss_bits {0};

    // Unique identifier for entity (message, file chunks) inside engine session.
    entity_id _entity_id {0};

    // Host (listener) identifier.
    universal_id _host_uuid;

    // Used to solve problems with output overflow.
    std::chrono::milliseconds _output_timepoint;

    // Mapping from writer contact identifier to socket identifier.
    // Contains element if the writer connected to peer.
    std::map<universal_id, socket_id> _writer_ids;

    // Mapping from writer socket identifier to contact identifier.
    // Contains the element if peer discovered and started connection to it.
    std::map<socket_id, universal_id> _writer_uuids;

    // Mapping from reader socket identifier to contact identifier.
    // Contains element if the reader received data from peer.
    // Filled in / modified in process_socket_input while receiving packets
    std::map<socket_id, universal_id> _reader_uuids;

    // Input buffers pool
    // Buffers to accumulate payload from input packets
    struct ipool_item
    {
        std::vector<char> b;
    };

    // Packet output queue
    struct opool_item
    {
        oqueue_type q;
    };

    std::unordered_map<socket_id, ipool_item>    _ipool;
    std::unordered_map<universal_id, opool_item> _opool;

public: // Callbacks
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    /**
     * Called when new peer discovered by desicover engine.
     * This is an opposite event to `peer_expired`.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> peer_discovered
        = [] (universal_id, inet4_addr, std::uint16_t) {};

    /**
     * Called when no discovery packets were received for a specified period.
     * This is an opposite event to `peer_discovered`.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> peer_expired
        = [] (universal_id, inet4_addr, std::uint16_t) {};

    /**
     * Called when new writer socket ready (connected).
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> writer_ready
        = [] (universal_id, inet4_addr, std::uint16_t) {};

    /**
     * Called when writer socket closed/disconnected.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> writer_closed
        = [] (universal_id, inet4_addr, std::uint16_t) {};

    /**
     * Called when reader socket closed/disconnected.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> reader_closed
        = [] (universal_id, inet4_addr, std::uint16_t) {};

    /**
     * Message received.
     */
    mutable std::function<void (universal_id, std::string)> data_received
        = [] (universal_id, std::string) {};

    /**
     * Message dispatched
     *
     * Do not call engine::send() method from this callback.
     */
//     mutable std::function<void (entity_id)> data_dispatched
//         = [] (entity_id) {};

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , filesize_t /*downloaded_size*/
        , filesize_t /*total_size*/)> download_progress
        = [] (universal_id, universal_id, filesize_t , filesize_t) {};

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , fs::path const & /*path*/
        , bool /*success*/)> download_complete
        = [] (universal_id, universal_id, fs::path const &, bool) {};

    /**
     * Called when file download interrupted, i.e. after peer closed.
     */
    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/)> download_interrupted
        = [] (universal_id, universal_id) {};

private:
    inline entity_id next_entity_id () noexcept
    {
        ++_entity_id;
        return _entity_id == INVALID_ENTITY ? ++_entity_id : _entity_id;
    }

    // UNUSED YET
    inline void loss_efficiency (efficiency value) noexcept
    {
        _efficiency_loss_bits.set(static_cast<std::size_t>(value));
    }

    // UNUSED YET
    inline void restore_efficiency (efficiency value) noexcept
    {
        _efficiency_loss_bits.reset(static_cast<std::size_t>(value));
    }

    inline bool test_efficiency (efficiency value) const noexcept
    {
        return !_efficiency_loss_bits.test(static_cast<std::size_t>(value));
    }

    inline void append_chunk (std::vector<char> & vec
        , char const * buf, std::size_t len)
    {
        if (len > 0)
            vec.insert(vec.end(), buf, buf + len);
    }

    socket_type * locate_writer (universal_id const & uuid)
    {
        // _writer_ids contains the element by uuid if the writer already
        // in connected
        auto pos = _writer_ids.find(uuid);

        return (pos == _writer_ids.end())
            ? nullptr
            : _socketsapi->locate(pos->second);
    }

    ipool_item * locate_ipool_item (socket_id sock_id)
    {
        auto pos = _ipool.find(sock_id);

        if (pos == _ipool.end()) {
            log_error(tr::f_("Cannot locate input pool item by socket identifier: {}"
                , sock_id));
            return nullptr;
        }

        return & pos->second;
    }

    opool_item * locate_opool_item (universal_id peer_uuid)
    {
        auto pos = _opool.find(peer_uuid);

        if (pos == _opool.end()) {
            log_error(tr::f_("Cannot locate output pool item by identifier: {}"
                , peer_uuid));
            return nullptr;
        }

        return & pos->second;
    }

    opool_item * ensure_opool_item (universal_id peer_uuid)
    {
        auto pos = _opool.find(peer_uuid);

        if (pos == _opool.end()) {
            auto res = _opool.emplace(peer_uuid, opool_item{});
            PFS__ASSERT(res.second, "");
            pos = res.first;
        }

        return & pos->second;
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    entity_id enqueue_packets (universal_id addressee
        , packet_type packettype
        , char const * data, std::streamsize len)
    {
        auto * pitem = ensure_opool_item(addressee);

        PFS__ASSERT(pitem, "");

        auto payload_size = PACKET_SIZE - packet::PACKET_HEADER_SIZE;
        auto remain_len   = len;
        char const * remain_data = data;
        std::uint32_t partindex  = 1;
        std::uint32_t partcount  = len / payload_size
            + (len % payload_size ? 1 : 0);

        auto entityid = next_entity_id();

        while (remain_len) {
            packet p;
            p.packettype  = packettype;
            p.packetsize  = PACKET_SIZE;
            p.addresser   = _host_uuid;
            p.payloadsize = remain_len > payload_size
                ? payload_size
                : static_cast<std::uint16_t>(remain_len);
            p.partcount   = partcount;
            p.partindex   = partindex++;
            std::memset(p.payload, 0, payload_size);
            std::memcpy(p.payload, remain_data, p.payloadsize);

            remain_len -= p.payloadsize;
            remain_data += p.payloadsize;

            // May throw std::bad_alloc
            pitem->q.push(oqueue_item{entityid, addressee, std::move(p)});
        }

        return entityid;
    }

public:
    /**
     * Initializes underlying APIs and constructs engine instance.
     *
     * @param uuid Unique identifier for this instance.
     */
    engine (universal_id host_uuid)
        : _host_uuid(host_uuid)
    {
        log_error = [] (std::string const & s) {
            fmt::print(stderr, "{}\n", s);
        };

        _discovery = pfs::make_unique<DiscoveryEngineBackend>(_host_uuid);
        _socketsapi = pfs::make_unique<SocketsApiBackend>();
        _transporter = pfs::make_unique<FileTransporterBackend>();
    }

    ~engine () = default;

    engine (engine const &) = delete;
    engine & operator = (engine const &) = delete;

    engine (engine &&) = default;
    engine & operator = (engine &&) = default;

    universal_id const & host_uuid () const noexcept
    {
        return _host_uuid;
    }

    bool start ()
    {
        _discovery->log_error = [this] (std::string const & errstr) {
            log_error(errstr);
        };

        _discovery->peer_discovered = [this] (universal_id peer_uuid
                , socket4_addr saddr) {
            process_peer_discovered(peer_uuid, saddr);
        };

        _discovery->peer_expired = [this] (universal_id peer_uuid
                , socket4_addr saddr) {
            peer_expired(peer_uuid, saddr.addr, saddr.port);
        };

        _socketsapi->log_error = [this] (std::string const & errstr) {
            log_error(errstr);
        };

        _socketsapi->socket_accepted = [this] (socket_id sid, socket4_addr saddr) {
            process_accepted(sid, saddr);
        };

        _socketsapi->socket_connected = [this] (socket_id sid, socket4_addr saddr) {
            process_socket_connected(sid, saddr);
        };

        _socketsapi->socket_closed = [this] (socket_id sid, socket4_addr saddr) {
            process_socket_closed(sid, saddr);
        };

        _socketsapi->ready_read = [this] (socket_id sid, socket_type * psock) {
            process_socket_input(sid, psock);
        };

        _transporter->log_error = [this] (std::string const & errstr) {
            log_error(errstr);
        };

        _transporter->addressee_ready = [this] (universal_id addressee) {
            auto * pitem = locate_opool_item(addressee);
            return pitem && (pitem->q.size() <= _opts.overflow_limit)
                ? true: false;
        };

        _transporter->ready_to_send = [this] (universal_id addressee
                , packet_type packettype
                , char const * data, std::streamsize len) {
            enqueue_packets(addressee, packettype, data, len);
        };

        _transporter->download_progress = [this] (universal_id addresser
                , universal_id fileid
                , filesize_t downloaded_size
                , filesize_t total_size) {
            download_progress(addresser, fileid, downloaded_size, total_size);
        };

        _transporter->download_complete = [this] (universal_id addresser
                , universal_id fileid
                , pfs::filesystem::path const & path
                , bool success) {

            if (!success) {
                log_error(tr::f_("Checksum does not match for income file: {} from {}"
                    , fileid, addresser));
            }

            download_complete(addresser, fileid, path, success);
        };

        _transporter->download_interrupted = [this] (universal_id addresser
                , universal_id fileid) {
            download_interrupted(addresser, fileid);
        };

        auto now = current_timepoint();
        _output_timepoint = now;

        try {
            _discovery->listen();
            _socketsapi->listen();
        } catch (error ex) {
            log_error(tr::f_("Start netty::p2p::engine failure: {}", ex.what()));
            return false;
        }

        return true;
    }

    bool set_option (option_enum opttype, fs::path const & path)
    {
        switch (opttype) {
            case option_enum::download_directory:
                return _transporter->set_option(_transporter->download_directory, path);

            default:
                break;
        }

        log_error(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, socket4_addr sa)
    {
        switch (opttype) {
            case option_enum::discovery_address:
                return _discovery->set_option(_discovery->discovery_address, sa);
            case option_enum::listener_address:
                return _socketsapi->set_option(_socketsapi->listener_address, sa)
                    && _discovery->set_option(_discovery->listener_port, sa.port);
            default:
                break;
        }

        log_error(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, std::chrono::milliseconds msecs)
    {
        switch (opttype) {
            case option_enum::transmit_interval:
                return _discovery->set_option(_discovery->transmit_interval, msecs);

            case option_enum::poll_interval:
                return _socketsapi->set_option(_socketsapi->poll_interval, msecs);

            default:
                break;
        }

        log_error(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, std::intmax_t value)
    {
        switch (opttype) {
            case option_enum::overflow_limit:
            if (value > 0 && value <= (std::numeric_limits<std::int32_t>::max)()) {
                _opts.overflow_limit = static_cast<std::int32_t>(value);
                return true;
            }

            log_error(tr::_("Overflow limit must be a positive integer"));
            break;

            case option_enum::remove_transient_files_on_error:
                return _transporter->set_option(
                    _transporter->remove_transient_files_on_error
                    , value);

            case option_enum::file_chunk_size:
                return _transporter->set_option(
                    _transporter->file_chunk_size
                    , value);

            case option_enum::max_file_size:
                return _transporter->set_option(_transporter->max_file_size
                    , value);

            case option_enum::download_progress_granularity:
                return _transporter->set_option(
                    _transporter->download_progress_granularity, value);

            case option_enum::listener_backlog:
                return _socketsapi->set_option(_socketsapi->listener_backlog
                    , value);

            default:
                break;
        }

        log_error(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    /**
     * @return @c false if some error occurred, otherwise return @c true.
     */
    void loop ()
    {
        _discovery->loop();
        _socketsapi->loop();

        auto output_possible = current_timepoint() >= _output_timepoint;

        if (test_efficiency(efficiency::file_output)) {
            if (output_possible)
                _transporter->loop();
        }

        if (output_possible)
            send_outgoing_packets();
    }

    /**
     * Check if state of engine is good (engine is in working order).
     */
    bool good () const noexcept
    {
        return !_efficiency_loss_bits.any();
    }

    /**
     * Break internal loop while call @c run() method.
     */
    // TODO Only for standalone mode
    //void quit () noexcept
    //{
    //    _running_flag.clear(std::memory_order_relaxed);
    //}

    // TODO Only for standalone mode
    //void run ()
    //{
    //    while (_running_flag.test_and_set(std::memory_order_relaxed))
    //        loop();
    //}

    void add_discovery_target (inet4_addr const & addr, std::uint16_t port)
    {
        _discovery->add_target(socket4_addr{addr, port});
    }

    /**
     * Send data.
     *
     * @details Actually this method splits data to send into packets and
     *          enqueue them into output queue.
     *
     * @param addressee_id Addressee unique identifier.
     * @param data Data to send.
     * @param len  Data length.
     *
     * @return Unique entity identifier.
     */
    entity_id send (universal_id addressee_id, char const * data
        , std::streamsize len)
    {
        return enqueue_packets(addressee_id, packet_type::regular, data, len);
    }

    /**
     * Send file.
     *
     * @param addressee_id Addressee unique identifier.
     * @param file_id Unique file identifier. If @a file_id is invalid it
     *        will be generated automatically.
     * @param path Path to sending file.
     *
     * @return Unique file identifier or invalid identifier on error.
     */
    universal_id send_file (universal_id addressee, universal_id fileid
        , fs::path const & path)
    {
        return _transporter->send_file(addressee, fileid, path);
    }

    /**
     * Send file.
     *
     * @param addressee_id Addressee unique identifier.
     * @param path Path to sending file.
     *
     * @return Unique file identifier or invalid identifier on error.
     */
    universal_id send_file (universal_id addressee_id, fs::path const & path)
    {
        return _transporter->send_file(addressee_id, universal_id{}, path);
    }

    /**
     * Send command to stop uploading file by @a addressee.
     */
    void stop_file (universal_id addressee, universal_id fileid)
    {
        _transporter->stop_file(addressee, fileid);
    }

private:
    void process_peer_discovered (universal_id peer_uuid, socket4_addr saddr)
    {
        peer_discovered(peer_uuid, saddr.addr, saddr.port);
        auto sid = _socketsapi->connect(saddr);

        _writer_uuids.emplace(sid, peer_uuid);

        // Connecting to peer
        LOG_TRACE_2("Connecting to: {}@{}", peer_uuid, to_string(saddr.addr));
    }

    void process_socket_connected (socket_id sid, socket4_addr saddr)
    {
        auto pos = _writer_uuids.find(sid);

        if (pos == std::end(_writer_uuids)) {
            log_error(tr::f_("Unexpected socket connected to: {}", to_string(saddr)));
            return;
        }

        _writer_ids.emplace(pos->second, sid);
        writer_ready(pos->second, saddr.addr, saddr.port);
    }

    void process_socket_closed (socket_id sid, socket4_addr saddr)
    {
        auto pos = _writer_uuids.find(sid);

        if (pos != std::end(_writer_uuids)) {
            LOG_TRACE_2("Writer socket closed: {}", sid);

            // Erase item from output pool for specified socket
            _opool.erase(pos->second);

            // Erase file output pool
            _transporter->expire_addressee(pos->second);

            writer_closed(pos->second, saddr.addr, saddr.port);
            _writer_ids.erase(pos->second);
            _writer_uuids.erase(pos);

        } else {
            auto pos = _reader_uuids.find(sid);

            // Erase item from input pool for specified socket
            _ipool.erase(sid);

            if (pos != std::end(_reader_uuids)) {
                LOG_TRACE_2("Reader socket closed: {}", sid);

                // Erase file input pool
                _transporter->expire_addresser(pos->second);

                reader_closed(pos->second, saddr.addr, saddr.port);
                _reader_uuids.erase(pos);
            } else {
                LOGW("netty-p2p", "Unknown socket closed: {} (need investigation)", sid);
            }
        }
    }

    void process_accepted (socket_id sid, socket4_addr saddr)
    {
        // Insert item to input pool or reset it
        auto res = _ipool.emplace(sid, ipool_item{});

        if (res.second) {
            // New inserted
            LOG_TRACE_2("Accepted socket added to input pool: {} (size={})"
                , to_string(saddr)
                , _ipool.size());
        } else {
            auto & item = res.first->second;

            // Already exists
            item.b.clear();
            LOG_TRACE_2("Accepted socket reset in input pool: {}", to_string(saddr));
        }
    }

    void process_socket_input (socket_id sid, socket_type * psock)
    {
        int read_iterations = 0;

        auto * pitem = locate_ipool_item(sid);

        do {
            std::array<char, PACKET_SIZE> buf;
            std::streamsize rc = psock->recvmsg(buf.data(), buf.size());

            if (rc == 0) {
                if (read_iterations == 0) {
                    log_error(tr::f_("Expected any data from: {}:{}, but no data read"
                        , to_string(psock->addr())
                        , psock->port()));
                }

                break;
            }

            if (rc < 0) {
                log_error(tr::f_("Receive data from: {}:{} failure: {} (code={})"
                    , to_string(psock->addr())
                    , psock->port()
                    , psock->error_string()
                    , psock->error_code()));
                break;
            }

            read_iterations++;

            // Ignore input
            if (!pitem)
                continue;

            input_envelope_type in {buf.data(), buf.size()};

            static_assert(sizeof(packet_type) <= sizeof(char), "");

            auto packettype = static_cast<packet_type>(in.peek());
            decltype(packet::packetsize) packetsize {0};

            auto valid_packettype = packettype == packet_type::regular
                || packettype == packet_type::file_credentials
                || packettype == packet_type::file_request
                || packettype == packet_type::file_chunk
                || packettype == packet_type::file_end
                || packettype == packet_type::file_state
                || packettype == packet_type::file_stop;

            if (!valid_packettype) {
                log_error(tr::f_("Unexpected packet type ({})"
                    " received from: {}:{}, ignored."
                    , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                    , to_string(psock->addr())
                    , psock->port()));
                continue;
            }

            packet pkt;
            in.unseal(pkt);
            packetsize = pkt.packetsize;

            if (rc != packetsize) {
                log_error(tr::f_("Unexpected packet size ({})"
                    " received from: {}:{}, expected: {}"
                    , rc
                    , to_string(psock->addr())
                    , psock->port()
                    , packetsize));

                // Ignore
                continue;
            }

            // Start new sequence
            if (pkt.partindex == 1)
                pitem->b.clear();

            append_chunk(pitem->b, pkt.payload, pkt.payloadsize);

            // Message complete
            if (pkt.partindex == pkt.partcount) {
                auto sender = pkt.addresser;

                switch (packettype) {
                    case packet_type::regular:
                        this->data_received(sender
                            , std::string(pitem->b.data(), pitem->b.size()));
                        break;

                    // Initiating file sending from peer
                    case packet_type::file_credentials:
                        _transporter->process_file_credentials(sender, pitem->b);
                        break;

                    case packet_type::file_request:
                        _transporter->process_file_request(sender, pitem->b);
                        break;

                    case packet_type::file_stop:
                        _transporter->process_file_stop(sender, pitem->b);
                        break;

                    // File chunk received
                    case packet_type::file_chunk:
                        _transporter->process_file_chunk(sender, pitem->b);
                        break;

                    // File received completely
                    case packet_type::file_end:
                        _transporter->process_file_end(sender, pitem->b);
                        break;

                    case packet_type::file_state:
                        _transporter->process_file_state(sender, pitem->b);
                        break;
                }
            }
        } while (true);
    }

    void send_outgoing_packets ()
    {
        for (auto & pool_item: _opool) {
            auto & output_queue = pool_item.second.q;

            if (output_queue.empty())
                continue;

            std::size_t total_bytes_sent = 0;
            auto * sock = locate_writer(pool_item.first);

            if (!sock)
                continue;

            output_envelope_type out;

            while (!output_queue.empty()) {
                auto & item = output_queue.front();

                auto const & pkt = item.pkt;
                out.reset();   // Empty envelope
                out.seal(pkt); // Seal new data

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size <= PACKET_SIZE, "");

                auto bytes_sent = sock->sendmsg(data.data(), size);

                if (bytes_sent > 0) {
                    total_bytes_sent += bytes_sent;

//                     // Message sent complete
//                     if (pkt.partindex == pkt.partcount)
//                         data_dispatched(item.id);
                } else if (bytes_sent < 0) {
                    // Output queue overflow
                    if (sock->overflow()) {
                        // Skip sending for some milliseconds
                        _output_timepoint = current_timepoint() + DEFAULT_OVERFLOW_TIMEOUT;
                        return;
                    } else {
                        // Is socket is non-functional because of broken or closed
                        // do not log error, ignore silently.
                        if (sock->state() != sock->CONNECTED) {
                            ;
                        } else {
                            log_error(tr::f_("Sending failure to {}@{}: {}"
                                , pool_item.first
                                , to_string(sock->saddr())
                                , sock->error_string()));
                        }
                    }
                } else {
                    // FIXME Need to handle this state (broken connection ?)
                    ;
                }

                output_queue.pop();
            }
        }
    }
};

}} // namespace netty::p2p
