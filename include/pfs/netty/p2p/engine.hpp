////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.20 Initial version.
//      2021.11.01 Complete basic version.
//      2023.01.18 Version 2 started.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "discovery_engine.hpp"
#include "engine_traits.hpp"
#include "envelope.hpp"
#include "file_transporter.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/log.hpp"
#include "pfs/memory.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/startup.hpp"
#include <array>
#include <bitset>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace netty {
namespace p2p {

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
    , typename EngineTraits     = default_engine_traits
    , typename FileTransporter  = file_transporter
    , std::uint16_t PACKET_SIZE = packet::MAX_PACKET_SIZE>  // Meets the requirements for reliable and in-order data delivery
class engine
{
    static_assert(PACKET_SIZE <= packet::MAX_PACKET_SIZE
        && PACKET_SIZE > packet::PACKET_HEADER_SIZE, "");

private:
    using entity_id = std::uint64_t; // Zero value is invalid entity
    using client_poller_type    = typename EngineTraits::client_poller_type;
    using server_poller_type    = typename EngineTraits::server_poller_type;
    using server_type           = typename EngineTraits::server_type;
    using reader_type           = typename EngineTraits::reader_type;
    using writer_type           = typename EngineTraits::writer_type;
    using reader_id             = typename EngineTraits::reader_id;
    using writer_id             = typename EngineTraits::writer_id;
    using discovery_engine_type = discovery_engine<DiscoveryEngineBackend>;
    using input_envelope_type   = input_envelope<>;
    using output_envelope_type  = output_envelope<>;

    static constexpr entity_id INVALID_ENTITY {0};

public:
    struct options
    {
        // Limit to size for output queue
        std::int32_t overflow_limit = 1024;

        std::chrono::milliseconds overflow_timeout {100};

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        int listener_backlog {100};

        bool check_reader_consistency {true};
        bool check_writer_consistency {true};

        typename FileTransporter::options filetransporter;
        typename discovery_engine_type::options discovery;
    } _opts;

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

    using oqueue_type = pfs::ring_buffer<oqueue_item, 64 * 1024>;

private:
    // Host (listener) identifier.
    universal_id _host_uuid;

    server_type _server;

    std::unique_ptr<discovery_engine_type> _discovery;
    std::unique_ptr<FileTransporter>       _transporter;
    std::unique_ptr<server_poller_type>    _reader_poller;
    std::unique_ptr<client_poller_type>    _writer_poller;

    std::bitset<4> _efficiency_loss_bits {0};

    // Unique identifier for entity (message, file chunks) inside engine session.
    entity_id _entity_id {0};

    // Used to solve problems with output overflow.
    clock_type::time_point _output_timepoint;

    struct writer_item {
        universal_id uuid;
        writer_type writer;

        // Packet output queue
        oqueue_type q;
    };

    using writer_collection_type = std::vector<std::pair<bool, writer_item>>;
    using writer_index = typename writer_collection_type::size_type;
    writer_collection_type               _writers;
    std::map<writer_id, writer_index>    _writer_ids;
    std::map<universal_id, writer_index> _writer_uuids;

    struct reader_item {
        universal_id uuid;
        reader_type reader;

        // Buffer to accumulate payload from input packets
        std::vector<char> b;
    };

    using reader_collection_type = std::vector<std::pair<bool, reader_item>>;
    using reader_index = typename reader_collection_type::size_type;
    reader_collection_type               _readers;
    std::map<reader_id, reader_index>    _reader_ids;
    std::map<universal_id, reader_index> _reader_uuids;

public: // Callbacks
    mutable std::function<void (std::string const &)> on_error
        = [] (std::string const &) {};

