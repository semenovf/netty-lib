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
#include "file.hpp"
#include "hello_packet.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/fmt.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/sha256.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include <array>
#include <bitset>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cassert>

namespace netty {
namespace p2p {

namespace {
constexpr std::chrono::milliseconds DEFAULT_DISCOVERY_INTERVAL {  5000};
constexpr std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT { 10000};
constexpr std::chrono::milliseconds DEFAULT_OVERFLOW_TIMEOUT   {   100};
constexpr std::size_t               DEFAULT_BUFFER_BULK_SIZE   {64 * 1024};
constexpr std::size_t               MAX_BUFFER_SIZE            {100000};
constexpr filesize_t                DEFAULT_FILE_CHUNK_SIZE    {16 * 1024};
constexpr filesize_t                MAX_FILE_SIZE              {0x7ffff000};
constexpr char const * INPUT_QUEUE_NAME  = "input queue";
constexpr char const * OUTPUT_QUEUE_NAME = "output queue";
constexpr char const * FILE_QUEUE_NAME   = "file queue";
constexpr char const * UNSUITABLE_VALUE  = tr::noop_("Unsuitable value for option: {}");
constexpr char const * BAD_VALUE         = tr::noop_("Bad value for option: {}");
}

enum class option_enum: std::int16_t
{
      download_directory // fs::path
    , auto_download      // bool
    , file_chunk_size    // std::int32_t
    , max_file_size      // std::int32_t
};

/**
 * @brief P2P delivery engine.
 *
 * @tparam DiscoverySocketAPI Discovery socket API.
 * @tparam ReliableSocketAPI Reliable socket API.
 * @tparam PACKET_SIZE Maximum packet size (must be less or equal to
 *         @c packet::MAX_PACKET_SIZE and greater than @c packet::PACKET_HEADER_SIZE).
 */
template <
      typename DiscoverySocketAPI
    , typename ReliableSocketAPI
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

private:
    struct options
    {
        // Directory to store received files
        fs::path download_directory;
        fs::path transient_directory;

        // Directory to store upload file info
        fs::path cache_directory;

        // Automatically send file request packet after receiving file credentials.
        bool auto_download {false};

        filesize_t file_chunk_size {DEFAULT_FILE_CHUNK_SIZE};
        filesize_t max_file_size {MAX_FILE_SIZE};
    };

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

    using discovery_socket_type = typename DiscoverySocketAPI::socket_type;
    using socket_type           = typename ReliableSocketAPI::socket_type;
    using poller_type           = typename ReliableSocketAPI::poller_type;
    using socket_id             = decltype(socket_type{}.id());
    using oqueue_type           = pfs::ring_buffer<oqueue_item, DEFAULT_BUFFER_BULK_SIZE>;

    struct socket_address
    {
        inet4_addr    addr;
        std::uint16_t port;
    };

    struct socket_info
    {
        universal_id   uuid; // Valid for writers (connected sockets as opposite to accepted) only
        socket_type    sock;
        socket_address saddr;
    };

    using socket_container = std::list<socket_info>;
    using socket_iterator  = typename socket_container::iterator;

private:
    // TODO Only for standalone mode
    //std::atomic_flag _running_flag = ATOMIC_FLAG_INIT;

    options _opts;

    std::bitset<4> _efficiency_loss_bits {0};

    // Unique identifier for entity (message, file chunks) inside engine session.
    entity_id _entity_id {0};

    universal_id   _uuid;
    socket_type    _listener;
    socket_address _listener_address;
    std::chrono::milliseconds _poll_interval {10};

    // Used to solve problems with output overflow
    std::chrono::milliseconds _output_timepoint;

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

    std::vector<socket_id> _expired_sockets;

    // Mapping socket identified by key (native handler) to socket iterator
    // (see _sockets)
    std::unordered_map<socket_id, socket_iterator> _index_by_socket_id;

    // Writer sockets
    std::unordered_map<universal_id, socket_iterator> _writers;

    std::unordered_map<socket_id, std::chrono::milliseconds> _expiration_timepoints;

    // Poller to observe socket status (from connecting to connected status)
    poller_type _connecting_poller;

    // Default poller
    poller_type _poller;

    // Input buffers pool
    // Buffers to accumulate payload from input packets
    struct ipool_item
    {
        std::vector<char> b;
    };

