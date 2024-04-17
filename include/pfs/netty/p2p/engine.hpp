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
#include "hello_packet.hpp"
#include "file.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/memory.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/startup.hpp"
#include <bitset>
#include <limits>
#include <map>
#include <queue>
#include <thread>
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
 *
 * @deprecated Use `delivery_engine`, `discovery_engine`.
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

public:
    using input_envelope_type   = input_envelope<>;
    using output_envelope_type  = output_envelope<>;

private:
    using entity_id = std::uint64_t; // Zero value is invalid entity
    using client_poller_type    = typename EngineTraits::client_poller_type;
    using server_poller_type    = typename EngineTraits::server_poller_type;
    using server_type           = typename EngineTraits::server_type;
    using reader_type           = typename EngineTraits::reader_type;
    using writer_type           = typename EngineTraits::writer_type;
    using reader_id             = typename EngineTraits::reader_id_type;
    using writer_id             = typename EngineTraits::writer_id_type;
    using discovery_engine_type = discovery_engine<DiscoveryEngineBackend>;

    // TODO replace with INVALID_MESSAGE_ID
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

        // UNUSED
        // Number packets send in one iteration
        // int send_packet_limit {10};

        // UNUSED
        // Priority in percents of sending regular packets in relation to
        // sending of file chunk packets. Applicable to `send_packet_limit`.
        // int regular_packets_priority {80};

        typename FileTransporter::options filetransporter;
        typename discovery_engine_type::options discovery;

        // Fixed C2512 on MSVC 2017
        options () {}
    } _opts;

