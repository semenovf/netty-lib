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
#include "pfs/netty/error.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <array>
#include <atomic>
#include <bitset>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cmath>

namespace netty {
namespace p2p {

namespace {
constexpr std::chrono::milliseconds DEFAULT_DISCOVERY_INTERVAL {  5000};
constexpr std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT { 10000};
constexpr std::chrono::milliseconds DEFAULT_OVERFLOW_TIMEOUT   {   100};
constexpr std::int32_t DEFAULT_LISTENER_BACKLOG {64};
constexpr std::size_t  DEFAULT_BUFFER_BULK_SIZE {64 * 1024};
constexpr std::size_t  MAX_BUFFER_SIZE          {100000};
constexpr filesize_t   DEFAULT_FILE_CHUNK_SIZE  {16 * 1024};
constexpr filesize_t   MAX_FILE_SIZE            {0x7ffff000};
constexpr char const * INPUT_QUEUE_NAME  = "input queue";
constexpr char const * OUTPUT_QUEUE_NAME = "output queue";
constexpr char const * FILE_QUEUE_NAME   = "file queue";
constexpr char const * UNSUITABLE_VALUE  = tr::noop_("Unsuitable value for option: {}");
constexpr char const * BAD_VALUE         = tr::noop_("Bad value for option: {}");
}

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
    using checksum_type = pfs::crypto::sha256_digest;

    static constexpr entity_id INVALID_ENTITY {0};

    enum class option_enum: std::int16_t
    {
          download_directory // fs::path
//         , auto_download      // bool
        , remove_transient_files_on_error // bool
        , file_chunk_size    // std::int32_t
        , max_file_size      // std::int32_t
        , discovery_address  // socket4_addr
        , listener_address   // socket4_addr

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        , listener_backlog   // std::int32_t

        // Discovery transmit interval
        , transmit_interval  // std::chrono::milliseconds
        , expiration_timeout // std::chrono::milliseconds
        , poll_interval      // std::chrono::milliseconds

        , download_progress_granularity // std::int32_t (from 0 to 100)
    };

private:
    struct options
    {
        // Directory to store received files
        fs::path download_directory;

        // FIXME REMOVE
//         // Directory to store incomplete (downloading) and complete (downloaded)
//         // files.
//         fs::path transient_directory;
//
//         // Directory to store upload file info
//         fs::path cache_directory;

        // Automatically send file request packet after receiving file credentials.
//         bool auto_download {false};

        // Automatically remove transient files on error
        bool remove_transient_files_on_error {false};

        filesize_t file_chunk_size {DEFAULT_FILE_CHUNK_SIZE};
        filesize_t max_file_size {MAX_FILE_SIZE};

        socket4_addr discovery_address;
        socket4_addr listener_address;
        std::int32_t listener_backlog {DEFAULT_LISTENER_BACKLOG};

        std::chrono::milliseconds transmit_interval {DEFAULT_DISCOVERY_INTERVAL};
        std::chrono::milliseconds expiration_timeout {DEFAULT_EXPIRATION_TIMEOUT};
        std::chrono::milliseconds poll_interval {10};

        // The dowload progress granularity (from 0 to 100) determines the
        // frequency of download progress notification. 0 means notification for
        // all download progress and 100 means notification only when the
        // download is complete.
        std::int32_t download_progress_granularity {1}; // Notify every 1% change
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

    struct socket_info
    {
        universal_id uuid; // Valid for writers (connected sockets as opposite to accepted) only
        socket_type  sock;
        socket4_addr saddr;
    };

    using socket_container = std::list<socket_info>;
    using socket_iterator  = typename socket_container::iterator;

private:
    // TODO Only for standalone mode
    //std::atomic_flag _running_flag = ATOMIC_FLAG_INIT;

    static std::atomic<int> _instance_count;

    options _opts;

    std::bitset<4> _efficiency_loss_bits {0};

    // Unique identifier for entity (message, file chunks) inside engine session.
    entity_id _entity_id {0};

    universal_id _uuid;
    socket_type  _listener;

    // Used to solve problems with output overflow
    std::chrono::milliseconds _output_timepoint;

    // Discovery specific members
    struct {
        discovery_socket_type     receiver;
        discovery_socket_type     transmitter;
        std::chrono::milliseconds last_timepoint;
        std::vector<socket4_addr> targets;
    } _discovery;

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
        universal_id addressee;
        ifile_t data_file;
        bool at_end;          // true if file transferred
        pfs::crypto::sha256 hash;
    };