    std::unordered_map<socket_id, ipool_item> _ipool;

    // Packet output queue
    struct opool_item
    {
        oqueue_type q;
    };

    std::unordered_map<universal_id, opool_item> _opool;

    struct ofile_item
    {
        universal_id    addressee;
        ifile_handle_t  ifh;
    };

    struct ifile_item
    {
        ofile_handle_t info_ofh;
        ofile_handle_t data_ofh;
    };

    // file UUID ------------
    //                      |
    //                      v
    std::unordered_map<universal_id, ofile_item> _ofile_pool;
    std::unordered_map<universal_id, ifile_item> _ifile_pool;

public: // Callbacks
    mutable std::function<void (std::string const &)> failure;

    /**
     * Launched when new writer socket ready (connected).
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> writer_ready;

    /**
     * Launched when new address accepted by discoverer.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> rookie_accepted;

    /**
     * Launched when peer closed.
     */
    mutable std::function<void (universal_id, inet4_addr, std::uint16_t)> peer_closed;

    /**
     * Message received.
     */
    mutable std::function<void (universal_id, std::string)> data_received;

    /**
     * File credentials received (need too prepare to receive file content).
     */
    mutable std::function<void (universal_id, file_credentials)> file_credentials_received;

    /**
     * Message dispatched
     *
     * Do not call engine::send() method from this callback.
     */
    mutable std::function<void (entity_id)> data_dispatched;

private:
    inline entity_id next_entity_id () noexcept
    {
        ++_entity_id;
        return _entity_id == INVALID_ENTITY ? ++_entity_id : _entity_id;
    }

    inline std::chrono::milliseconds current_timepoint () const
    {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::steady_clock;

        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }

    inline void loss_efficiency (efficiency value) noexcept
    {
        _efficiency_loss_bits.set(static_cast<std::size_t>(value));
    }

    inline void restore_efficiency (efficiency value) noexcept
    {
        _efficiency_loss_bits.reset(static_cast<std::size_t>(value));
    }

    inline bool test_efficiency (efficiency value) const noexcept
    {
        return !_efficiency_loss_bits.test(static_cast<std::size_t>(value));
    }

    inline void append_chunk (std::vector<char> & vec, char const * buf, std::size_t len)
    {
        if (len > 0)
            vec.insert(vec.end(), buf, buf + len);
    }

    socket_info * locate_writer (universal_id const & uuid)
    {
        socket_info * result {nullptr};
        auto pos = _writers.find(uuid);

        if (pos != _writers.end())
            result = & *pos->second;

        return result;
    }

    inline ipool_item * locate_ipool_item (decltype(socket_type{}.id()) sock_id)
    {
        auto pos = _ipool.find(sock_id);

        if (pos == _ipool.end()) {
            failure(tr::f_("Cannot locate input pool item by socket id: {}", sock_id));
            return nullptr;
        }

        return & pos->second;
    }

    inline opool_item * locate_opool_item (universal_id const & peer_uuid)
    {
        auto pos = _opool.find(peer_uuid);

        if (pos == _opool.end()) {
            auto res = _opool.emplace(peer_uuid, opool_item{});
            LOG_TRACE_1("Peer added to output pool: {}", peer_uuid);
            return & res.first->second;
        }

        return & pos->second;
    }

    inline ifile_item * locate_ifile_item (universal_id const & fileid)
    {
        auto pos = _ifile_pool.find(fileid);

        if (pos == _ifile_pool.end()) {
            fs::path infofilepath = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".info");
            fs::path filepath     = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".data");

            std::error_code ec;
            auto info_ofh = open_ofile(infofilepath, CURRENT_OFFSET
                , truncate_enum::off, ec);

            if (info_ofh < 0) {
                this->failure(tr::f_("Open info file failure: {}: {}"
                    , infofilepath, ec.message()));
                return nullptr;
            }

            auto data_ofh = open_ofile(filepath, CURRENT_OFFSET
                , truncate_enum::off, ec);

            if (data_ofh < 0) {
                this->failure(tr::f_("Open data file failure: {}: {}"
                    , filepath, ec.message()));
                return nullptr;
            }

            auto res = _ifile_pool.emplace(fileid, ifile_item{info_ofh, data_ofh});