private:
    // TODO replace with message_unit
    struct oqueue_item
    {
        entity_id    id;
        universal_id addressee;
        packet       pkt;
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

    // Unique identifier for entity (message, file chunks) inside engine session.
    entity_id _entity_id {0};

    struct writer_account {
        universal_id uuid;
        bool can_write;
        bool connected; // Used to check complete channel
        writer_type writer;

        // Regular packets output queue
        oqueue_type regular_queue;

        // File chunks output queues (mapped by file identifier).
        std::map<universal_id, oqueue_type> chunks;

        // Raw data to send
        std::vector<char> raw;
    };

    using writer_collection_type = std::vector<std::pair<bool, writer_account>>;
    using writer_index = typename writer_collection_type::size_type;
    writer_collection_type               _writers;
    std::map<writer_id, writer_index>    _writer_ids;
    std::map<universal_id, writer_index> _writer_uuids;

    struct reader_account {
        universal_id uuid;
        reader_type reader;

        // Buffer to accumulate payload from input packets
        std::vector<char> b;

        // Buffer to accumulate raw data
        std::vector<char> raw;
    };

    using reader_collection_type = std::vector<std::pair<bool, reader_account>>;
    using reader_index = typename reader_collection_type::size_type;
    reader_collection_type               _readers;
    std::map<reader_id, reader_index>    _reader_ids;
    std::map<universal_id, reader_index> _reader_uuids;

    std::queue<universal_id> _peer_expiration_queue;

public: // Callbacks
    /**
     * Called when unrecoverable error occurred - engine became disfunctional.
     * It must be restarted.
     */
    mutable std::function<void (error const & err)> on_failure = [] (error const & err) {};

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
     * Called when channel (reader and writer available) established.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> channel_established
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
        , pfs::filesystem::path const & /*path*/
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
    engine (universal_id host_uuid, socket4_addr server_addr, options const & opts)
        : _host_uuid(host_uuid)
    {
        auto bad = false;
        std::string invalid_argument_desc;
        _opts = opts;

        on_failure = [] (error const & err) { fmt::println(stderr, "{}", err.what()); };

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
                  make_error_code(std::errc::invalid_argument)
                , invalid_argument_desc
            };

            throw err;
        }

        // Call befor any network operations
        startup();

        _server = server_type(server_addr);

        configure_discovery();
        configure_transporter();
    }

    ~engine ()
    {
        _reader_poller.reset();
        _writer_poller.reset();
        _transporter.reset();
        _discovery.reset();
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
        // Create and configure main server poller
        ////////////////////////////////////////////////////////////////////////
        {
            auto accept_proc = [this] (typename server_poller_type::native_socket_type listener_sock, bool & ok) {
                auto * reader = acquire_reader(listener_sock);

                if (!reader) {
                    ok = false;
                    return reader->kINVALID_SOCKET;
                }

                ok = true;
                return reader->native();
            };

            _reader_poller = pfs::make_unique<server_poller_type>(std::move(accept_proc));

            _reader_poller->on_listener_failure = [this] (typename server_poller_type::native_socket_type, error const & err) {
                this->on_failure(err);
            };

            _reader_poller->on_reader_failure = [this] (typename server_poller_type::native_socket_type sock, error const & err) {
                this->on_failure(err);

                auto paccount = locate_reader_account(sock);

                if (paccount && paccount->uuid != universal_id{}) {
                    deferred_expire_peer(paccount->uuid);
                } else {
                    LOG_TRACE_3("Failure on socket: socket={}, but reader account not found", sock);
                }
            };

            _reader_poller->ready_read = [this] (typename server_poller_type::native_socket_type sock) {
                process_reader_input(sock);
            };

            _reader_poller->disconnected = [this] (typename server_poller_type::native_socket_type sock) {
                auto paccount = locate_reader_account(sock);

                if (paccount && paccount->uuid != universal_id{}) {
                    LOG_TRACE_3("Reader socket disconnected: UUID={}, socket={}", paccount->uuid, sock);
                    deferred_expire_peer(paccount->uuid);
                } else {
                    LOG_TRACE_3("Reader socket disconnected: socket={}, but reader account not found", sock);
                }
            };
        }

        ////////////////////////////////////////////////////////////////////////
        // Create and configure writer poller
        ////////////////////////////////////////////////////////////////////////
        {
            _writer_poller = pfs::make_unique<client_poller_type>();

            _writer_poller->on_failure = [this] (typename client_poller_type::native_socket_type sock, error const & err) {
                this->on_failure(err);

                auto paccount = locate_writer_account(sock);

                if (paccount) {
                    LOG_TRACE_3("Failure on socket: UUID={}, socket={}", paccount->uuid, sock);
                    deferred_expire_peer(paccount->uuid);
                } else {
                    LOG_TRACE_3("Failure on socket: socket={}, but writer account not found", sock);
                }
            };

            _writer_poller->connection_refused = [this] (typename client_poller_type::native_socket_type sock) {
                auto paccount = locate_writer_account(sock);

                if (paccount) {
                    LOG_TRACE_3("Connection refused for: UUID={}, socket={}", paccount->uuid, sock);
                    deferred_expire_peer(paccount->uuid);
                } else {
                    LOG_TRACE_3("Connection refused for: socket={}, but writer account not found", sock);
                }
            };

            _writer_poller->connected = [this] (typename client_poller_type::native_socket_type sock) {
                LOG_TRACE_3("Connection established for: socket={}", sock);
                process_socket_connected(sock);
            };

            _writer_poller->disconnected = [this] (typename client_poller_type::native_socket_type sock) {
                auto paccount = locate_writer_account(sock);

                if (paccount) {
                    LOG_TRACE_3("Connection disconnected for: UUID={}, socket={}", paccount->uuid, sock);
                    deferred_expire_peer(paccount->uuid);
                } else {
                    LOG_TRACE_3("Connection disconnected for: socket={}, but writer account not found", sock);
                }
            };

            // Not need, writer sockets for write only
            _writer_poller->ready_read = [] (typename client_poller_type::native_socket_type) {};

            _writer_poller->can_write = [this] (typename client_poller_type::native_socket_type sock) {
                auto pos = _writer_ids.find(sock);

                if (pos != _writer_ids.end()) {
                    auto index = pos->second;
                    _writers[index].second.can_write = true;
                }
            };
        }

        try {
            _reader_poller->add_listener(_server);

            // Start listening on main listener
            _server.listen(_opts.listener_backlog);
        } catch (error const & ex) {
            on_failure(error {
                  ex.code()
                , tr::f_("start netty::p2p::engine failure: {}", ex.what())
            });
            return false;
        }

        return true;
    }

    void set_open_outcome_file (std::function<file (std::string const & /*path*/)> && f)
    {
        _transporter->open_outcome_file = std::move(f);
    }

private:
    std::chrono::milliseconds _current_poller_timeout {0};