    /**
     * Called when new peer discovered by desicover engine.
     * This is an opposite event to `peer_expired`.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t
        , std::chrono::milliseconds const &)> peer_discovered
        = [] (universal_id, inet4_addr, std::uint16_t, std::chrono::milliseconds const &) {};

    /**
     * Called when the time difference has changed significantly.
     */
    mutable std::function<void (universal_id, std::chrono::milliseconds const &)> peer_timediff
        = [] (universal_id, std::chrono::milliseconds const &) {};

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
     * Called when new reader socket ready (handshaked).
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> reader_ready
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

public:
    /**
     * Initializes underlying APIs and constructs engine instance.
     *
     * @param uuid Unique identifier for this instance.
     */
    engine (universal_id host_uuid, socket4_addr server_addr, options && opts)
        : _host_uuid(host_uuid)
        , _server(server_addr)
    {
        auto bad = false;
        std::string invalid_argument_desc;
        _opts = std::move(opts);

        do {
            bad = _opts.overflow_limit <= 0
                && _opts.overflow_limit > (std::numeric_limits<std::int32_t>::max)();

            if (bad) {
                invalid_argument_desc = tr::_("overflow limit must be a positive integer");
                break;
            }

        } while (false);

        if (bad) {
            error err {
                  make_error_code(errc::invalid_argument)
                , invalid_argument_desc
            };

            throw err;
        }

        // Call befor any network operations
        startup();

        _discovery = pfs::make_unique<discovery_engine_type>(_host_uuid
            , std::move(_opts.discovery));

        _transporter = pfs::make_unique<FileTransporter>(
            std::move(_opts.filetransporter));

        on_error = [] (std::string const & s) {
            fmt::print(stderr, "{}\n", s);
        };
    }

    ~engine ()
    {
        cleanup();
    }

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
        ////////////////////////////////////////////////////////////////////////
        // Configure discovery
        ////////////////////////////////////////////////////////////////////////
        {
            _discovery->on_error = [this] (std::string const & errstr) {
                this->on_error(errstr);
            };

            _discovery->peer_discovered = [this] (universal_id peer_uuid
                    , socket4_addr saddr, std::chrono::milliseconds const & timediff) {
                this->process_peer_discovered(peer_uuid, saddr, timediff);
            };

            _discovery->peer_timediff = [this] (universal_id peer_uuid
                    , std::chrono::milliseconds const & timediff) {
                this->peer_timediff(peer_uuid, timediff);
            };

            _discovery->peer_expired = [this] (universal_id peer_uuid, socket4_addr saddr) {
                release_reader(peer_uuid);
                release_writer(peer_uuid);
                peer_expired(peer_uuid, saddr.addr, saddr.port);
            };
        }

        ////////////////////////////////////////////////////////////////////////
        // Configure file transporter
        ////////////////////////////////////////////////////////////////////////
        {
            _transporter->on_error = [this] (std::string const & errstr) {
                this->on_error(errstr);
            };

            _transporter->addressee_ready = [this] (universal_id addressee) {
                auto * pitem = locate_writer_item(addressee);
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
                    on_error(tr::f_("Checksum does not match for income file: {} from {}"
                        , fileid, addresser));
                }

                download_complete(addresser, fileid, path, success);
            };

            _transporter->download_interrupted = [this] (universal_id addresser
                    , universal_id fileid) {
                download_interrupted(addresser, fileid);
            };
        }

        ////////////////////////////////////////////////////////////////////////
        // Create and configure main listener poller
        ////////////////////////////////////////////////////////////////////////
        {
            typename server_poller_type::callbacks callbacks;

            callbacks.on_error = [this] (typename server_poller_type::native_socket_type
                , std::string const & text) {
                this->on_error(text);
            };

            callbacks.accept = [this] (typename server_poller_type::native_socket_type listener_sock, bool & ok) {
                auto * reader = acquire_reader(listener_sock);

                if (!reader) {
                    ok = false;
                    return reader->INVALID_SOCKET;
                }

                ok = true;
                return reader->native();
            };

            callbacks.ready_read = [this] (typename server_poller_type::native_socket_type sock) {
                process_reader_input(sock);
            };

            callbacks.can_write = [] (typename server_poller_type::native_socket_type) {
                // FIXME
            };

            _reader_poller = pfs::make_unique<server_poller_type>(std::move(callbacks));
        }