            return & res.first->second;
        }

        return & pos->second;
    }

    entity_id enqueue_packets (universal_id addressee
        , packet_enum packettype
        , char const * data, std::streamsize len)
    {
        auto * pitem = locate_opool_item(addressee);

        if (!pitem)
            return INVALID_ENTITY;

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
            p.addresser   = _uuid;
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

    bool save_file_credentials (file_credentials const & fc, filesize_t offset = 0)
    {
        fs::path infofilepath = _opts.transient_directory
            / fs::utf8_decode(to_string(fc.fileid) + ".info");
        fs::path filepath     = _opts.transient_directory
            / fs::utf8_decode(to_string(fc.fileid) + ".data");

        bool success = true;
        std::error_code ec;
        auto info_ofh = open_ofile(infofilepath, 0, truncate_enum::on, ec);

        if (info_ofh < 0)
            success = false;

        if (success) {
            // Only create/reset file
            auto data_ofh = open_ofile(filepath, 0, truncate_enum::on, ec);

            if (data_ofh < 0) {
                if (ec) {
                    this->failure(tr::f_("Save file credentials failure: {}: {}"
                        , filepath, ec.message()));
                }
                success = false;
            } else {
                close_file(data_ofh);
            }
        }

        if (success) {
            success =
                   ::write(info_ofh, reinterpret_cast<char const *>(& offset), sizeof(offset)) > 0
                && ::write(info_ofh, reinterpret_cast<char const *>(& fc.filesize), sizeof(fc.filesize)) > 0
                && ::write(info_ofh, reinterpret_cast<char const *>(fc.sha256.value.data())
                    , fc.sha256.value.size()) > 0
                && ::write(info_ofh, reinterpret_cast<char const *>(fc.filename.c_str())
                    , fc.filename.size()) > 0;

            if (!success)
                ec = std::error_code(errno, std::generic_category());
        }

        if (ec) {
            this->failure(tr::f_("Save file credentials failure: {}: {}"
                , infofilepath, ec.message()));
        }

        close_file(info_ofh);
        return success;
    }

    bool load_file_credentials (universal_id fileid, filesize_t * offset, file_credentials * fc)
    {
        fs::path path = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".info");
        bool success = true;
        std::error_code ec;

        auto ifh = open_ifile(path, 0, ec);

        if (ifh < 0)
            success = false;

        if (success) {
            fc->fileid = fileid;

            success =
                   ::read(ifh, offset, sizeof(*offset)) > 0
                && ::read(ifh, & fc->filesize, sizeof(fc->filesize)) > 0
                && ::read(ifh, fc->sha256.value.data(), fc->sha256.value.size()) > 0;

            if (success) {
                std::error_code ec;
                fc->filename = read_file(ifh, CURRENT_OFFSET, ec);

                if (ec) {
                    this->failure(tr::f_("Load file credentials failure: {}", path));
                    success = false;
                } else if (fc->filename.empty()) {
                    this->failure(tr::f_("File with file credentials may be corrupted: {}", path));
                    success = false;
                }
            }
        }

        close_file(ifh);
        return true;
    }