    struct ifile_item
    {
        ofile_t desc_file;
        ofile_t data_file;
        filesize_t filesize;
        pfs::crypto::sha256 hash;
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
     * Message dispatched
     *
     * Do not call engine::send() method from this callback.
     */
    mutable std::function<void (entity_id)> data_dispatched;

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , filesize_t /*downloaded_size*/
        , filesize_t /*total_size*/)> download_progress;

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , fs::path const & /*path*/
        , bool /*success*/)> download_complete;

        // FIXME REMOVE
//     mutable std::function<void (universal_id /*addresser*/
//         , universal_id /*fileid*/
//         , std::string const & /*filename*/
//         , filesize_t /*filesize*/)> download_begin;

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

    void ensure_directory (fs::path const & dir) const
    {
        if (!fs::exists(dir)) {
            std::error_code ec;

            if (!fs::create_directories(dir, ec)) {
                throw error{make_error_code(errc::filesystem_error)
                    , tr::f_("Create directory failure: {}: {}", dir, ec.message())};
            }

            fs::permissions(dir
                , fs::perms::owner_all | fs::perms::group_all
                , fs::perm_options::replace
                , ec);

            if (ec) {
                throw error{make_error_code(errc::filesystem_error)
                    , tr::f_("Set directory permissions failure: {}: {}"
                        , dir, ec.message())};
            }
        }
    }

    fs::path make_transientfilepath (universal_id addresser, universal_id fileid
        , std::string const & ext) const
    {
        auto dir = _opts.download_directory
            / fs::utf8_decode(to_string(addresser))
            / PFS__LITERAL_PATH("transient");

        if (!fs::exists(dir))
            ensure_directory(dir);

        return dir / fs::utf8_decode(to_string(fileid) + ext);
    }