        ////////////////////////////////////////////////////////////////////////
        // Create and configure writer poller
        ////////////////////////////////////////////////////////////////////////
        {
            typename client_poller_type::callbacks callbacks;

            callbacks.on_error = [this] (typename client_poller_type::native_socket_type
                , std::string const & text) {
                this->on_error(text);
            };

            callbacks.connection_refused = [this] (typename client_poller_type::native_socket_type sock) {
                release_writer(sock);
            };

            callbacks.disconnected = [this] (typename client_poller_type::native_socket_type sock) {
                release_writer(sock);
            };

            callbacks.connected = [this] (typename client_poller_type::native_socket_type sock) {
                process_socket_connected(sock);
            };

            // No need, writer sockets for write only
            callbacks.ready_read = [] (typename client_poller_type::native_socket_type sock) {};

            // No need yet
            callbacks.can_write = [] (typename client_poller_type::native_socket_type) {};

            _writer_poller = pfs::make_unique<client_poller_type>(std::move(callbacks));
        }

        auto now = current_timepoint();
        _output_timepoint = now;

        try {
            if (!_discovery->has_receivers()) {
                on_error(tr::_("no receivers specified for discovery"));
                return false;
            }

            if (!_discovery->has_targets()) {
                on_error(tr::_("no targets specified for discovery"));
                return false;
            }

            _reader_poller->add_listener(_server);

            // Start listening on main listener
            _server.listen(_opts.listener_backlog);
        } catch (error ex) {
            on_error(tr::f_("start netty::p2p::engine failure: {}", ex.what()));
            return false;
        }