public:
    /**
     * @return @c false if some error occurred, otherwise return @c true.
     */
    void loop ()
    {
        auto n1 = _reader_poller->poll(_current_poller_timeout);
        auto n2 = _writer_poller->poll((n1 <= 0)
            ? _current_poller_timeout : std::chrono::milliseconds{0});
        auto n3 = _discovery->discover((n1 <= 0 && n2 <= 0)
            ? _current_poller_timeout : std::chrono::milliseconds{0});

        _transporter->loop();

        send_outgoing_packets();

        if (n1 == 0 && n2 == 0 && n3 == 0) {
            _current_poller_timeout += std::chrono::milliseconds{1};

            if (_current_poller_timeout > std::chrono::milliseconds{10})
                _current_poller_timeout = std::chrono::milliseconds{10};
        } else {
            _current_poller_timeout = std::chrono::milliseconds{0};
        }

        // Expire peers
        while (!_peer_expiration_queue.empty()) {
            _discovery->expire_peer(_peer_expiration_queue.front());
            _peer_expiration_queue.pop();
        }
    }

    /**
     * Add discovery receiver.
     */
    void add_receiver (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value)
    {
        _discovery->add_receiver(src_saddr, local_addr);
    }

    void add_target (socket4_addr src_saddr, inet4_addr local_addr
        , std::chrono::seconds transmit_interval
        , std::chrono::seconds expiration_interval)
    {
        _discovery->add_target(src_saddr, local_addr, transmit_interval, expiration_interval);
    }

    void add_target (socket4_addr src_saddr, std::chrono::seconds transmit_interval
        , std::chrono::seconds expiration_interval)
    {
        _discovery->add_target(src_saddr, transmit_interval, expiration_interval);
    }

    /**
     * Clears all receivers and targets for discovery manager.
     */
    void reset_discovery ()
    {
        _discovery.reset();
        configure_discovery();
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
    entity_id send (universal_id addressee_id, char const * data, int len)
    {
        return enqueue_packets(addressee_id, packet_type_enum::regular, data, len);
    }

    entity_id send (universal_id addressee_id, std::string const & data)
    {
        return enqueue_packets(addressee_id, packet_type_enum::regular, data.c_str()
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
        , pfs::filesystem::path const & path)
    {
        return _transporter->send_file(addressee, fileid, path);
    }

    universal_id send_file (universal_id addressee, universal_id fileid
        , std::string const & path, std::string const & display_name
        , std::int64_t filesize)
    {
        return _transporter->send_file(addressee, fileid, path, display_name, filesize);
    }

    /**
     * Send file.
     *
     * @param addressee_id Addressee unique identifier.
     * @param path Path to sending file.
     *
     * @return Unique file identifier or invalid identifier on error.
     */
    universal_id send_file (universal_id addressee_id, std::string const & path)
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
    void process_failure (error && err)
    {
        on_failure(std::move(err));
    }

    // Deferred peer expiration
    void deferred_expire_peer (universal_id peer_uuid)
    {
        LOG_TRACE_2("Deferred peer expiration: {}", peer_uuid);
        _peer_expiration_queue.push(peer_uuid);
    }

    void release_peer (universal_id peer_uuid, socket4_addr saddr)
    {
        LOG_TRACE_2("Release peer: {}@{}", peer_uuid, to_string(saddr));

        release_reader(peer_uuid);
        release_writer(peer_uuid);

        // Erase file input pool
        if (_transporter)
            _transporter->expire_addresser(peer_uuid);

        peer_expired(peer_uuid, saddr.addr, saddr.port);
    }

    void configure_discovery ()
    {
        _discovery = pfs::make_unique<discovery_engine_type>(_host_uuid, _opts.discovery);

        _discovery->on_failure = [this] (error const & err) {
            this->on_failure(err);
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
            this->release_peer(peer_uuid, saddr);
        };
    }

    void configure_transporter ()
    {
        _transporter = pfs::make_unique<FileTransporter>(_opts.filetransporter);

        _transporter->on_failure = [this] (error const & err) {
            this->on_failure(err);
        };

        _transporter->addressee_ready = [this] (universal_id addressee) {
            auto * paccount = locate_writer_account(addressee);
            return paccount && (paccount->regular_queue.size() <= _opts.overflow_limit)
                ? true: false;
        };

        _transporter->ready_to_send = [this] (universal_id addressee
                , universal_id fileid
                , packet_type_enum packettype
                , char const * data, int len) {
            switch (packettype) {
                // Commands
                case packet_type_enum::file_credentials:
                case packet_type_enum::file_request:
                case packet_type_enum::file_stop:
                case packet_type_enum::file_state:
                    enqueue_packets(addressee, packettype, data, len);
                    break;
                // Data
                case packet_type_enum::file_begin:
                case packet_type_enum::file_end:
                case packet_type_enum::file_chunk:
                    enqueue_file_chunk(addressee, fileid, packettype, data, len);
                    break;
                default:
                    return;
            }
        };

        _transporter->upload_stopped = [this] (universal_id addressee
            , universal_id fileid) {

            auto * paccount = locate_writer_account(addressee);

            if (! paccount)
                return;

            paccount->chunks.erase(fileid);
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
                on_failure(error {
                      errc::wrong_checksum
                    , tr::f_("checksum does not match for income file: {} from {}"
                        , fileid, addresser)
                });
            }

            download_complete(addresser, fileid, path, success);
        };

        _transporter->download_interrupted = [this] (universal_id addresser
                , universal_id fileid) {
            download_interrupted(addresser, fileid);
        };
    }

    inline entity_id next_entity_id () noexcept
    {
        ++_entity_id;
        return _entity_id == INVALID_ENTITY ? ++_entity_id : _entity_id;
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
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_reader_consistency:"
                    " reader socket not found in collection (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto index = pos1->second;

        if (index >= _readers.size()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_reader_consistency:"
                    " index for reader item is out of bounds (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto alive = _readers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
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
        LOG_TRACE_2("Acquire reader by listener: socket={}", listener_id);

        auto reader = _server.accept_nonblocking(listener_id);

        reader_index index = 0;
        auto count = _readers.size();

        if (_readers.empty()) {
            _readers.reserve(32);
        } else {
            index = count;

            for (reader_index i = 0; i < count; i++) {
                if (!_readers[i].first) {
                    index = i;
                    break;
                }
            }
        }

        if (_readers.empty() || index == count) {
            // Free element not found, append new one to store reader account
            _readers.emplace_back(true, reader_account{});
            index = _readers.size() - 1;
            _readers.back().second.raw.reserve(64 * 1024);
            _readers.back().second.uuid   = universal_id{};
            _readers.back().second.reader = std::move(reader);
        } else {
            // Use free element to store reader account
            _readers[index].first = true;
            _readers[index].second.uuid = universal_id{};
            _readers[index].second.reader = std::move(reader);
            _readers[index].second.b.clear();
            _readers[index].second.raw.clear();
        }

        reader_id id = _readers[index].second.reader.native();

        _reader_ids[id] = index;

        // Will be assign when hello packet received
        //_reader_uuids[uuid] = index;

        LOG_TRACE_2("Reader acquired: index={}", index);

        return & _readers[index].second.reader;
    }

    reader_account * locate_reader_account (reader_id id)
    {
        auto pos = _reader_ids.find(id);

        if (pos == _reader_ids.end()) {
            LOG_TRACE_2("No reader located: socket={}", id);
            return nullptr;
        }

        check_reader_consistency(id);

        auto index = pos->second;

        return & _readers[index].second;
    }

    void release_reader (universal_id uuid)
    {
        LOG_TRACE_2("Release reader: uuid={}", uuid);

        auto pos1 = _reader_uuids.find(uuid);

        if (pos1 == _reader_uuids.end()) {
            LOG_TRACE_2("No reader found for release: uuid={}", uuid);
            return;
        }

        auto index = pos1->second;
        auto & item = _readers[index].second;

        check_reader_consistency(item.reader.native());

        auto pos2 = _reader_ids.find(item.reader.native());

        // Release reader indices
        _reader_uuids.erase(pos1);
        _reader_ids.erase(pos2);

        auto saddr = item.reader.saddr();

        if (_reader_poller)
            _reader_poller->remove(item.reader);

        // Alive is false, element at `index` can be reused
        _readers[index].first = false;

        item.uuid = universal_id{};
        item.reader.~reader_type();
        item.b.clear(); // No need to destroy, can be reused later
        item.raw.clear();

        LOG_TRACE_2("Reader released: uuid={}", uuid);

        // Notify
        reader_closed(uuid, saddr.addr, saddr.port);
    }

    void check_writer_consistency (writer_id id)
    {
        if (!_opts.check_writer_consistency)
            return;

        auto pos1 = _writer_ids.find(id);

        if (pos1 == _writer_ids.end()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer socket not found in collection (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto index = pos1->second;

        if (index >= _writers.size()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                " index for writer item is out of bounds (id={})"
                " , need to fix algorithm.", id)
            };
        }

        auto alive = _writers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " invalid writer in collection found (id={})"
                    " , need to fix algorithm.", id)
            };
        }

        auto & item = _writers[index].second;

        auto pos2 = _writer_uuids.find(item.uuid);

        if (pos2 == _writer_uuids.end()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
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
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer universal identifier not found in collection (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto index = pos2->second;

        if (index >= _writers.size()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " index for writer item is out of bounds (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto alive = _writers[index].first;

        if (!alive) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " invalid writer in collection found (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }

        auto & item = _writers[index].second;

        auto pos1 = _writer_ids.find(item.writer.native());

        if (pos1 == _writer_ids.end()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("p2p::engine::check_writer_consistency:"
                    " writer socket not found in collection (uuid={})"
                    " , need to fix algorithm.", uuid)
            };
        }
    }

    writer_type * acquire_writer (universal_id uuid)
    {
        LOG_TRACE_2("Acquire writer: uuid={}", uuid);

        writer_index index = 0;
        auto count = _writers.size();

        if (_writers.empty()) {
            _writers.reserve(32);
        } else {
            index = count;

            for (writer_index i = 0; i < count; i++) {
                if (!_writers[i].first) {
                    index = i;
                    break;
                }
            }
        }

        if (_writers.empty() || index == count) {
            _writers.emplace_back(true, writer_account{});
            index = _writers.size() - 1;
            auto & wref = _writers[index];
            wref.second.uuid = uuid;
            wref.second.can_write = false;
            wref.second.connected = false;
            wref.second.writer = writer_type{};
            wref.second.raw.reserve(PACKET_SIZE * 10);
        } else {
            auto & wref = _writers[index];
            wref.first = true;
            wref.second.uuid = uuid;
            wref.second.can_write = false;
            wref.second.connected = false;
            wref.second.writer = writer_type{};
            wref.second.regular_queue.clear();
            wref.second.chunks.clear();
            wref.second.raw.clear();
        }

        writer_id id = _writers[index].second.writer.native();

        _writer_ids[id] = index;
        _writer_uuids[uuid] = index;

        LOG_TRACE_2("Writer acquired: uuid={}, index={}", uuid, index);

        return & _writers[index].second.writer;
    }

    writer_account * locate_writer_account (writer_id id)
    {
        auto pos = _writer_ids.find(id);

        if (pos == _writer_ids.end()) {
            LOG_TRACE_2("No writer account located: socket={}", id);
            return nullptr;
        }

        check_writer_consistency(id);

        auto index = pos->second;

        return & _writers[index].second;
    }

    writer_account * locate_writer_account (universal_id uuid)
    {
        auto pos = _writer_uuids.find(uuid);

        if (pos == _writer_uuids.end()) {
            LOG_TRACE_2("No writer account located: uuid={}", uuid);
            return nullptr;
        }

        check_writer_consistency(uuid);

        auto index = pos->second;

        return & _writers[index].second;
    }

    void connect_writer (universal_id uuid, socket4_addr remote_saddr)
    {
        LOG_TRACE_2("Connect writer to server: uuid={}, remote_saddr={}", uuid
            , to_string(remote_saddr));
        auto * writer = acquire_writer(uuid);
        auto conn_state = writer->connect(remote_saddr);
        _writer_poller->add(*writer, conn_state);
    }

    // Release writer by universal identifier
    void release_writer (universal_id uuid)
    {
        LOG_TRACE_2("Release writer: uuid={}", uuid);

        auto pos = _writer_uuids.find(uuid);

        if (pos == _writer_uuids.end()) {
            LOG_TRACE_2("No writer found for release: uuid={}", uuid);
            return;
        }

        auto index = pos->second;
        auto & item = _writers[index].second;

        release_writer(item.writer.native());
    }

    // Release writer by native identifier
    void release_writer (writer_id id)
    {
        LOG_TRACE_2("Release writer: socket={}", id);

        check_writer_consistency(id);

        auto pos1 = _writer_ids.find(id);
        auto index = pos1->second;

        // Alive is false, element at `index` can be reused
        _writers[index].first = false;

        auto & item = _writers[index].second;
        auto pos2 = _writer_uuids.find(item.uuid);

        // Release writer indices
        _writer_ids.erase(pos1);
        _writer_uuids.erase(pos2);

        auto saddr = item.writer.saddr();

        if (_writer_poller)
            _writer_poller->remove(item.writer);

        auto uuid = item.uuid;

        item.uuid = universal_id{};
        item.writer.disconnect();
        item.writer.~writer_type();
        item.regular_queue.clear(); // No need to destroy, can be reused later
        item.chunks.clear();
        item.raw.clear();

        LOG_TRACE_2("Writer released: socket={}, uuid={}", id, uuid);

        // Notify
        writer_closed(uuid, saddr.addr, saddr.port);
    }

    void check_complete_channel (universal_id peer_uuid)
    {
        LOG_TRACE_2("Check complete channel: peer={}", peer_uuid);

        int complete = 0;
        auto reader_pos = _reader_uuids.find(peer_uuid);

        if (reader_pos != _reader_uuids.end())
            complete++;

        auto const * paccount = locate_writer_account(peer_uuid);

        if (paccount != nullptr && paccount->connected)
            complete++;

        if (complete >= 2) {
            auto saddr = paccount->writer.saddr();
            LOG_TRACE_2("Channel complete (established): peer={}, saddr={}", peer_uuid, to_string(saddr));
            channel_established(peer_uuid, saddr.addr, saddr.port);
        }
    }

    // TODO replace with netty::p2p::enqueue_packets
    entity_id enqueue_packets_helper (oqueue_type * q, universal_id addressee
        , packet_type_enum packettype, char const * data, int len)
    {
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
            q->push(oqueue_item{entityid, addressee, std::move(p)});
        }

        return entityid;
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    entity_id enqueue_packets (universal_id addressee
        , packet_type_enum packettype, char const * data, int len)
    {
        auto * paccount = locate_writer_account(addressee);

        if (! paccount)
            return INVALID_ENTITY;

        return enqueue_packets_helper(& paccount->regular_queue, addressee
            , packettype, data, len);
    }

    entity_id enqueue_file_chunk (universal_id addressee, universal_id fileid
        , packet_type_enum packettype, char const * data, int len)
    {
        auto * paccount = locate_writer_account(addressee);

        if (! paccount)
            return INVALID_ENTITY;

        auto pos = paccount->chunks.find(fileid);

        if (pos == paccount->chunks.end()) {
            auto res = paccount->chunks.emplace(fileid, oqueue_type{});
            pos = res.first;
        }

        oqueue_type * q = & pos->second;

        return enqueue_packets_helper(q, addressee, packettype, data, len);
    }

    void process_peer_discovered (universal_id peer_uuid, socket4_addr saddr
        , std::chrono::milliseconds const & timediff)
    {
        LOG_TRACE_2("Process peer discovered: peer={}, saddr={}", peer_uuid, to_string(saddr));
        connect_writer(peer_uuid, saddr);
        peer_discovered(peer_uuid, saddr.addr, saddr.port, timediff);
    }

    void process_socket_connected (writer_id id)
    {
        LOG_TRACE_2("Process socket connected: socket={}", id);

        auto * paccount = locate_writer_account(id);

        if (!paccount)
            return;

        _writer_poller->wait_for_write(paccount->writer);

        paccount->connected = true;

        writer_ready(paccount->uuid, paccount->writer.saddr().addr
            , paccount->writer.saddr().port);

        auto pkt = _discovery->serialize_default_hello_packet();
        enqueue_packets(paccount->uuid, packet_type_enum::hello, pkt.data(), pkt.size());

        LOG_TRACE_2("Socket connected and enqueued `hello` packet: socket={}", id);
        check_complete_channel(paccount->uuid);
    }

    void process_reader_input (reader_id sock)
    {
        auto * paccount = locate_reader_account(sock);

        if (!paccount)
            return;

        auto * pbuffer = & paccount->raw;

        while (pbuffer->size() < PACKET_SIZE) {
            auto available = paccount->reader.available();

            auto offset = pbuffer->size();
            pbuffer->resize(offset + available);

            auto n = paccount->reader.recv(pbuffer->data() + offset, available);

            if (n < 0) {
                on_failure(error {
                      errc::socket_error
                    , tr::f_("Receive data failure from: {}", to_string(paccount->reader.saddr()))
                });

                // Expire peer and release resources allocated for it
                deferred_expire_peer(paccount->uuid);
                return;
            }

            pbuffer->resize(offset + n);

            if (n == 0)
                break;

            if (n < PACKET_SIZE)
                break;
        }

        auto available = pbuffer->size();

        if (available < PACKET_SIZE)
            return;

        auto pbuffer_pos = pbuffer->begin();

        do {
            input_envelope_type in {& *pbuffer_pos, PACKET_SIZE};
            available -= PACKET_SIZE;
            pbuffer_pos += PACKET_SIZE;

            static_assert(sizeof(packet_type_enum) <= sizeof(char), "");

            auto packettype = static_cast<packet_type_enum>(in.peek());
            decltype(packet::packetsize) packetsize {0};

            auto valid_packettype
                 = packettype == packet_type_enum::regular
                || packettype == packet_type_enum::hello
                || packettype == packet_type_enum::file_credentials
                || packettype == packet_type_enum::file_request
                || packettype == packet_type_enum::file_chunk
                || packettype == packet_type_enum::file_begin
                || packettype == packet_type_enum::file_end
                || packettype == packet_type_enum::file_state
                || packettype == packet_type_enum::file_stop;

            if (!valid_packettype) {
                on_failure(error {
                      std::make_error_code(std::errc::bad_message)
                    , tr::f_("unexpected packet type ({}) received from: {}, ignored."
                        , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                        , to_string(paccount->reader.saddr()))
                });

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                deferred_expire_peer(paccount->uuid);

                pbuffer_pos = pbuffer->end();
                break;
            }

            packet pkt;
            in.unseal(pkt);
            packetsize = pkt.packetsize;

            if (PACKET_SIZE != packetsize) {
                on_failure(error {
                      std::make_error_code(std::errc::bad_message)
                    , tr::f_("Unexpected packet size ({}) received from: {}, expected: {}"
                        , PACKET_SIZE
                        , to_string(paccount->reader.saddr())
                        , packetsize)
                });

                // There is a problem in communication (or this engine
                // implementation is wrong). Expiration can restore
                // functionality at next connection (after discovery).
                deferred_expire_peer(paccount->uuid);

                pbuffer_pos = pbuffer->end();
                break;
            }

            // Start new sequence
            if (pkt.partindex == 1)
                paccount->b.clear();

            append_chunk(paccount->b, pkt.payload, pkt.payloadsize);

            // Message complete
            if (pkt.partindex == pkt.partcount) {
                auto sender_uuid = pkt.addresser;

                switch (packettype) {
                    case packet_type_enum::regular:
                        LOG_TRACE_3("Data received from: {} {} bytes", sender_uuid, paccount->b.size());
                        this->data_received(sender_uuid
                            , std::string(paccount->b.data(), paccount->b.size()));
                        break;

                    case packet_type_enum::hello: {
                        auto pos1 = _reader_ids.find(sock);

                        if (pos1 == _reader_ids.end()) {
                            process_failure(error {
                                  make_error_code(pfs::errc::unexpected_error)
                                , tr::f_("p2p::engine::process_reader_input:"
                                    " no reader found by socket id: {},"
                                    " need to fix algorithm", sock)
                            });

                            return;
                        }

                        auto index = pos1->second;

                        auto pos = _reader_uuids.find(sender_uuid);

                        if (pos != _reader_uuids.end() && pos->second != index) {
                            process_failure(error {
                                  make_error_code(pfs::errc::unexpected_error)
                                , tr::f_("p2p::engine::process_reader_input:"
                                    " found inconsistency for reader with universal"
                                    " identifier: {}, need to fix algorithm"
                                    , sender_uuid)
                            });

                            return;
                        }

                        // Complete reader account
                        _readers[index].second.uuid = sender_uuid;
                        _reader_uuids[sender_uuid] = index;
                        reader_ready(sender_uuid, paccount->reader.saddr().addr
                            , paccount->reader.saddr().port);

                        auto pwriter = locate_writer_account(sender_uuid);

                        if (!pwriter) {
                            // TODO This call may be superfluous, check
                            _discovery->force_peer_discovered(paccount->reader.saddr()
                                , paccount->b.data(), paccount->b.size());
                        }

                        check_complete_channel(sender_uuid);

                        break;
                    }

                    // Initiating file sending from peer
                    case packet_type_enum::file_credentials:
                        _transporter->process_file_credentials(sender_uuid, paccount->b);
                        break;

                    case packet_type_enum::file_request:
                        _transporter->process_file_request(sender_uuid, paccount->b);
                        break;

                    case packet_type_enum::file_stop:
                        _transporter->process_file_stop(sender_uuid, paccount->b);
                        break;

                    // File chunk received
                    case packet_type_enum::file_chunk:
                        _transporter->process_file_chunk(sender_uuid, paccount->b);
                        break;

                    // File
                    case packet_type_enum::file_begin:
                        _transporter->process_file_begin(sender_uuid, paccount->b);
                        break;

                    // File received completely
                    case packet_type_enum::file_end:
                        _transporter->process_file_end(sender_uuid, paccount->b);
                        break;

                    case packet_type_enum::file_state:
                        _transporter->process_file_state(sender_uuid, paccount->b);
                        break;
                }
            }
        } while (available >= PACKET_SIZE);

        if (available == 0)
            pbuffer->clear();
        else
            pbuffer->erase(pbuffer->begin(), pbuffer_pos);
    }

    /**
     * Serializes packets to send.
     *
     * @param raw Buffer to store packets as raw bytes before sending.
     * @param output_queue Queue that stores output packets.
     * @param limit Number of messages/chunks to store as contiguous sequence
     *        of bytes.
     */
    void serialize_outgoing_packets (std::vector<char> * raw
        , oqueue_type * output_queue, int limit)
    {
        output_envelope_type out;

        while (limit && !output_queue->empty()) {
            auto & oitem = output_queue->front();
            auto const & pkt = oitem.pkt;

            if (pkt.partindex == pkt.partcount)
                --limit;

            out.reset();   // Empty envelope
            out.seal(pkt); // Seal new data

            auto data = out.data();
            auto size = data.size();
            auto offset = raw->size();
            raw->resize(offset + size);
            std::memcpy(raw->data() + offset, data.c_str(), size);
            output_queue->pop();
        }
    }

    void send_outgoing_data (writer_account * paccount)
    {
        std::size_t total_bytes_sent = 0;

        error err;
        bool break_sending = false;

        // TODO Implement throttling
        // std::this_thread::sleep_for(std::chrono::milliseconds{10});

        while (!break_sending && !paccount->raw.empty()) {
            int nbytes_to_send = PACKET_SIZE * 10;

            if (nbytes_to_send > paccount->raw.size())
                nbytes_to_send = static_cast<int>(paccount->raw.size());

            auto sendresult = paccount->writer.send(paccount->raw.data()
                , nbytes_to_send, & err);

            switch (sendresult.state) {
                case netty::send_status::failure:
                    on_failure(error {
                          err.code()
                        , tr::f_("send failure to {}: {}"
                            , to_string(paccount->writer.saddr()), err.what())
                    });

                    // Expire peer and release resources allocated for it
                    deferred_expire_peer(paccount->uuid);
                    break_sending = true;
                    break;

                case netty::send_status::network:
                    on_failure(error {
                          err.code()
                        , tr::f_("send failure to {}: network failure: {}"
                            , to_string(paccount->writer.saddr()), err.what())
                    });

                    // Expire peer and release resources allocated for it
                    deferred_expire_peer(paccount->uuid);
                    break_sending = true;
                    break;

                case netty::send_status::again:
                case netty::send_status::overflow:
                    if (paccount->can_write != false) {
                        paccount->can_write = false;
                        _writer_poller->wait_for_write(paccount->writer);
                    }

                    break_sending = true;
                    break;

                case netty::send_status::good:
                    if (sendresult.n > 0) {
                        total_bytes_sent += sendresult.n;
                        paccount->raw.erase(paccount->raw.begin()
                            , paccount->raw.begin() + sendresult.n);
                    }

                    break;
            }
        }
    }

    void send_outgoing_packets ()
    {
        for (auto & witem: _writers) {
            auto * paccount = & witem.second;

            if (!paccount->can_write)
                continue;

            // Serialize (bufferize) packets to send
            if (paccount->raw.size() < PACKET_SIZE) {
                auto & output_queue = paccount->regular_queue;

                if (!output_queue.empty()) {
                    // Serialize non-file_chunk (priority) packets.
                    serialize_outgoing_packets(& paccount->raw, & output_queue, 10);
                }

                if (!paccount->chunks.empty()) {
                    auto pos  = paccount->chunks.begin();
                    auto last = paccount->chunks.end();

                    while (pos != last) {
                        oqueue_type & chunks_output_queue = pos->second;

                        if (!chunks_output_queue.empty()) {
                            serialize_outgoing_packets(& paccount->raw, & chunks_output_queue, 10);
                            ++pos;
                        } else {
                            auto complete = !_transporter->request_chunk(paccount->uuid, pos->first);

                            if (complete)
                                pos = paccount->chunks.erase(pos);
                            else
                                ++pos;
                        }
                    }
                }
            }

            // Send serialized (bufferized) data
            send_outgoing_data(paccount);
        }
    }
};

}} // namespace netty::p2p
