////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "file.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/sha256.hpp"
#include "pfs/netty/exports.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

// File transfer algorithm
//------------------------------------------------------------------------------
//         addresser                    addressee
//           ----                          ___
// send_file   |                            |
// ----------->|                            |
//             |-------file_credentials---->|
//             |                            |
//             |<--------file_request-------|
//             |                            |
//             |---------file_chunk-------->|
//             |---------file_chunk-------->|
//             |            ...             |
//             |---------file_chunk-------->|
//             |----------file_end--------->|
//             |                            |
//             |<--------file_state---------|
//             |                            |
//
// If addressee has already file credentials it can initiate the file transfering
// by sending `file_request` packet to addresser.
//

namespace netty {
namespace p2p {

class file_transporter
{
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

    static constexpr filesize_t const DEFAULT_FILE_CHUNK_SIZE {16 * 1024};
    static constexpr filesize_t const MIN_FILE_CHUNK_SIZE     {32};
    static constexpr filesize_t const MAX_FILE_CHUNK_SIZE     {1024 * 1024};
    static constexpr filesize_t const MAX_FILE_SIZE           {0x7ffff000};

public:
    using checksum_type = pfs::crypto::sha256_digest;

    struct options {
        pfs::filesystem::path download_directory;

        // The dowload progress granularity (from 0 to 100) determines the
        // frequency of download progress notification. 0 means notification for
        // all download progress and 100 means notification only when the
        // download is complete.
        int download_progress_granularity {1};

        filesize_t file_chunk_size {DEFAULT_FILE_CHUNK_SIZE};
        filesize_t max_file_size {MAX_FILE_SIZE};
        bool remove_transient_files_on_error {false};
    };

private:
    struct ifile_item
    {
        universal_id addresser;
        ofile_t desc_file;
        ofile_t data_file;
        filesize_t filesize;
        pfs::crypto::sha256 hash;
    };

    struct ofile_item
    {
        universal_id addressee;
        ifile_t data_file;
        bool at_end;          // true if file transferred
        pfs::crypto::sha256 hash;
    };

private:
    options _opts;

    std::unordered_map<universal_id/*fileid*/, ifile_item> _ifile_pool;
    std::unordered_map<universal_id/*fileid*/, ofile_item> _ofile_pool;

public:
    mutable std::function<void (std::string const &)> on_error
        = [] (std::string const &) {};

    /**
     * Called to check if addressee is busy or not available
     */
    mutable std::function<bool (universal_id /*addressee*/)> addressee_ready
        = [] (universal_id) { return false; };

    /**
     * Called when need to send prepared data
     */
    mutable std::function<void (universal_id /*addressee*/
        , packet_type /*packettype*/
        , char const * /*data*/, std::streamsize /*len*/)> ready_to_send
        = [] (universal_id, packet_type, char const *, std::streamsize) {};

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , filesize_t /*downloaded_size*/
        , filesize_t /*total_size*/)> download_progress
        = [] (universal_id, universal_id, filesize_t, filesize_t) {};

    /**
     * Called when file download completed successfully or with an error.
     */
    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , pfs::filesystem::path const & /*path*/
        , bool /*success*/)> download_complete
        = [] (universal_id, universal_id, pfs::filesystem::path const &, bool) {};

    /**
     * Called when file download interrupted, i.e. after peer closed.
     */
    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/)> download_interrupted
        = [] (universal_id, universal_id) {};

private:
    bool ensure_directory (pfs::filesystem::path const & dir, error * perr = nullptr) const;
    std::vector<char> read_chunk (file const & data_file, filesize_t count);

    pfs::filesystem::path make_transientfilepath (universal_id addresser
        , universal_id fileid
        , std::string const & ext) const;

    pfs::filesystem::path make_descfilepath (universal_id addresser
        , universal_id fileid) const;

    pfs::filesystem::path make_datafilepath (universal_id addresser
        , universal_id fileid) const;

    pfs::filesystem::path make_donefilepath (universal_id addresser
        , universal_id fileid) const;

    pfs::filesystem::path make_errfilepath (universal_id addresser
        , universal_id fileid) const;

    fs::path make_cachefilepath (universal_id fileid) const;
    fs::path make_targetfilepath (universal_id addresser, std::string const & filename) const;
    void remove_transient_files (universal_id addresser, universal_id fileid);

    /**
     * Locates @c ifile_item by @a fileid.
     *
     * @details If @a ensure is @c true and if @c ifile_item not found, then
     *          the new item emplaced into the pool.
     */
    ifile_item * locate_ifile_item (universal_id addresser
        , universal_id fileid, bool ensure);

    void remove_ifile_item (universal_id fileid);
    void remove_ofile_item (universal_id fileid);

    /**
     * Load file credentials for incoming file
     */
    file_credentials incoming_file_credentials (universal_id addresser
        , universal_id fileid);

    void cache_file_credentials (universal_id fileid
        , pfs::filesystem::path const & abspath);

    void uncache_file_credentials (universal_id fileid);

    void commit_chunk (universal_id addresser, file_chunk const & fc);
    void complete_file (universal_id fileid, bool /*success*/);

    /**
     * Notify receiver about file status
     */
    void notify_file_status (universal_id addressee, universal_id fileid
        , file_status state);

    /**
     * Commit income file.
     */
    void commit_incoming_file (universal_id addresser
        , universal_id fileid/*, checksum_type checksum*/);

    /**
     * Save file credentials for incoming file if not exists.
     */
    void cache_incoming_file_credentials (universal_id addresser
        , file_credentials const & fc);

public:
    NETTY__EXPORT file_transporter (options && opts);
    NETTY__EXPORT ~file_transporter ();

    /**
     * Sends a dozen file chunks (fragments)
     */
    NETTY__EXPORT void loop ();

    /**
     * Sets file size upper limit.
     */
    NETTY__EXPORT void set_max_file_size (filesize_t value);

    NETTY__EXPORT void process_file_credentials (universal_id sender, std::vector<char> const & data);
    NETTY__EXPORT void process_file_request (universal_id sender, std::vector<char> const & data);
    NETTY__EXPORT void process_file_stop (universal_id sender, std::vector<char> const & data);
    NETTY__EXPORT void process_file_chunk (universal_id sender, std::vector<char> const & data);
    NETTY__EXPORT void process_file_end (universal_id sender, std::vector<char> const & data);
    NETTY__EXPORT void process_file_state(universal_id sender, std::vector<char> const & data);

    /**
     * Remove output pool item associated with @a addressee.
     */
    NETTY__EXPORT void expire_addressee (universal_id addressee);

    /**
     * Remove input pool item associated with @a addresser.
     * Notify when active downloads are interrupted.
     */
    NETTY__EXPORT void expire_addresser (universal_id addresser);

    /**
     * Send file.
     *
     * @details Actually this method initiates file sending by send file
     *          credentials.
     *
     * @param addressee_id Addressee unique identifier.
     * @param fileid Unique file identifier. If @a file_id is invalid it
     *        will be generated automatically.
     * @param path Path to sending file.
     *
     * @return Unique file identifier or invalid identifier on error (file too
     *        big to send or has no permissions to read file).
     */
    NETTY__EXPORT universal_id send_file (universal_id addressee, universal_id fileid
        , pfs::filesystem::path const & path);

    /**
     * Send request to download file from @a addressee_id.
     */
    NETTY__EXPORT void send_file_request (universal_id addressee_id, universal_id file_id);

    /**
     * Stop file downloading and send command to @a addressee to stop
     * downloading file from it.
     */
    NETTY__EXPORT void stop_file (universal_id addressee, universal_id fileid);
};

}} // namespace netty::p2p