        return true;
    }

    /**
     * @return @c false if some error occurred, otherwise return @c true.
     */
    void loop ()
    {
        // No need poll timeout
        _discovery->discover(std::chrono::milliseconds{0});

        /*auto res = */_reader_poller->poll(std::chrono::milliseconds{0});
        /*auto res = */_writer_poller->poll(std::chrono::milliseconds{0});

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
     * Add discovery receiver.
     */
    void add_receiver (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value)
    {
        _discovery->add_receiver(src_saddr, local_addr);
    }

    void add_target (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value)
    {
        _discovery->add_target(src_saddr, local_addr);
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

    entity_id send (universal_id addressee_id, std::string const & data)
    {
        return enqueue_packets(addressee_id, packet_type::regular, data.c_str()
            , data.size());
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

public: // static
    static options default_options () noexcept
    {
        return options{};
    }

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

    static inline void append_chunk (std::vector<char> & vec
        , char const * buf, std::size_t len)
    {
        if (len > 0)
            vec.insert(vec.end(), buf, buf + len);
    }

    void check_reader_consistency (reader_id id)
    {
        if (!_opts.check_reader_consistency)
            return;

        auto pos1 = _reader_ids.find(id);

        if (pos1 == _reader_ids.end()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_reader_consistency:"
                    " reader socket not found in collection (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto index = pos1->second;

        if (index >= _readers.size()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_reader_consistency:"
                    " index for reader item is out of bounds (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto alive = _readers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_reader_consistency:"
                    " invalid reader in collection found (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        // NOTE: The reader may not have been associated with universal
        //       identifier yet.
        //
        // auto & item = _readers[index].second;
        // auto pos2 = _reader_uuids.find(item.uuid);
        //
        // PFS__TERMINATE(pos2 != _reader_uuids.end()
        //      , tr::f_("p2p::engine::check_reader_consistency:"
        //          " reader universal identifier not found in collection (id={}, uuid={})"
        //          " , need to fix algorithm.", id, item.uuid));
    }

    reader_type * acquire_reader (reader_id listener_id)
    {
        auto reader = _server.accept(listener_id);

        reader_index index = 0;
        auto count = _readers.size();

        if (_readers.empty()) {
            _readers.reserve(32);
        } else {
            for (reader_index i = 0; i < count; i++) {
                if (!_readers[i].first) {
                    index = i;
                    break;
                }
            }
        }

        if (_readers.empty() || index == count) {
            _readers.emplace_back(true, reader_item{universal_id{}
                , std::move(reader), std::vector<char>{}});
            index = _readers.size() - 1;
        } else {
            _readers[index].first = true;
            _readers[index].second.uuid = universal_id{};
            _readers[index].second.reader = std::move(reader);
            _readers[index].second.b.clear();
        }

        reader_id id = _readers[index].second.reader.native();

        _reader_ids[id] = index;

        // Will be assign when hello packet received
        //_reader_uuids[uuid] = index;

        return & _readers[index].second.reader;
    }

    reader_item * locate_reader_item (reader_id id)
    {
        check_reader_consistency(id);

        auto pos = _reader_ids.find(id);
        auto index = pos->second;
        return & _readers[index].second;
    }

    void release_reader (universal_id uuid)
    {
        auto pos1 = _reader_uuids.find(uuid);

        if (pos1 == _reader_uuids.end())
            return;

        auto index = pos1->second;
        auto & item = _readers[index].second;

        check_reader_consistency(item.reader.native());

        auto pos2 = _reader_ids.find(item.reader.native());

        _readers[index].first = false;

        auto saddr = item.reader.saddr();

        // Erase file output pool
        _transporter->expire_addressee(item.uuid);

        _reader_poller->remove(item.reader);

        // Notify
        reader_closed(item.uuid, saddr.addr, saddr.port);

        // All is ok, release raeder resources
        _reader_uuids.erase(pos1);
        _reader_ids.erase(pos2);

        item.uuid = universal_id{};
        item.reader.~reader_type();
        item.b.clear(); // No need to destroy, can be reused later
    }

    void check_writer_consistency (writer_id id)
    {
        if (!_opts.check_writer_consistency)
            return;

        auto pos1 = _writer_ids.find(id);

        if (pos1 == _writer_ids.end()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer socket not found in collection (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto index = pos1->second;

        if (index >= _writers.size()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                " index for writer item is out of bounds (id={})"
                " , need to fix algorithm.", id)
            };
        }

        auto alive = _writers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " invalid writer in collection found (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto & item = _writers[index].second;

        auto pos2 = _writer_uuids.find(item.uuid);

        if (pos2 == _writer_uuids.end()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer universal identifier not found in collection (id={}, uuid={})"
                    " , need to fix algorithm.", id, item.uuid)
            };
        }
    }

    void check_writer_consistency (universal_id uuid)
    {
        if (!_opts.check_writer_consistency)
            return;

        auto pos2 = _writer_uuids.find(uuid);

        if (pos2 == _writer_uuids.end()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer universal identifier not found in collection (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto index = pos2->second;

        if (index >= _writers.size()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " index for writer item is out of bounds (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto alive = _writers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " invalid writer in collection found (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto & item = _writers[index].second;

        auto pos1 = _writer_ids.find(item.writer.native());

        if (pos1 == _writer_ids.end()) {
            throw error {
                  make_error_code(errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer socket not found in collection (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }
    }

    writer_type * acquire_writer (universal_id uuid)
    {
        writer_index index = 0;
        auto count = _writers.size();

        if (_writers.empty()) {
            _writers.reserve(32);
        } else {
            for (writer_index i = 0; i < count; i++) {
                if (!_writers[i].first) {
                    index = i;
                    break;
                }
            }
        }

        if (_writers.empty() || index == count) {
            _writers.emplace_back(true, writer_item{uuid, writer_type{}, oqueue_type{}});
            index = _writers.size() - 1;
        } else {
            _writers[index].first = true;
            _writers[index].second.uuid = uuid;
            _writers[index].second.writer = writer_type{};
            _writers[index].second.q.clear();
        }

        writer_id id = _writers[index].second.writer.native();

        _writer_ids[id] = index;
        _writer_uuids[uuid] = index;

        return & _writers[index].second.writer;
    }

    writer_item * locate_writer_item (writer_id id)
    {
        check_writer_consistency(id);

        auto pos = _writer_ids.find(id);
        auto index = pos->second;
        return & _writers[index].second;
    }

    writer_item * locate_writer_item (universal_id uuid)
    {
        check_writer_consistency(uuid);

        auto pos = _writer_uuids.find(uuid);
        auto index = pos->second;
        return & _writers[index].second;
    }

    void connect_writer (universal_id uuid, socket4_addr remote_saddr)
    {
        auto * writer = acquire_writer(uuid);
        auto conn_state = writer->connect(remote_saddr);
        _writer_poller->add(*writer, conn_state);
    }

    // Release writer by universal identifier
    void release_writer (universal_id uuid)
    {
        auto pos = _writer_uuids.find(uuid);

        if (pos == _writer_uuids.end())
            return;

        auto index = pos->second;
        auto & item = _writers[index].second;

        release_writer(item.writer.native());
    }

    // Release writer by native identifier
    void release_writer (writer_id id)
    {
        check_writer_consistency(id);

        auto pos1 = _writer_ids.find(id);
        auto index = pos1->second;

        _writers[index].first = false;

        auto & item = _writers[index].second;
        auto pos2 = _writer_uuids.find(item.uuid);

        // Erase file output pool
        _transporter->expire_addressee(item.uuid);

        auto do_release_reader = (_reader_uuids.find(item.uuid) != _reader_uuids.end());

        if (do_release_reader)
            release_reader(item.uuid);

        auto saddr = item.writer.saddr();

        _writer_poller->remove(item.writer);

        // Notify
        writer_closed(item.uuid, saddr.addr, saddr.port);

        // All is ok, release writer resources
        _writer_ids.erase(pos1);
        _writer_uuids.erase(pos2);

        item.uuid = universal_id{};
        item.writer.disconnect();
        item.writer.~writer_type();
        item.q.clear(); // No need to destroy, can be reused later
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    entity_id enqueue_packets (universal_id addressee
        , packet_type packettype
        , char const * data, std::streamsize len)
    {
        auto * pitem = locate_writer_item(addressee);

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

    void process_peer_discovered (universal_id peer_uuid, socket4_addr saddr
        , std::chrono::milliseconds const & timediff)
    {
        //LOG_TRACE_2("Peer discovered: {}@{}", peer_uuid, to_string(saddr));
        //LOG_TRACE_2("Connecting to: {}@{}", peer_uuid, to_string(saddr));
        connect_writer(peer_uuid, saddr);
        peer_discovered(peer_uuid, saddr.addr, saddr.port, timediff);
    }

    void process_socket_connected (writer_id id)
    {
        auto * item = locate_writer_item(id);
        writer_ready(item->uuid, item->writer.saddr().addr, item->writer.saddr().port);

        // Payload doesn't matter, but length must be greater than zero.
        char h = 'H';
        enqueue_packets(item->uuid, packet_type::hello, & h, 1);
    }

    void process_reader_input (reader_id sock)
    {
        int read_iterations = 0;

        auto * pitem = locate_reader_item(sock);
        auto available = pitem->reader.available();

        do {
            if (available < PACKET_SIZE)
                break;

            std::array<char, PACKET_SIZE> buf;
            std::streamsize rc = pitem->reader.recv(buf.data(), buf.size());

            if (rc == 0) {
                if (read_iterations == 0) {
                    // FIXME
                    // Disconnected ?
                    LOG_TRACE_2("process_reader_input: sender: {}.{}, need to DISCONNECT?"
                        , rc, to_string(pitem->reader.saddr()), sock);
                }

                break;
            }

            if (rc < 0) {
                on_error(tr::f_("Receive data failure from: {}"
                    , to_string(pitem->reader.saddr())));
                break;
            }

            available -= PACKET_SIZE;

            read_iterations++;

            input_envelope_type in {buf.data(), buf.size()};

            static_assert(sizeof(packet_type) <= sizeof(char), "");

            auto packettype = static_cast<packet_type>(in.peek());
            decltype(packet::packetsize) packetsize {0};

            auto valid_packettype = packettype == packet_type::regular
                || packettype == packet_type::hello
                || packettype == packet_type::file_credentials
                || packettype == packet_type::file_request
                || packettype == packet_type::file_chunk
                || packettype == packet_type::file_end
                || packettype == packet_type::file_state
                || packettype == packet_type::file_stop;

            if (!valid_packettype) {
                on_error(tr::f_("Unexpected packet type ({})"
                    " received from: {}, ignored."
                    , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                    , to_string(pitem->reader.saddr())));
                continue;
            }

            packet pkt;
            in.unseal(pkt);
            packetsize = pkt.packetsize;

            if (rc != packetsize) {
                on_error(tr::f_("Unexpected packet size ({})"
                    " received from: {}, expected: {}"
                    , rc
                    , to_string(pitem->reader.saddr())
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
                auto sender_uuid = pkt.addresser;

                switch (packettype) {
                    case packet_type::regular:
                        this->data_received(sender_uuid
                            , std::string(pitem->b.data(), pitem->b.size()));
                        break;

                    case packet_type::hello: {
                        auto pos1 = _reader_ids.find(sock);

                        if (pos1 == _reader_ids.end()) {
                            throw error {
                                  make_error_code(errc::unexpected_error)
                                , tr::f_("p2p::engine::process_reader_input:"
                                    " no reader found by socket id: {}"
                                    " , need to fix algorithm.", sock)
                            };
                        }

                        auto index = pos1->second;

                        auto pos = _reader_uuids.find(sender_uuid);

                        if (pos != _reader_uuids.end() && pos->second != index) {
                            throw error {
                                  make_error_code(errc::unexpected_error)
                                , tr::f_("p2p::engine::process_reader_input:"
                                    " found inconsistency for reader with universal"
                                    " identifier: {}, need to fix algorithm."
                                    , sender_uuid)
                            };
                        }

                        _readers[index].second.uuid = sender_uuid;
                        _reader_uuids[sender_uuid] = index;
                         reader_ready(sender_uuid, pitem->reader.saddr().addr
                            , pitem->reader.saddr().port);

                        break;
                    }

                    // Initiating file sending from peer
                    case packet_type::file_credentials:
                        _transporter->process_file_credentials(sender_uuid, pitem->b);
                        break;

                    case packet_type::file_request:
                        _transporter->process_file_request(sender_uuid, pitem->b);
                        break;

                    case packet_type::file_stop:
                        _transporter->process_file_stop(sender_uuid, pitem->b);
                        break;

                    // File chunk received
                    case packet_type::file_chunk:
                        _transporter->process_file_chunk(sender_uuid, pitem->b);
                        break;

                    // File received completely
                    case packet_type::file_end:
                        _transporter->process_file_end(sender_uuid, pitem->b);
                        break;

                    case packet_type::file_state:
                        _transporter->process_file_state(sender_uuid, pitem->b);
                        break;
                }
            }
        } while (true);
    }

    void send_outgoing_packets ()
    {
        for (auto & item: _writers) {
            auto & output_queue = item.second.q;

            if (output_queue.empty())
                continue;

            std::size_t total_bytes_sent = 0;
            auto & writer = item.second.writer;

            output_envelope_type out;
            error err;

            while (!output_queue.empty()) {
                auto & item = output_queue.front();

                auto const & pkt = item.pkt;
                out.reset();   // Empty envelope
                out.seal(pkt); // Seal new data

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size <= PACKET_SIZE, "");

                bool overflow = false;
                auto bytes_sent = writer.send(data.data(), size, & overflow, & err);

                if (bytes_sent > 0) {
                    total_bytes_sent += bytes_sent;

                } else if (bytes_sent < 0) {
                    // Output queue overflow
                    if (overflow) {
                        // Skip sending for some milliseconds
                        _output_timepoint = current_timepoint() + _opts.overflow_timeout;
                        return;
                    } else {
                        on_error(tr::f_("send failure to {}: {}"
                            , to_string(writer.saddr()), err.what()));
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