    inline fs::path make_descfilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".desc");
    }

    inline fs::path make_datafilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".data");
    }

    inline fs::path make_donefilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".done");
    }

    inline fs::path make_errfilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".err");
    }

    fs::path make_cachefilepath (universal_id fileid) const
    {
        auto dir = _opts.download_directory / PFS__LITERAL_PATH(".cache");

        if (!fs::exists(dir))
            ensure_directory(dir);

        return dir / fs::utf8_decode(to_string(fileid) + ".desc");
    }

    fs::path make_targetfilepath (universal_id addresser, std::string const & filename) const
    {
        auto dir = _opts.download_directory / fs::utf8_decode(to_string(addresser));

        if (!fs::exists(dir))
            ensure_directory(dir);

        return dir / fs::utf8_decode(filename);
    }

    void remove_transient_files (universal_id addresser, universal_id fileid)
    {
        auto descfilepath = make_descfilepath(addresser, fileid);
        auto datafilepath = make_datafilepath(addresser, fileid);
        auto donefilepath = make_donefilepath(addresser, fileid);
        auto errfilepath  = make_errfilepath(addresser, fileid);

        fs::remove(descfilepath);
        fs::remove(datafilepath);
        fs::remove(donefilepath);
        fs::remove(errfilepath);
    }

    std::vector<char> read_chunk (file const & data_file, filesize_t count)
    {
        std::vector<char> chunk(count);
        auto n = data_file.read(chunk.data(), count);

        if (n == 0)
            chunk.clear();
        else if (n < count)
            chunk.resize(n);

        return chunk;
    }

    template <typename P>
    void unseal (std::vector<char> const & buffer, P & payload)
    {
        input_envelope_type in {buffer.data(), buffer.size()};
        in.unseal(payload);
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

    socket_info * locate_writer (universal_id const & uuid)
    {
        auto pos = _writers.find(uuid);

        if (pos != _writers.end())
            return & *pos->second;

        return nullptr;
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

    inline opool_item * locate_opool_item (universal_id const & peer_uuid
        , bool ensure)
    {
        auto pos = _opool.find(peer_uuid);

        if (pos == _opool.end()) {
            if (ensure) {
                auto res = _opool.emplace(peer_uuid, opool_item{});
                LOG_TRACE_2("Peer added to output pool: {}", peer_uuid);
                return & res.first->second;
            } else {
                return nullptr;
            }
        }

        return & pos->second;
    }

    /**
     * Locates @c ifile_item by @a fileid.
     *
     * @details If @a ensure is @c true and if @c ifile_item not found, then
     *          the new item emplaced into the pool.
     */
    ifile_item * locate_ifile_item (universal_id addresser
        , universal_id const & fileid, bool ensure)
    {
        auto pos = _ifile_pool.find(fileid);

        if (pos == _ifile_pool.end()) {
            if (ensure) {
                auto descfilepath = make_descfilepath(addresser, fileid);
                auto datafilepath = make_datafilepath(addresser, fileid);

                auto desc_file = file::open_write_only(descfilepath);
                auto data_file = file::open_write_only(datafilepath);

                auto res = _ifile_pool.emplace(fileid, ifile_item{
                      std::move(desc_file)
                    , std::move(data_file)
                    , 0
                    , pfs::crypto::sha256{}
                });

                return & res.first->second;
            } else {
                return nullptr;
            }
        }

        return & pos->second;
    }

    inline void remove_ifile_item (universal_id fileid)
    {
        _ifile_pool.erase(fileid);
    }

    inline void remove_ofile_item (universal_id fileid)
    {
        _ofile_pool.erase(fileid);
    }

    /**
     * Splits @a data into packets and enqueue them into output queue.
     */
    entity_id enqueue_packets (universal_id addressee
        , packet_type packettype
        , char const * data, std::streamsize len)
    {
        auto * pitem = locate_opool_item(addressee, true);

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

    /**
     * Save file credentials for incoming file if not exists.
     */
    void cache_incoming_file_credentials (universal_id addresser
        , file_credentials const & fc)
    {
        auto descfilepath = make_descfilepath(addresser, fc.fileid);

        if (!fs::exists(descfilepath)) {
            auto datafilepath = make_datafilepath(addresser, fc.fileid);

            auto desc_file = file::open_write_only(descfilepath, truncate_enum::on);
            // Only create/reset file
            auto data_file = file::open_write_only(datafilepath, truncate_enum::on);

            desc_file.write(fc.offset);
            desc_file.write(fc.filesize);
            desc_file.write(reinterpret_cast<char const *>(fc.filename.c_str()), fc.filename.size());
        }
    }

    /**
     * Load file credentials for incoming file
     */
    void load_incoming_file_credentials (universal_id addresser, universal_id fileid
        , file_credentials * fc)
    {
        auto descfilepath = make_descfilepath(addresser, fileid);
        auto desc_file = file::open_read_only(descfilepath);

        fc->fileid = fileid;

        desc_file.read(fc->offset);
        desc_file.read(fc->filesize);
        fc->filename = desc_file.read_all();
    }

    void cache_file_credentials (universal_id fileid, fs::path const & abspath)
    {
        auto cachefilepath = make_cachefilepath(fileid);
        file::rewrite(cachefilepath, fs::utf8_encode(abspath));
    }

    void uncache_file_credentials (universal_id fileid)
    {
        auto cachefilepath = make_cachefilepath(fileid);
        fs::remove(cachefilepath);
    }

    void commit_chunk (universal_id addresser, file_chunk const & fc)
    {
        // Returns non-null pointer or throws an exception
        auto * p = locate_ifile_item(addresser, fc.fileid, true);

        PFS__ASSERT(p, "");

        // Write data chunk
        p->data_file.set_pos(fc.offset);

//         if (ec == std::errc::resource_unavailable_try_again) {
//             // FIXME Need to slow down bitrate
//             LOG_TRACE_1("-- RESOURCE TEMPORARY UNAVAILABLE");
//             p->data_file.close();
//             p->desc_file.close();
//             std::abort();
//         }

        filesize_t last_offset = p->data_file.offset();

        p->data_file.write(fc.chunk.data(), fc.chunk.size());
        p->hash.update(fc.chunk.data(), fc.chunk.size());

        // Write offset
        filesize_t offset = p->data_file.offset();

        p->desc_file.set_pos(0);
        p->desc_file.write(offset);

        if (p->filesize > 0) {
            if (download_progress) {
                auto pass_download_progress = p->filesize == offset
                    || _opts.download_progress_granularity == 0;

                if (!pass_download_progress) {
                    if (_opts.download_progress_granularity > 0) {
                        int last_percents = std::round(100.0f * (static_cast<float>(last_offset) / p->filesize));
                        int percents = std::round(100.0f * (static_cast<float>(offset) / p->filesize));

                        if (percents > last_percents
                                && (percents % _opts.download_progress_granularity) == 0) {
                            pass_download_progress = true;
                        }
                    }
                }

                if (pass_download_progress)
                    download_progress(addresser, fc.fileid, offset, p->filesize);
            }
        }
    }

    // Complete or stop file sending
    void complete_file (universal_id fileid, bool /*success*/)
    {
        uncache_file_credentials(fileid);
        remove_ofile_item(fileid);
    }

    // Complete or stop file sending
    void stop_file (universal_id fileid)
    {
        remove_ofile_item(fileid);
    }

    /**
     * Commit income file.
     */
    void commit_income_file (universal_id addresser, universal_id fileid
        , checksum_type checksum)
    {
        auto * p = locate_ifile_item(addresser, fileid, false);

        if (!p) {
            throw pfs::error{make_error_code(std::errc::no_such_file_or_directory)
                , tr::f_("ifile item not found by fileid: {}", fileid)};
        }

        auto descfilepath = make_descfilepath(addresser, fileid);

        file_credentials fc;
        load_incoming_file_credentials(addresser, fileid, & fc);

        if (checksum == p->hash.digest()) {
            auto donefilepath = make_donefilepath(addresser, fileid);
            auto datafilepath = make_datafilepath(addresser, fileid);
            auto targetfilepath = make_targetfilepath(addresser, fc.filename);

            fs::rename(descfilepath, donefilepath);
            fs::rename(datafilepath, targetfilepath);
            notify_file_status(addresser, fileid, file_status::success);

            if (download_complete)
                download_complete(addresser, fileid, targetfilepath, true);
        } else {
            auto errfilepath = make_errfilepath(addresser, fileid);
            fs::rename(descfilepath, errfilepath);
            notify_file_status(addresser, fileid, file_status::checksum);

            if (download_complete) {
                failure(tr::f_("Checksum does not match for income file: {} from {}"
                    , fileid, addresser));
                download_complete(addresser, fileid, fs::path{}, false);
            }

            if (_opts.remove_transient_files_on_error)
                remove_transient_files(addresser, fileid);
        }

        remove_ifile_item(fileid);
    }

    /**
     * Notify receiver about file status
     */
    void notify_file_status (universal_id addressee
        , universal_id fileid
        , file_status filestatus)
    {
        file_state fs;
        fs.fileid = fileid;
        fs.status = filestatus;

        output_envelope_type out;
        out << fs;

        LOG_TRACE_3("Notify file status: {} (fileid={}, status:{})"
            , addressee
            , fileid
            , filestatus);

        enqueue_packets(addressee, packet_type::file_state
            , out.data().data(), out.data().size());
    }

    /**
     * Resume download files
     */
//     void resume_downloads (universal_id addresser_id)
//     {
//         auto dir = _opts.download_directory
//             / fs::utf8_decode(to_string(addresser_id))
//             / PFS__LITERAL_PATH("transient");
//
//         if (fs::exists(dir)) {
//             for (auto const & dir_entry: fs::directory_iterator{dir}) {
//                 if (dir_entry.path().extension() == ".desc") {
//                     universal_id fileid = pfs::from_string<universal_id>(dir_entry.path().stem());
//                     send_file_request(addresser_id, fileid);
//                 }
//             }
//         }
//     }

public:
    /**
     * Initializes underlying APIs and constructs engine instance.
     *
     * @param uuid Unique identifier for this instance.
     * @param pec Pointer to store error code while initialization.
     *        If @a pec is @c nullptr an exception occurs on error.
     */
    engine (universal_id uuid, std::error_code * pec = nullptr)
        : _uuid(uuid)
        , _connecting_poller("connecting")
        , _poller("main")
    {
        if (_instance_count == 0) {
            std::error_code ec;

            if (DiscoverySocketAPI::startup(ec)) {
                if (ReliableSocketAPI::startup(ec)) {
                    ;
                } else {
                    DiscoverySocketAPI::cleanup(ec);
                }
            }

            if (ec) {
                if (pec) {
                    *pec = ec;
                    return;
                }

                throw pfs::error {ec};
            }
        }

        failure = [] (std::string const & s) {
            fmt::print(stderr, "{}\n", s);
        };

        writer_ready              = [] (universal_id, inet4_addr, std::uint16_t) {};
        rookie_accepted           = [] (universal_id, inet4_addr, std::uint16_t) {};
        peer_closed               = [] (universal_id, inet4_addr, std::uint16_t) {};
        data_received             = [] (universal_id, std::string) {};
        data_dispatched           = [] (entity_id) {};
        download_progress         = [] (universal_id /*receiver_id*/
            , universal_id /*fileid*/
            , filesize_t /*downloaded_size*/
            , filesize_t /*total_size*/) {};

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

        ++_instance_count;
    }

    ~engine ()
    {
        _connecting_poller.release();
        _poller.release();
        _writers.clear();
        _expiration_timepoints.clear();

        for (auto & sock: _index_by_socket_id) {
            auto pos = sock.second;
            auto uuid = pos->uuid;
            auto addr = pos->saddr.addr;
            auto port = pos->saddr.port;

            LOG_TRACE_1("Socket closing: {} (socket address={}:{}; socket id={})"
                , uuid
                , to_string(addr)
                , port
                , sock.first);

            pos->sock.close();

            LOG_TRACE_1("Socket closed: {} (socket address={}:{}; socket id={})"
                , uuid
                , to_string(addr)
                , port
                , sock.first);
        }

        _index_by_socket_id.clear();
        _sockets.clear();

        if (--_instance_count == 0) {
            std::error_code ec;
            DiscoverySocketAPI::cleanup(ec);
            ReliableSocketAPI::cleanup(ec);
        }
    }

    engine (engine const &) = delete;
    engine & operator = (engine const &) = delete;

    engine (engine &&) = default;
    engine & operator = (engine &&) = default;

    universal_id const & uuid () const noexcept
    {
        return _uuid;
    }

    bool start ()
    {
        bool success = true;

        success = success && _connecting_poller.initialize();
        success = success && _poller.initialize();
        success = success && _discovery.receiver.bind(_opts.discovery_address.addr
            , _opts.discovery_address.port);
        success = success && _listener.bind(_opts.listener_address.addr
            , _opts.listener_address.port);
        success = success && _listener.listen(_opts.listener_backlog);

        if (success)
            _poller.add(_listener.id());

        if (success) {
            LOG_TRACE_2("Discovery listener backend: {}"
                , _discovery.receiver.backend_string());
            LOG_TRACE_2("General listener backend: {}"
                , _listener.backend_string());
            LOG_TRACE_2("Discovery listener: {}:{}. Status: {}"
                , to_string(_opts.discovery_address.addr)
                , _opts.discovery_address.port
                , _discovery.receiver.state_string());
            LOG_TRACE_2("General listener: {}:{}. Status: {}"
                , to_string(_opts.listener_address.addr)
                , _opts.listener_address.port
                , _listener.state_string());

            auto listener_options = _listener.dump_options();

            LOG_TRACE_3("General listener options (socket id={})", _listener.id());

            for (auto const & opt_item: listener_options) {
                LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
            }
        }

        auto now = current_timepoint();

        _discovery.last_timepoint = now > _opts.transmit_interval
            ? now - _opts.transmit_interval : now;

        _output_timepoint = now;

        return success;
    }

    bool set_option (option_enum opttype, fs::path const & path)
    {
        switch (opttype) {
            case option_enum::download_directory: {
                auto dir  = path;

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

                _opts.download_directory = std::move(dir);

                return true;
            }

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, socket4_addr sa)
    {
        switch (opttype) {
            case option_enum::discovery_address:
                _opts.discovery_address.addr = sa.addr;
                _opts.discovery_address.port = sa.port;
                return true;
            case option_enum::listener_address:
                _opts.listener_address.addr = sa.addr;
                _opts.listener_address.port = sa.port;
                return true;
            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, std::chrono::milliseconds msecs)
    {
        switch (opttype) {
            case option_enum::transmit_interval:
                _opts.transmit_interval = msecs;
                return true;
            case option_enum::expiration_timeout:
                _opts.expiration_timeout = msecs;
                return true;
            case option_enum::poll_interval:
                _opts.poll_interval = msecs;
                return true;

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
    }

    bool set_option (option_enum opttype, std::intmax_t value)
    {
        switch (opttype) {
//             case option_enum::auto_download:
//                 _opts.auto_download = (value != 0);
//                 return true;

            case option_enum::remove_transient_files_on_error:
                _opts.remove_transient_files_on_error = (value != 0);
                return true;

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

            case option_enum::listener_backlog:
                if (value <= 0 && value > (std::numeric_limits<std::int32_t>::max)()) {
                    failure(tr::f_(BAD_VALUE, opttype));
                    return false;
                }
                _opts.listener_backlog = value;
                return true;

            case option_enum::download_progress_granularity:
                if (value < 0 && value > 100) {
                    failure(tr::f_(BAD_VALUE, opttype));
                    return false;
                }
                _opts.download_progress_granularity = value;
                return true;

            default:
                break;
        }

        failure(tr::f_(UNSUITABLE_VALUE, opttype));
        return false;
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
        _discovery.targets.emplace_back(socket4_addr{addr, port});

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
     * @param addressee_id Addressee unique identifier.
     * @param data Data to send.
     * @param len  Data length.
     * @param priority Priority for sending data.
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
     * @details Actually this method enqueue file send credentials into output queue.
     *
     * @param addressee_id Addressee unique identifier.
     * @param file_id Unique file identifier. If @a file_id is invalid it
     *        will be generated automatically.
     * @param path Path to sending file.
     * @param csum Checksum for file.
     *
     * @return Unique file identifier or invalid identifier on error.
     */
    universal_id send_file (universal_id addressee_id
        , universal_id file_id
        , fs::path const & path)
    {
        if (fs::file_size(path) > _opts.max_file_size) {
            this->failure(tr::f_("Unable to send file: {}, file too big."
                " Max size is {} bytes", path, _opts.max_file_size));
            return universal_id{};
        }

        auto * pitem = locate_opool_item(addressee_id, true);

        PFS__ASSERT(pitem, "");

        try {
            file::open_read_only(path);
        } catch (...) {
            this->failure(tr::f_("Unable to send file: {}: file not found or"
                " has no permissions", path));
            return universal_id{};
        }

        if (file_id == universal_id{})
            file_id = pfs::generate_uuid();

        file_credentials fc {
              file_id
            , path.filename()
            , static_cast<filesize_t>(fs::file_size(path))
            , 0
        };

        LOG_TRACE_3("Send file credentials: {} (file id={}; size={} bytes)"
            , path, file_id, fc.filesize);

        cache_file_credentials(file_id, fs::absolute(path));

        output_envelope_type out;
        out << fc;

        enqueue_packets(addressee_id, packet_type::file_credentials
            , out.data().data(), out.data().size());

        return file_id;
    }

    /**
     * Send file.
     *
     * @details Actually this method enqueue file send credentials into output queue.
     *
     * @param addressee_id Addressee unique identifier.
     * @param path Path to sending file.
     * @param sha256 SHA-2 digest for file.
     *
     * @return Unique file identifier or invalid identifier on error.
     */
    universal_id send_file (universal_id addressee_id
        , fs::path const & path
        , checksum_type csum)
    {
        return send_file(addressee_id, universal_id{}, path, csum);
    }

    /**
     * Send request to download file from @a addressee_id.
     */
    void send_file_request (universal_id addressee_id, universal_id file_id)
    {
        file_credentials fc;
        load_incoming_file_credentials(addressee_id, file_id, & fc);

        auto datafilepath = make_datafilepath(addressee_id, file_id);
        auto filesize = fs::file_size(datafilepath);

        LOGD("", "=== DATA FILE: {}; filesize={}; offset={}"
            , datafilepath, filesize, fc.offset);

        if (fc.offset > filesize) {
            this->failure(tr::f_("Original file size is less than offset stored in"
                " info file for: {}"
                ", using file size as an offset but the file may be corrupted"
                " when the download completes."
                , datafilepath));

            fc.offset = filesize;
        }

        auto * p = locate_ifile_item(addressee_id, file_id, true);

        if (p) {
            p->filesize = fc.filesize;
        }

        file_request fr { file_id, fc.offset };
        output_envelope_type out;
        out << fr;

        enqueue_packets(addressee_id, packet_type::file_request
            , out.data().data(), out.data().size());

        LOG_TRACE_3("Send file request: addressee={}; file={}; offset={}"
            , addressee_id, fr.fileid, fr.offset);
    }

    /**
     * Send request to download file from @a addressee_id.
     */
    void send_file_stop (universal_id addressee_id, universal_id file_id)
    {
        file_stop fs { file_id };
        output_envelope_type out;
        out << fs;

        enqueue_packets(addressee_id, packet_type::file_stop
            , out.data().data(), out.data().size());

        LOG_TRACE_3("Send file stop: addressee={}; file={}", addressee_id, fs.fileid);
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
        PFS__ASSERT(res.second, "");

        return pos;
    }

    void process_connected (socket_id sid)
    {
        auto it = _index_by_socket_id.find(sid);
        PFS__ASSERT(it != std::end(_index_by_socket_id), "");

        auto pos = it->second;
        auto status = pos->sock.state();

        PFS__ASSERT(status == socket_type::CONNECTED, "");

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
                PFS__ASSERT(res.second, "");

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
            LOG_TRACE_2("Peer (socket id={}) added to input pool", sock_id);
        } else {
            // Already exists
            item.b.clear();
            LOG_TRACE_2("Peer (socket id={}) reset in input pool", sock_id);
        }

        _poller.add(pos->sock.id()
            , poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
    }

    void close_socket (socket_id sid)
    {
        auto it = _index_by_socket_id.find(sid);
        PFS__ASSERT(it != std::end(_index_by_socket_id), "");

        auto pos = it->second;

        auto uuid = pos->uuid;
        auto addr = pos->saddr.addr;
        auto port = pos->saddr.port;

        LOG_TRACE_1("Socket closing: {} (socket address={}:{}; socket id={})"
            , uuid
            , to_string(addr)
            , port
            , sid);

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

        // Clear output pool
        auto opool_pos = _opool.find(uuid);

        if (opool_pos != _opool.end()) {
            _opool.erase(opool_pos);
            LOG_TRACE_2("Output pool item erased for socket: {}", uuid);
        }

        // Clear input pool for input socket
        auto ipool_pos = _ipool.find(sid);

        if (ipool_pos != _ipool.end()) {
            _ipool.erase(ipool_pos);
            LOG_TRACE_2("Input pool item erased for socket: {}", sid);
        }

        // Erase items from file output pool associated with closed socket.
        for (auto it = _ofile_pool.begin(); it != _ofile_pool.end();) {
            if (it->second.addressee == uuid) {
                it = _ofile_pool.erase(it);
            } else {
                ++it;
            }
        }

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
            auto rc = _poller.wait(_opts.poll_interval);

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
            // NOTE Maybe this code block is unused.
            if (sockinfo.sock.state() != socket_type::CONNECTED) {
                LOG_TRACE_2("Mark socket as expired (socket id={}; status={})"
                    , sid
                    , sockinfo.sock.state_string());
                mark_socket_as_expired(sid);
            }

            if (is_input_event) {
                process_socket_input(& sockinfo);
            } else {
//                 LOG_TRACE_2("PROCESS SOCKET EVENT (OUTPUT): id: {}. Status: {}"
//                     , sid
//                     , sockinfo.sock.state_string());
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

        LOG_TRACE_2("PROCESS SOCKET INPUT EVENT (socket id={}; status={})"
            , sockinfo->sock.id()
            , sockinfo->sock.state_string());

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

            static_assert(sizeof(packet_type) <= sizeof(char), "");

            auto packettype = static_cast<packet_type>(in.peek());
            decltype(packet::packetsize) packetsize {0};

            auto valid_packettype = packettype == packet_type::regular
                || packettype == packet_type::file_credentials
                || packettype == packet_type::file_request
                || packettype == packet_type::file_chunk
                || packettype == packet_type::file_end
                || packettype == packet_type::file_state;

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
                    case packet_type::regular:
                        this->data_received(sender
                            , std::string(pitem->b.data(), pitem->b.size()));
                        break;

                    // Initiating file sending from peer
                    case packet_type::file_credentials: {
                        file_credentials fc;
                        unseal(pitem->b, fc);

                        // Cache incoming if file credentials if not exists
                        cache_incoming_file_credentials(sender, fc);

                        send_file_request(sender, fc.fileid);
                        break;
                    }

                    case packet_type::file_request: {
                        file_request fr;
                        unseal(pitem->b, fr);

                        auto cachefilepath = make_cachefilepath(fr.fileid);
                        auto orig_path = file::read_all(cachefilepath);

                        if (!orig_path.empty()) {
                            LOG_TRACE_3("File request received from {}: {}"
                                " (path={}; offset={})"
                                , sender, fr.fileid, orig_path, fr.offset);

                            auto data_file = file::open_read_only(fs::utf8_decode(orig_path));
                            data_file.set_pos(fr.offset);

                            // Add info to output pool for sending file
                            _ofile_pool.emplace(fr.fileid, ofile_item{
                                sender, std::move(data_file), false, pfs::crypto::sha256{}
                            });
                        }

                        break;
                    }

                    case packet_type::file_stop: {
                        file_stop fs;
                        unseal(pitem->b, fs);
                        stop_file(fs.fileid);
                        break;
                    }

                    // File chunk received
                    case packet_type::file_chunk: {
                        file_chunk fc;
                        unseal(pitem->b, fc);

                        LOG_TRACE_3("File chunk received from: {}"
                            " (fileid={}; offset={}; chunk size={})"
                            , sender, fc.fileid, fc.offset, fc.chunk.size());

                        commit_chunk(sender, fc);

                        break;
                    }

                    // File received completely
                    case packet_type::file_end: {
                        file_end fe;
                        unseal(pitem->b, fe);
                        commit_income_file(sender, fe.fileid, fe.checksum);
                        LOG_TRACE_2("File received completely from: {} (fileid={})"
                            , sender, fe.fileid);
                        break;
                    }

                    case packet_type::file_state: {
                        input_envelope_type in {pitem->b.data(), pitem->b.size()};
                        file_state fs;
                        in.unseal(fs);

                        switch (fs.status) {
                            // File received successfully by receiver
                            case file_status::success:
                                complete_file(fs.fileid, true);
                                break;

                            case file_status::checksum:
                                complete_file(fs.fileid, false);
                                break;

                            default:
                                this->failure(tr::f_("Unexpected file status ({})"
                                    " received from: {}:{}, ignored."
                                    , static_cast<std::underlying_type<file_status>::type>(fs.status)
                                    , to_string(sockinfo->saddr.addr)
                                    , sockinfo->saddr.port));
                                break;
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
                        // multicast / broadcast transmission)
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
        auto now = current_timepoint();
        bool interval_exceeded = false;

        if (_discovery.last_timepoint > now) {
            interval_exceeded = true;
        } else {
            interval_exceeded = (_discovery.last_timepoint
                + _opts.transmit_interval) <= now;
        }

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _uuid;
            packet.port = _opts.listener_address.port;

            packet.crc16 = crc16_of(packet);

            output_envelope_type out;
            out.seal(packet);

            auto data = out.data();
            auto size = data.size();

            PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

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
            if (it->second < now) {
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
        auto expiration_timepoint = now + _opts.expiration_timeout;

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

            PFS__ASSERT(sinfo->uuid == pool_item.first, "");

            auto status = sinfo->sock.state();

            if (status == socket_type::CLOSED || status == socket_type::BROKEN)
                mark_socket_as_expired(sinfo->sock.id());

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

                PFS__ASSERT(size <= PACKET_SIZE, "");

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

        for (auto it = _ofile_pool.begin(); it != _ofile_pool.end();) {
            // Check if addressee alive
            socket_info * sinfo = locate_writer(it->second.addressee);

            // Writer closed
            if (!sinfo) {
                // In normal this code is unreachable (see close_socket)
                ++it;
                continue;
            }

            if (it->second.at_end) {
                ++it;
                continue;
            }

            auto * opitem = locate_opool_item(it->second.addressee, false);

            if (opitem) {
                // Output queue is overloaded. File operations has low priority
                // so wait
                if (opitem->q.size() > 512) { // TODO Add option for this value
                    ++it;
                    continue;
                }
            } else {
                ++it;
                continue;
            }

            auto fileid  = it->first;
            auto * p = & it->second;
            auto offset = p->data_file.offset();

            file_chunk fc;
            fc.chunk = read_chunk(p->data_file, _opts.file_chunk_size);

            // End of file
            if (fc.chunk.empty()) {
                // File send completely, send `file_end` packet

                p->at_end = true;
                file_end fe { fileid, p->hash.digest() };

                output_envelope_type out;
                out << fe;

                enqueue_packets(it->second.addressee, packet_type::file_end
                    , out.data().data(), out.data().size());
            } else if (fc.chunk.size() > 0) {
                fc.fileid = it->first;
                fc.offset = offset;
                fc.chunksize = static_cast<filesize_t>(fc.chunk.size());

                output_envelope_type out;
                out << fc;

                LOG_TRACE_3("Send file chunk: {} (offset={}, chunk size={})"
                    , it->first, offset, fc.chunk.size() /*bytes_read*/);

                enqueue_packets(it->second.addressee, packet_type::file_chunk
                    , out.data().data(), out.data().size());

                p->hash.update(fc.chunk.data(), fc.chunk.size());
            }

            ++it;
        }
    }
};

template <
      typename DiscoverySocketAPI
    , typename ReliableSocketAPI
    , std::uint16_t PACKET_SIZE>
std::atomic<int> engine<DiscoverySocketAPI, ReliableSocketAPI, PACKET_SIZE>::_instance_count {0};

}} // namespace netty::p2p