//     bool update_read_offset (universal_id fileid, filesize_t offset)
//     {
//         fs::path path = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".info");
//         auto ofh = open_ofile(path, 0, false);
//
//         if (ofh < 0)
//             return false;
//
//         // Reserve space for offset
//         bool success = ::write(ofh, reinterpret_cast<char const *>(& offset), sizeof(offset)) > 0
//             && ::fsync(ofh);
//
//         if (!success) {
//             auto ec = std::error_code(errno, std::generic_category());
//             this->failure(tr::f_("Update read offset failure: {}: {}"
//                 , path, ec.message()));
//         }
//
//         close_file(ofh);
//         return success;
//     }

    bool cache_file_credentials (fs::path const & path, file_credentials const & fc)
    {
        fs::path infofilepath = _opts.cache_directory / fs::utf8_decode(to_string(fc.fileid) + ".info");
        std::error_code ec;
        rewrite_file(infofilepath, fs::utf8_encode(path), ec);

        if (ec) {
            this->failure(tr::f_("Cache file credentials failure: {}: {}", path
                , ec.message()));
            return false;
        }

        return true;
    }

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
    engine (universal_id uuid)
        : _uuid(uuid)
        , _connecting_poller("connecting")
        , _poller("main")
    {
        _listener.failure = [this] (std::string const & error) {
            this->failure(error);
        };

        _discovery.receiver.failure = [this] (std::string const & error) {
            this->failure(error);
        };

        _discovery.transmitter.failure = [this] (std::string const & error) {
            this->failure(error);
        };

        _connecting_poller.failure = [this] (std::string const & error) {
            this->failure(error);
        };

        _poller.failure = [this] (std::string const & error) {
            this->failure(error);
        };
    }

    ~engine ()
    {
        _connecting_poller.release();
        _poller.release();
        _writers.clear();
        _expiration_timepoints.clear();

        for (auto & sock: _index_by_socket_id) {
            LOG_TRACE_1("Socket closing (socket id={})", sock.first);

            auto pos = sock.second;
            auto uuid = pos->uuid;
            auto addr = pos->saddr.addr;
            auto port = pos->saddr.port;

            pos->sock.close();

            LOG_TRACE_1("Socket closed: {} (socket address={}:{}; socket id={})"
                , uuid
                , to_string(addr)
                , port
                , sock.first);
        }

        _index_by_socket_id.clear();
        _sockets.clear();
    }

    engine (engine const &) = delete;
    engine & operator = (engine const &) = delete;

    engine (engine &&) = default;
    engine & operator = (engine &&) = default;

    universal_id const & uuid () const noexcept
    {
        return _uuid;
    }

    bool set_option (option_enum opttype, fs::path const & path)
    {
        switch (opttype) {
            case option_enum::download_directory: {
                auto download_dir  = path;
                auto transient_dir = path / PFS__LITERAL_PATH("transient");
                auto cache_dir     = path / PFS__LITERAL_PATH("cache");

                for (auto const & dir: {download_dir, transient_dir, cache_dir}) {
                    if (!fs::exists(dir)) {
                        std::error_code ec;

                        if (!fs::create_directory(dir, ec)) {
                            failure(tr::f_("Create directory failure: {}: {}"
                                , dir, ec.message()));
                        }

                        fs::permissions(dir
                            , fs::perms::owner_all | fs::perms::group_all
                            , fs::perm_options::replace
                            , ec);

                        if (ec) {
                            failure(tr::f_("Set directory permissions failure: {}: {}"
                                , dir, ec.message()));
                            return false;
                        }
                    }
                }

                _opts.download_directory  = std::move(download_dir);
                _opts.transient_directory = std::move(transient_dir);
                _opts.cache_directory     = std::move(cache_dir);

                return true;
            }

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, bool value)
    {
        switch (opttype) {
            case option_enum::auto_download:
                _opts.auto_download = value;
                return true;

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, std::int32_t value)
    {
        switch (opttype) {
            case option_enum::file_chunk_size:
                if (value <= 0) {
                    failure(tr::f_(BAD_VALUE, opttype));
                    return false;
                }
                _opts.file_chunk_size = value;
                return true;
            case option_enum::max_file_size:
                if (value <= 0 || value > MAX_FILE_SIZE) {
                    failure(tr::f_(BAD_VALUE, opttype));
                    return false;
                }
                _opts.max_file_size = value;
                return true;

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    // DEPRECATED
    // FIXME Replace with sequence of set_option() calls
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
            LOG_TRACE_2("Discovery listener backend: {}"
                , _discovery.receiver.backend_string());
            LOG_TRACE_2("General listener backend: {}"
                , _listener.backend_string());

            LOG_TRACE_2("Discovery listener: {}:{}. Status: {}"
                , to_string(c.discovery_address())
                , c.discovery_port()
                , _discovery.receiver.state_string());
            LOG_TRACE_2("General listener: {}:{}. Status: {}"
                , to_string(_listener_address.addr)
                , _listener_address.port
                , _listener.state_string());

            auto listener_options = _listener.dump_options();

            LOG_TRACE_3("General listener options (socket id={})", _listener.id());

            for (auto const & opt_item: listener_options) {
                LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
            }
        }

//         if (success) {
//             _file_chunk.resize(c.file_chunk_size());
//         }

        auto now = current_timepoint();

        _discovery.last_timepoint = now > _discovery.transmit_interval
            ? now - _discovery.transmit_interval : now;

        _output_timepoint = now;

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

        auto output_possible = current_timepoint() >= _output_timepoint;

        if (test_efficiency(efficiency::file_output)) {
            if (output_possible)
                enqueue_file_packets();
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

    bool add_discovery_target (inet4_addr const & addr, std::uint16_t port)
    {
        _discovery.targets.emplace_back(socket_address{addr, port});

        if (is_multicast(addr)) {
            auto success = _discovery.receiver.join_multicast_group(addr);

            if (success) {
                LOG_TRACE_2("Discovery receiver joined into multicast group: {}"
                    , to_string(addr));
            } else {
                return false;
            }
        }

        return true;
    }

    /**
     * Send data.
     *
     * @details Actually this method splits data to send into packets and
     *          enqueue them into output queue.
     *
     * @param addressee Addressee unique identifier.
     * @param data Data to send.
     * @param len  Data length.
     * @param priority Priority for sending data.
     *
     * @return Unique entity identifier.
     */
    entity_id send (universal_id addressee, char const * data
        , std::streamsize len)
    {
        return enqueue_packets(addressee, packet_enum::regular, data, len);
    }

    /**
     * Send file.
     *
     * @details Actually this method enqueue file send credentials into output queue.
     *
     * @param addressee Addressee unique identifier.
     * @param path Path to sending file.
     * @param sha256 SHA-2 digest for file.
     * @param offset File offset to start sending.
     *
     * @return Unique file identifier.
     */
    universal_id send_file (universal_id addressee
        , fs::path const & path
        , pfs::crypto::sha256_digest sha256
        , filesize_t offset = 0)
    {
        if (fs::file_size(path) > _opts.max_file_size) {
            this->failure(tr::f_("Unable to send file: {}, file too big."
                " Max size is {} bytes", path, _opts.max_file_size));
            return universal_id{};
        }

        auto * pitem = locate_opool_item(addressee);

        if (!pitem)
            return universal_id{};

        std::error_code ec;
        auto ifh = open_ifile(path, offset, ec);

        if (ifh < 0) {
            this->failure(tr::f_("Open file failure to send: {}: {}"
                , path, ec.message()));
            return universal_id{};
        }

        close_file(ifh);

        auto fileid = pfs::generate_uuid();

        file_credentials fc {
              fileid
            , path.filename()
            , static_cast<filesize_t>(fs::file_size(path))
            , sha256
        };

        if (!cache_file_credentials(fs::absolute(path), fc))
            return universal_id{};

        output_envelope_type out;
        out << fc;

        LOG_TRACE_2("###--- Send file credentials: {} ({})", fileid, path);

        enqueue_packets(addressee, packet_enum::file_credentials
            , out.data().data(), out.data().size());

        return fileid;
    }

    void send_file_request (universal_id addressee, universal_id fileid)
    {
        fs::path filepath     = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".data");
        fs::path infofilepath = _opts.transient_directory / fs::utf8_decode(to_string(fileid) + ".info");

        if (!fs::exists(filepath)) {
            failure(tr::f_("Send file request failure: file not found: {}"
                , filepath));
            return;
        }

        if (!fs::exists(infofilepath)) {
            failure(tr::f_("Send file request failure: info file not found: {}"
                , infofilepath));
            return;
        }

        auto filesize = fs::file_size(filepath);

        filesize_t offset = 0;
        file_credentials fc;
        auto success = load_file_credentials(fileid, & offset, & fc);

        if (!success)
            return;

        if (offset > filesize) {
            failure(tr::f_("Original file size is less than offset stored in"
                " info file for: {}"
                ", using file size as an offset but the file may be corrupted"
                " when the download completes."
                , filepath));

            offset = filesize;
        }

        file_request fr { fileid, offset };
        output_envelope_type out;
        out << fr;

        enqueue_packets(addressee, packet_enum::file_request
            , out.data().data(), out.data().size());

        LOG_TRACE_2("###--- Send file request for: {} (offset={})"
            , fr.fileid, fr.offset);
    }

private:
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

        LOG_TRACE_2("Socket connected to: {} (socket address={}:{}; socket id={}; status={})"
            , pos->uuid
            , to_string(pos->saddr.addr)
            , pos->saddr.port
            , sid
            , pos->sock.state_string());

        auto options = pos->sock.dump_options();

        LOG_TRACE_3("Connected socket options (socket id={})", sid);

        for (auto const & opt_item: options) {
            (void)opt_item;
            LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
        }

        _connecting_poller.remove(pos->sock.id());
        _poller.add(pos->sock.id(), poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);

        this->writer_ready(pos->uuid, pos->saddr.addr, pos->saddr.port);
        update_expiration_timepoint(pos->sock.id());
    }

    void connect_to_peer (universal_id peer_uuid
        , inet4_addr const & addr
        , std::uint16_t port)
    {
        bool is_indexing_socket   = false;
        bool is_connecting_socket = false;
        bool is_connected_socket  = false;

        socket_info sockinfo;

        sockinfo.sock.failure = [this] (std::string const & error) {
            this->failure(error);
        };

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
                    LOG_TRACE_2("Connecting to: {} (socket address={}:{}; socket id={}; status={})"
                        , peer_uuid
                        , to_string(pos->saddr.addr)
                        , pos->saddr.port
                        , pos->sock.id()
                        , pos->sock.state_string());

                    _connecting_poller.add(pos->sock.id());
                }

                if (is_connected_socket) {
                    LOG_TRACE_2("Connected to: {} (socket address={}:{}; socket id={}; status={})"
                        , peer_uuid
                        , to_string(pos->saddr.addr)
                        , pos->saddr.port
                        , pos->sock.id()
                        , pos->sock.state_string());
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
        auto sock_id = pos->sock.id();

        LOG_TRACE_2("Socket accepted (socket address={}:{}; socket id={}; status: {})"
            , to_string(pos->saddr.addr)
            , pos->saddr.port
            , sock_id
            , pos->sock.state_string());

        auto options = pos->sock.dump_options();

        LOG_TRACE_3("Accepted socket options (socket id={})", sock_id);

        for (auto const & opt_item: options) {
            (void)opt_item;
            LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
        }

        // Insert/reset item to input pool
        auto res = _ipool.emplace(sock_id, ipool_item{});
        auto & item = res.first->second;

        if (res.second) {
            // New inserted
            LOG_TRACE_1("Peer (socket id={}) added to input pool", sock_id);
        } else {
            // Already exists
            item.b.clear();
            LOG_TRACE_1("Peer (socket id={}) reset in input pool", sock_id);
        }

        _poller.add(pos->sock.id()
            , poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
    }

    void close_socket (socket_id sid)
    {
        LOG_TRACE_1("Socket closing (socket id={})", sid);

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

        LOG_TRACE_1("Socket closed: {} (socket address={}:{}; socket id={})"
            , uuid
            , to_string(addr)
            , port
            , sid);

        auto writers_erased = _writers.erase(uuid);
        _index_by_socket_id.erase(sid);
        _sockets.erase(pos);
        _expiration_timepoints.erase(sid);

        if (writers_erased > 0) {
            // TODO
            // Do not need to erase here input and output contexts associated
            // with peer. Where can i do it?

            this->peer_closed(uuid, addr, port);
        }
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
                LOG_TRACE_2("Mark socket as expired (socket id={}; status={})"
                    , sid
                    , sockinfo.sock.state_string());
                mark_socket_as_expired(sid);
            }

            if (is_input_event) {
                process_socket_input(& sockinfo);
            } else {
                //("PROCESS SOCKET EVENT (OUTPUT): id: {}. Status: {}"
                //    , sid
                //    , sockinfo.sock.state_string());
                ;
            }
        } else {
            this->failure(tr::f_("Socket not found by socket id: {}"
                ", may be it was closed before removing from poller"
                , sid));
        }
    }

    void process_socket_input (socket_info * sockinfo)
    {
        int read_iterations = 0;

        auto * pitem = locate_ipool_item(sockinfo->sock.id());

        do {
            std::array<char, PACKET_SIZE> buf;
            std::streamsize rc = sockinfo->sock.recvmsg(buf.data(), buf.size());

            if (rc == 0) {
                if (read_iterations == 0) {
                    this->failure(tr::f_("Expected any data from: {}:{}, but no data read"
                        , to_string(sockinfo->saddr.addr)
                        , sockinfo->saddr.port));
                }

                break;
            }

            if (rc < 0) {
                this->failure(tr::f_("Receive data from: {}:{} failure: {} (code={})"
                    , to_string(sockinfo->saddr.addr)
                    , sockinfo->saddr.port
                    , sockinfo->sock.error_string()
                    , sockinfo->sock.error_code()));
                break;
            }

            read_iterations++;

            // Ignore input
            if (!pitem)
                continue;

            input_envelope_type in {buf.data(), buf.size()};

            static_assert(sizeof(packet_enum) <= sizeof(char), "");

            auto packettype = static_cast<packet_enum>(in.peek());
            decltype(packet::packetsize) packetsize {0};

            auto valid_packettype = packettype == packet_enum::regular
                || packettype == packet_enum::file_credentials
                || packettype == packet_enum::file_request
                || packettype == packet_enum::file_chunk;

            if (!valid_packettype) {
                this->failure(tr::f_("Unexpected packet type ({})"
                    " received from: {}:{}, ignored."
                    , static_cast<std::underlying_type<decltype(packettype)>::type>(packettype)
                    , to_string(sockinfo->saddr.addr)
                    , sockinfo->saddr.port));
                continue;
            }

            packet pkt;
            in.unseal(pkt);
            packetsize = pkt.packetsize;

            if (rc != packetsize) {
                this->failure(tr::f_("Unexpected packet size ({})"
                    " received from: {}:{}, expected: {}"
                    , rc
                    , to_string(sockinfo->saddr.addr)
                    , sockinfo->saddr.port
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
                    case packet_enum::regular:
                        this->data_received(sender
                            , std::string(pitem->b.data(), pitem->b.size()));
                        break;

                    case packet_enum::file_credentials: {
                        input_envelope_type in {pitem->b.data(), pitem->b.size()};
                        file_credentials fc;
                        in.unseal(fc);

                        if (save_file_credentials(fc)) {
                            LOG_TRACE_2("###--- File credentials received from {}: {}"
                                " (file name: {}; size: {}; sha256: {})"
                                , sender
                                , fc.fileid
                                , fc.filename
                                , fc.filesize
                                , to_string(fc.sha256));

                            file_credentials_received(sender, std::move(fc));

                            if (_opts.auto_download) {
                                send_file_request(sender, fc.fileid);
                            }
                        }

                        break;
                    }

                    case packet_enum::file_request: {
                        input_envelope_type in {pitem->b.data(), pitem->b.size()};
                        file_request fr;
                        in.unseal(fr);

                        fs::path infofilepath = _opts.cache_directory / fs::utf8_decode(to_string(fr.fileid) + ".info");
                        std::error_code ec;
                        auto orig_path = read_file(infofilepath, 0, ec);

                        if (ec) {
                            this->failure(tr::f_("Read path from cache file failure: {}: {}"
                                , infofilepath, ec.message()));
                            break;
                        }

                        if (!orig_path.empty()) {
                            LOG_TRACE_2("###--- File request received from {}: {}"
                                " (path={}; offset={})"
                                , sender
                                , fr.fileid
                                , orig_path
                                , fr.offset);

                            std::error_code ec;

                            auto ifh = open_ifile(fs::utf8_decode(orig_path)
                                , fr.offset, ec);

                            if (ifh < 0) {
                                this->failure(tr::f_("Open file failure: {}: {}"
                                    , orig_path, ec.message()));
                                break;
                            }

                            _ofile_pool.emplace(fr.fileid, ofile_item{sender, ifh});
                        }

                        break;
                    }

                    case packet_enum::file_chunk: {
                        input_envelope_type in {pitem->b.data(), pitem->b.size()};
                        file_chunk fc;
                        in.unseal(fc);

                        // TODO Implement
                        LOG_TRACE_2("###--- File chunk received from: {}"
                            " (fileid={}; offset={}; chunk size={})"
                            , pkt.addresser, fc.fileid, fc.offset, fc.chunk.size());

                        auto * p = locate_ifile_item(fc.fileid);

                        if (p) {
                            std::error_code ec;
                            write_chunk(p->data_ofh, fc.offset
                                , reinterpret_cast<char const *>(fc.chunk.data())
                                , fc.chunk.size(), ec);

                            if (ec) {
                                this->failure(tr::f_("Write file chunk failure: {}: {}"
                                    , fc.fileid, ec.message()));
                            }
                        }

                        break;
                    }
                }
            }
        } while (true);
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
                    , char const * data, std::size_t size) {

                input_envelope_type in {data, size};
                hello_packet packet;

                in.unseal(packet);
                auto success = is_valid(packet);

                if (success) {
                    if (packet.crc16 == crc16_of(packet)) {
                        // Ignore self received packets (can happen during
                        // multicast / broadcast transmission )
                        if (packet.uuid != _uuid) {
                            update_peer(packet.uuid, sender_addr, packet.port);
                        }
                    } else {
                        this->failure(tr::f_("Bad CRC16 for HELO packet received from: {}:{}"
                            , to_string(sender_addr), sender_port));
                    }
                } else {
                    this->failure(tr::f_("Bad HELO packet received from: {}:{}"
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
                    this->failure(tr::f_("Transmit failure to {}:{}: {}"
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

    void update_peer (universal_id peer_uuid, inet4_addr const & addr, std::uint16_t port)
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

    void send_outgoing_packets ()
    {
        for (auto & pool_item: _opool) {
            auto & output_queue = pool_item.second.q;

            if (output_queue.empty())
                continue;

            std::size_t total_bytes_sent = 0;
            socket_info * sinfo = locate_writer(pool_item.first);

            // Socket connecting, wait
            if (!sinfo)
                continue;

            assert(sinfo->uuid == pool_item.first);

            auto status = sinfo->sock.state();

            if (status != socket_type::CONNECTED)
                continue;

            output_envelope_type out;

            while (!output_queue.empty()) {
                auto & item = output_queue.front();

                auto const & pkt = item.pkt;
                out.reset();   // Empty envelope
                out.seal(pkt); // Seal new data

                auto data = out.data();
                auto size = data.size();

                assert(size <= PACKET_SIZE);

                auto & sock = sinfo->sock;
                auto bytes_sent = sock.sendmsg(data.data(), size);

                if (bytes_sent > 0) {
                    total_bytes_sent += bytes_sent;

                    // Message sent complete
                    if (pkt.partindex == pkt.partcount)
                        this->data_dispatched(item.id);

                } else if (bytes_sent < 0) {
                    // Output queue overflow
                    if (sock.overflow()) {
                        // Skip sending for some milliseconds
                        _output_timepoint = current_timepoint() + DEFAULT_OVERFLOW_TIMEOUT;
                        return;
                    } else {
                        auto & saddr = sinfo->saddr;

                        this->failure(tr::f_("Sending failure to {} ({}:{}): {}"
                            , sinfo->uuid
                            , to_string(saddr.addr)
                            , saddr.port
                            , sock.error_string()));
                    }
                } else {
                    // FIXME Need to handle this state (broken connection ?)
                    ;
                }

                output_queue.pop();
            }
        }
    }

    // Enqueue chunk of file packets
    void enqueue_file_packets ()
    {
        if (_ofile_pool.empty())
            return;

        for (auto it = _ofile_pool.begin(); it != _ofile_pool.end(); ) {
            auto ifh = it->second.ifh;
            auto offset = file_offset(ifh);

            file_chunk fc;
            std::error_code ec;
            fc.chunk = read_file(ifh, _opts.file_chunk_size, CURRENT_OFFSET, ec); //it->second.ec);

            if (ec) {
                this->failure(tr::f_("File upload error: {}", it->first));
                close_file(ifh);
                it = _ofile_pool.erase(it);
                continue;
            }

            // End of file
            if (fc.chunk.empty()) {
                LOG_TRACE_2("###--- File upload complete: {}", it->first);
                close_file(ifh);
                it = _ofile_pool.erase(it);
                continue;
            }

            if (fc.chunk.size() > 0) {
                fc.fileid = it->first;
                fc.offset = offset;
                fc.chunksize = static_cast<filesize_t>(fc.chunk.size());

                output_envelope_type out;
                out << fc;

                LOG_TRACE_2("###--- Send file chunk: {} (offset={}, chunk size={})"
                    , it->first, offset, fc.chunk.size() /*bytes_read*/);

                enqueue_packets(it->second.addressee, packet_enum::file_chunk
                    , out.data().data(), out.data().size());
            }

            ++it;
        }
    }
};

}} // namespace netty::p2p
