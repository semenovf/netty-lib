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
#include "universal_id.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/sha256.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/p2p/packet.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

namespace netty {
namespace p2p {

class file_transporter
{
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

public:
    using checksum_type = pfs::crypto::sha256_digest;

    enum option_enum: std::int16_t
    {
          download_directory   // pfs::filesystem::path
        //, auto_download      // bool
        , remove_transient_files_on_error // bool
        , file_chunk_size      // std::int32_t
        , max_file_size        // std::int32_t

        // The dowload progress granularity (from 0 to 100) determines the
        // frequency of download progress notification. 0 means notification for
        // all download progress and 100 means notification only when the
        // download is complete.
        , download_progress_granularity // std::int32_t (from 0 to 100)
    };

private:
    struct options {
        pfs::filesystem::path download_directory;
        bool remove_transient_files_on_error;
        filesize_t file_chunk_size;
        filesize_t max_file_size;
        int download_progress_granularity;
    } _opts;

    struct ifile_item
    {
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

    std::unordered_map<universal_id/*fileid*/, ifile_item> _ifile_pool;
    std::unordered_map<universal_id/*fileid*/, ofile_item> _ofile_pool;

public:
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    /**
     * Called to check if addressee is busy or not available
     */
    mutable std::function<bool (universal_id /*addressee*/)> addressee_ready
        = [] (universal_id) { return true; };

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

    mutable std::function<void (universal_id /*addresser*/
        , universal_id /*fileid*/
        , pfs::filesystem::path const & /*path*/
        , bool /*success*/)> download_complete
        = [] (universal_id, universal_id, pfs::filesystem::path const &, bool) {};

private:
    bool ensure_directory (pfs::filesystem::path const & dir) const;
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
     * Send request to download file from @a addressee_id.
     */
    void send_file_request (universal_id addressee_id, universal_id file_id);

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
    void commit_income_file (universal_id addresser
        , universal_id fileid, checksum_type checksum);

    /**
     * Save file credentials for incoming file if not exists.
     */
    void cache_incoming_file_credentials (universal_id addresser
        , file_credentials const & fc);

public:
    NETTY__EXPORT file_transporter ();
    NETTY__EXPORT ~file_transporter ();

    /**
     * Sets @c path type options.
     *
     * @return @c true on success, or @c false if option is unsuitable or
     *         value is bad.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, pfs::filesystem::path const & value);

    /**
     * Sets boolean or integer type options.
     *
     * @return @c true on success, or @c false if option is unsuitable or
     *         value is bad.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, std::intmax_t value);

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
     * Send command to stop downloading file from @a addressee.
     */
    NETTY__EXPORT void send_stop_file (universal_id addressee, universal_id fileid);
};

}} // namespace netty::p2p
