////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
//      2024.04.22 Added `step` method (`loop` method deprecated).
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "file.hpp"
#include "packet.hpp"
#include "primal_serializer.hpp"
#include "universal_id.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/log.hpp"
#include "pfs/sha256.hpp"
#include "pfs/traverse_directory.hpp"
#include "pfs/netty/exports.hpp"
#include <functional>
#include <memory>
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
//             |---------file_begin-------->|
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

namespace fs = pfs::filesystem;

template <typename Serializer = primal_serializer<>>
class file_transporter
{
    static constexpr filesize_t DEFAULT_FILE_CHUNK_SIZE {16 * 1024};
    static constexpr filesize_t MIN_FILE_CHUNK_SIZE     {32};
    static constexpr filesize_t MAX_FILE_CHUNK_SIZE     {1024 * 1024};
    static constexpr filesize_t MAX_FILE_SIZE           {0x7ffff000};

public:
    using checksum_type = pfs::crypto::sha256_digest;

    struct options {
        pfs::filesystem::path download_directory;

        // The dowload progress granularity (from 0 to 100) determines the
        // frequency of download progress notification. 0 means notification for
        // all download progress and 100 means notification only when the
        // download is complete.
        int download_progress_granularity {1};

        // FIXME
//         chunksize_type file_chunk_size {DEFAULT_FILE_CHUNK_SIZE};
//         filesize_type max_file_size {MAX_FILE_SIZE};
        filesize_t file_chunk_size {DEFAULT_FILE_CHUNK_SIZE};
        filesize_t max_file_size {MAX_FILE_SIZE};

        bool remove_transient_files_on_error {false};
    };

private:
    // Income file
    struct ifile_item
    {
        universal_id  addresser;
        ofile_t desc_file;
        ofile_t data_file;
        filesize_t filesize;
    };

    struct ofile_item
    {
        universal_id addressee;
        ifile_t data_file;
        bool chunk_requested;
    };

private:
    options _opts;

    std::unordered_map<universal_id/*fileid*/, ifile_item> _ifile_pool;
    std::unordered_multimap<universal_id/*fileid*/, ofile_item> _ofile_pool;

public:
    mutable std::function<void (error const &)> on_failure
        = [] (error const &) {};

    /**
     * Called to check if addressee is busy or not available
     *
     * @deprecated
     */
    mutable std::function<bool (universal_id /*addressee*/)> addressee_ready
        = [] (universal_id) { return true; };

    /**
     * Called when need to send prepared data
     */
    mutable std::function<void (universal_id /*addressee*/, universal_id /*fileid*/
        , packet_type_enum /*packettype*/, std::vector<char>)> ready_send
        = [] (universal_id, universal_id, packet_type_enum, std::vector<char>) {};

    mutable std::function<void (universal_id /*addressee*/, universal_id /*fileid*/)> upload_stopped
        = [] (universal_id, universal_id) {};

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

    /**
     * Used to open outcome files.
     */
    mutable std::function<file (std::string const & /*path*/)> open_outcome_file
        = [] (std::string const & path) { return file::open_read_only(path); };

private:
    bool ensure_directory (pfs::filesystem::path const & dir, error * perr = nullptr) const
    {
        if (!fs::exists(dir)) {
            std::error_code ec;

            if (!fs::create_directories(dir, ec)) {
                error err = {
                    errc::filesystem_error
                    , tr::f_("create directory failure: {}", dir)
                    , ec.message()
                };

                if (perr) {
                    *perr = err;
                    return false;
                } else {
                    throw err;
                }
            }

            fs::permissions(dir
                , fs::perms::owner_all | fs::perms::group_all
                , fs::perm_options::replace
                , ec);

            if (ec) {
                error err = {
                    errc::filesystem_error
                    , tr::f_("set directory permissions failure: {}", dir)
                    , ec.message()
                };

                if (perr) {
                    *perr = err;
                    return false;
                } else {
                    throw err;
                }
            }
        }

        return true;
    }

    //std::vector<char> read_chunk (local_file & data_file, chunksize_type size);
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

    pfs::filesystem::path make_transientfilepath (universal_id addresser, universal_id fileid
        , std::string const & ext) const
    {
        auto dir = _opts.download_directory
            / fs::utf8_decode(to_string(addresser))
            / PFS__LITERAL_PATH("transient");

        if (!fs::exists(dir))
            ensure_directory(dir);

        return dir / fs::utf8_decode(to_string(fileid) + ext);
    }

    pfs::filesystem::path make_descfilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".desc");
    }

    pfs::filesystem::path make_datafilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".data");
    }

    pfs::filesystem::path make_donefilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".done");
    }

    pfs::filesystem::path make_errfilepath (universal_id addresser, universal_id fileid) const
    {
        return make_transientfilepath(addresser, fileid, ".err");
    }

    pfs::filesystem::path make_cachefilepath (universal_id fileid) const
    {
        auto dir = _opts.download_directory / PFS__LITERAL_PATH(".cache");

        if (!fs::exists(dir))
            ensure_directory(dir);

        return dir / fs::utf8_decode(to_string(fileid) + ".desc");
    }

    pfs::filesystem::path make_targetfilepath (universal_id addresser, std::string const & filename) const
    {
        auto dir = _opts.download_directory / fs::utf8_decode(to_string(addresser));

        if (!fs::exists(dir))
            ensure_directory(dir);

        auto target_filename = fs::utf8_decode(filename);
        auto target_path = dir / target_filename;

        int counter = 1;

        if (fs::exists(target_path)) {
            auto target_filestem = target_filename.stem();
            auto target_fileext  = target_filename.extension();

            while (fs::exists(target_path) && counter != (std::numeric_limits<int>::max)()) {
                auto uniq_part = fmt::format("-({})", counter++);

                target_path = dir / target_filestem;
                target_path += fs::utf8_decode(uniq_part);
                target_path += target_fileext;
            }

            // Last chance
            if (fs::exists(target_path)) {
                auto uniq_part = fmt::format("-({})", pfs::generate_uuid());

                target_path = dir / target_filestem;
                target_path += fs::utf8_decode(uniq_part);
                target_path += target_fileext;
            }

            // It's impossible ;)
            if (fs::exists(target_path)) {
                throw error {
                    errc::filesystem_error
                    , tr::f_("Unable to generate unique file name for: {}", filename)
                };
            }
        }

        return target_path;
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

    /**
     * Locates @c ifile_item by @a fileid.
     *
     * @details If @a ensure is @c true and if @c ifile_item not found, then
     *          the new item emplaced into the pool.
     */
    ifile_item * locate_ifile_item (universal_id addresser, universal_id fileid, bool ensure)
    {
        auto pos = _ifile_pool.find(fileid);

        if (pos == _ifile_pool.end()) {
            if (ensure) {
                auto descfilepath = make_descfilepath(addresser, fileid);
                auto datafilepath = make_datafilepath(addresser, fileid);

                auto desc_file = file::open_write_only(descfilepath);
                auto data_file = file::open_write_only(datafilepath);

                auto res = _ifile_pool.emplace(fileid, ifile_item{
                    addresser
                    , std::move(desc_file)
                    , std::move(data_file)
                    , 0
                    //, pfs::crypto::sha256{}
                });

                return & res.first->second;
            } else {
                return nullptr;
            }
        }

        return & pos->second;
    }

    void remove_ifile_item (universal_id fileid)
    {
        _ifile_pool.erase(fileid);
    }

    void remove_ofile_item (universal_id addressee, universal_id fileid)
    {
        auto range = _ofile_pool.equal_range(fileid);

        for (auto pos = range.first; pos != range.second;) {
            if (pos->second.addressee == addressee) {
                pos = _ofile_pool.erase(pos);
            } else {
                ++pos;
            }
        }
    }

    /**
     * Load file credentials for incoming file
     */
    file_credentials incoming_file_credentials (universal_id addresser, universal_id fileid)
    {
        auto descfilepath = make_descfilepath(addresser, fileid);
        auto desc_file = file::open_read_only(descfilepath);

        file_credentials fc;
        fc.fileid = fileid;

        desc_file.read(fc.offset);
        desc_file.read(fc.filesize);
        fc.filename = desc_file.read_all();
        return fc;
    }

    void cache_file_credentials (universal_id fileid, std::string const & abspath)
    {
        auto cachefilepath = make_cachefilepath(fileid);
        file::rewrite(cachefilepath, abspath);
    }

    void uncache_file_credentials (universal_id fileid)
    {
        auto cachefilepath = make_cachefilepath(fileid);
        fs::remove(cachefilepath);
    }

    void commit_chunk (universal_id addresser, file_chunk const & fc)
    {
        // Returns non-null pointer or throws an exception
        auto ensure = false;
        auto * p = locate_ifile_item(addresser, fc.fileid, ensure);

        // May be file downloading is stopped
        if (!p)
            return;

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
        //p->hash.update(fc.chunk.data(), fc.chunk.size());

        // Write offset
        filesize_t offset = p->data_file.offset();

        p->desc_file.set_pos(0);
        p->desc_file.write(offset);

        if (p->filesize > 0) {
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

    /**
     * Complete or stop file sending
     */
    void complete_file (universal_id addressee, universal_id fileid, bool /*success*/)
    {
        uncache_file_credentials(fileid);
        remove_ofile_item(addressee, fileid);
    }

    /**
     * Notify receiver about file status
     */
    void notify_file_status (universal_id addressee, universal_id fileid, file_status state)
    {
        typename Serializer::ostream_type out;
        out << file_state {fileid, state};

        ready_send(addressee, fileid, packet_type_enum::file_state, out.take());
    }

    /**
     * Commit income file.
     */
    void commit_incoming_file (universal_id addresser, universal_id fileid/*, checksum_type checksum*/)
    {
        auto ensure = false;
        auto * p = locate_ifile_item(addresser, fileid, ensure);

        if (!p) {
            // May be double commit: file is completely downloaded and transient
            // files are removed. Seems it shouldn't be a fatal error (exception).

            on_failure(error {
                errc::filesystem_error
                , tr::f_("`ifile` item not found by file identifier ({}) "
                    "while commiting incoming file, it may be the result of double or"
                    " more commits, need to remove the cause."
                    , fileid)
            });
            return;
        }

        auto descfilepath = make_descfilepath(addresser, fileid);
        auto fc = incoming_file_credentials(addresser, fileid);

        p->desc_file.close();
        p->data_file.close();

        auto donefilepath = make_donefilepath(addresser, fileid);
        auto datafilepath = make_datafilepath(addresser, fileid);
        auto targetfilepath = make_targetfilepath(addresser, fc.filename);

        fs::rename(descfilepath, donefilepath);
        fs::rename(datafilepath, targetfilepath);

        notify_file_status(addresser, fileid, file_status::success);
        download_complete(addresser, fileid, targetfilepath, true);

        remove_ifile_item(fileid);
    }

    /**
     * Save file credentials for incoming file if not exists.
     */
    void cache_incoming_file_credentials (universal_id addresser, file_credentials const & fc)
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

public:
    file_transporter (options const & opts)
    {
        ensure_directory(opts.download_directory);
        _opts.download_directory = opts.download_directory;

        auto bad = false;
        std::string invalid_argument_desc;

        do {
            _opts.remove_transient_files_on_error = opts.remove_transient_files_on_error;

            bad = opts.file_chunk_size < MIN_FILE_CHUNK_SIZE
                || opts.file_chunk_size > MAX_FILE_CHUNK_SIZE;

            if (bad) {
                invalid_argument_desc = tr::f_("file chunk size, must be from {} to {} bytes"
                    , MIN_FILE_CHUNK_SIZE, MAX_FILE_CHUNK_SIZE);
                break;
            }

            _opts.file_chunk_size = opts.file_chunk_size;

            bad = opts.max_file_size < 0 || opts.max_file_size > MAX_FILE_SIZE;

            if (bad) {
                invalid_argument_desc = tr::f_("maximum file size must be"
                    " a positive integer and less or equals to {} bytes", MAX_FILE_SIZE);
                break;
            }

            _opts.max_file_size = opts.max_file_size;

            bad = opts.download_progress_granularity < 0 ||
                opts.download_progress_granularity > 100;

            if (bad) {
                invalid_argument_desc = tr::_("download granularity must be in range "
                    "from 0 to 100");
                break;
            }

            _opts.download_progress_granularity = opts.download_progress_granularity;
        } while (false);

        if (bad) {
            error err {
                make_error_code(std::errc::invalid_argument)
                , invalid_argument_desc
            };

            throw err;
        }
    }

    ~file_transporter ()
    {}

    /**
     * Sets file size upper limit.
     */
    NETTY__EXPORT void set_max_file_size (filesize_t value);

    void process_file_data(universal_id addresser, packet_type_enum packettype, std::vector<char> const & data)
    {
        switch (packettype) {
            case packet_type_enum::file_credentials:
                process_file_credentials(addresser, data);
                break;

            case packet_type_enum::file_request:
                process_file_request(addresser, data);
                break;

            case packet_type_enum::file_stop:
                process_file_stop(addresser, data);
                break;

            case packet_type_enum::file_chunk:
                process_file_chunk(addresser, data);
                break;

            case packet_type_enum::file_begin:
                process_file_begin(addresser, data);
                break;

            case packet_type_enum::file_end:
                process_file_end(addresser, data);
                break;

            case packet_type_enum::file_state:
                process_file_state(addresser, data);
                break;
        }
    }

    // TODO Bellow methos must be private
    void process_file_credentials (universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File credentials received from: {}", addresser);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_credentials fc;
        in >> fc;

        // Cache incoming if file credentials if not exists
        cache_incoming_file_credentials(addresser, fc);
        send_file_request(addresser, fc.fileid);
    }

    void process_file_request (universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File request received from: {}", addresser);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_request fr;
        in >> fr;

        auto cachefilepath = make_cachefilepath(fr.fileid);
        auto orig_path = file::read_all(cachefilepath);

        if (!orig_path.empty()) {
            auto data_file = open_outcome_file(orig_path);

            data_file.set_pos(fr.offset);

            _ofile_pool.emplace(fr.fileid, ofile_item {addresser, std::move(data_file), true});

            // Send file_begin packet
            file_begin fb { fr.fileid, fr.offset };

            typename Serializer::ostream_type out;
            out << fb;

            ready_send(addresser, fr.fileid, packet_type_enum::file_begin, out.take());
        }
    }

    void process_file_stop (universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File stop received from: {}", addresser);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_stop fs;
        in >> fs;

        remove_ofile_item(addresser, fs.fileid);
        upload_stopped(addresser, fs.fileid);
    }

    void process_file_begin (universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File begin received from: {}", addresser);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_begin fb;
        in >> fb;

        // Returns non-null pointer or throws an exception
        auto ensure = false;
        auto * p = locate_ifile_item(addresser, fb.fileid, ensure);

        // May be file downloading is stopped
        if (!p)
            return;

        download_progress(addresser, fb.fileid, fb.offset, p->filesize);
    }

    void process_file_chunk (universal_id addresser, std::vector<char> const & data)
    {
        try {
            typename Serializer::istream_type in {data.data(), data.size()};
            file_chunk fc;
            in >> fc;

            LOG_TRACE_3("File chunk received from: {} ({}; offset={}; chunk size={})"
                , addresser, fc.fileid, fc.offset, fc.chunk.size());

            commit_chunk(addresser, fc);
        } catch (...) {
            typename Serializer::istream_type in {data.data(), data.size()};
            file_chunk_header fch;
            in >> fch;

            auto * p = locate_ifile_item(addresser, fch.fileid, false);

            if (p) {
                on_failure(error {
                      errc::filesystem_error
                    , tr::f_("file chunk from {} may be corrupted, stopping file receive: {}"
                        , addresser, fch.fileid)
                });

                stop_file(addresser, fch.fileid);
            } else if (_ifile_pool.size() == 1) {
                // Only one file is downloading
                auto pos = _ifile_pool.begin();

                on_failure(error {
                      errc::filesystem_error
                    , tr::f_("file chunk from {} may be corrupted, stopping file receive: {}"
                    , addresser, pos->first)
                });

                stop_file(addresser, pos->first);
            } else {
                on_failure(error {
                      errc::filesystem_error
                    , tr::f_("file chunk from {} may be corrupted"
                        ", stopping all files from specified sender", addresser)
                });

                // Unknown file, stop all from specified sender
                for (auto const & x: _ifile_pool) {
                    if (x.second.addresser == addresser) {
                        stop_file(addresser, x.first);
                    }
                }
            }
        }
    }

    void process_file_end (universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File received completely from: {} ({})", addresser, fe.fileid);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_end fe;
        in >> fe;

        commit_incoming_file(addresser, fe.fileid/*, fe.checksum*/);
    }

    void process_file_state(universal_id addresser, std::vector<char> const & data)
    {
        LOG_TRACE_3("File state received  from: {}", addresser);

        typename Serializer::istream_type in {data.data(), data.size()};
        file_state fs;
        in >> fs;

        switch (fs.status) {
            // File received successfully by receiver
            case file_status::success:
                complete_file(addresser, fs.fileid, true);
                break;
    //         case file_status::checksum:
    //             complete_file(fs.fileid, false);
    //             break;

            default:
                // FIXME
    //             this->failure(tr::f_("Unexpected file status ({})"
    //                 " received from: {}:{}, ignored."
    //                 , static_cast<std::underlying_type<file_status>::type>(fs.status)
    //                 , to_string(psock->addr())
    //                 , psock->port()));
                break;
        }
    }

    /**
     * Remove output pool item associated with @a addressee.
     */
    void expire_addressee (universal_id addressee)
    {
        // Erase all items associated with specified addressee
        for (auto pos = _ofile_pool.begin(); pos != _ofile_pool.end();) {
            if (pos->second.addressee == addressee)
                pos = _ofile_pool.erase(pos);
            else
                ++pos;
        }
    }

    /**
     * Remove input pool item associated with @a addresser.
     * Notify when active downloads are interrupted.
     */
    void expire_addresser (universal_id addresser)
    {
        // Erase all items associated with specified addressee
        for (auto pos = _ifile_pool.begin(); pos != _ifile_pool.end();) {
            if (pos->second.addresser == addresser) {
                download_interrupted(addresser, pos->first);
                pos = _ifile_pool.erase(pos);
            } else {
                ++pos;
            }
        }
    }

    /**
     * Send file.
     *
     * @details Actually this method initiates file sending by send file
     *          credentials.
     *
     * @param addressee Addressee unique identifier.
     * @param fileid Unique file identifier. If @a file_id is invalid it
     *        will be generated automatically.
     * @param path Path to sending file.
     *
     * @return Unique file identifier or invalid identifier on error (file too
     *        big to send or has no permissions to read file).
     */
    universal_id send_file (universal_id addressee, universal_id fileid, pfs::filesystem::path const & path)
    {
        // File too big to send
        //if (filesize > _opts.max_file_size) {
        if (fs::file_size(path) > _opts.max_file_size) {
            // log_error(tr::f_("Unable to send file: {}, file too big."
            //      " Max size is {} bytes", path, _opts.max_file_size));
            return universal_id{};
        }

        try {
            // Check if file can be read
            file::open_read_only(path);
        } catch (...) {
            //log_error(tr::f_("Unable to send file: {}: file not found or"
            //    " has no permissions", path));
            return universal_id{};
        }

        if (fileid == universal_id{})
            fileid = pfs::generate_uuid();

        file_credentials fc {
            fileid
            , fs::utf8_encode(path.filename())
            //, static_cast<filesize_type>(filesize)
            , static_cast<filesize_t>(fs::file_size(path))
            , 0
        };

        LOG_TRACE_3("Send file credentials: {} (file id={}; size={} bytes)"
            , path, fileid, fc.filesize);

        cache_file_credentials(fileid, fs::utf8_encode(fs::absolute(path)));

        typename Serializer::ostream_type out;
        out << fc;

        ready_send(addressee, fileid, packet_type_enum::file_credentials, out.take());

        return fileid;
    }

    universal_id send_file (universal_id addressee, universal_id fileid
        , std::string const & path, std::string const & display_name, std::int64_t filesize)
    {
        if (fileid == universal_id{})
            fileid = pfs::generate_uuid();

        file_credentials fc {
            fileid
            , display_name
            , static_cast<filesize_t>(filesize)
            , 0
        };

        LOG_TRACE_3("Send file credentials: {} (file id={}; size={} bytes)"
            , path, fileid, fc.filesize);

        cache_file_credentials(fileid, path);

        typename Serializer::ostream_type out;
        out << fc;

        ready_send(addressee, fileid, packet_type_enum::file_credentials, out.take());

        return fileid;
    }

    /**
     * Send request to download file from @a addressee_id.
     */
    void send_file_request (universal_id addressee, universal_id fileid)
    {
        auto fc = incoming_file_credentials(addressee, fileid);
        auto datafilepath = make_datafilepath(addressee, fileid);
        auto filesize = fs::file_size(datafilepath);

        // Original file size is less than offset stored in description file
        if (fc.offset > filesize)
            fc.offset = filesize;

        auto * p = locate_ifile_item(addressee, fileid, true);

        if (p)
            p->filesize = fc.filesize;

        file_request fr { fileid, fc.offset };
        typename Serializer::ostream_type out;
        out << fr;

        ready_send(addressee, fileid, packet_type_enum::file_request, out.take());

        LOG_TRACE_3("Send file request: addressee={}; file={}; offset={}"
            , addressee, fr.fileid, fr.offset);
    }

    /**
     * Stop file downloading and send command to @a addressee to stop
     * downloading file from it.
     */
    void stop_file (universal_id addressee, universal_id fileid)
    {
        // Do not process incoming file chunks
        remove_ifile_item(fileid);

        file_stop fs { fileid };
        typename Serializer::ostream_type out;
        out << fs;

        LOG_TRACE_3("Send file stop to: {} ({})", addressee, fs.fileid);

        ready_send(addressee, fileid, packet_type_enum::file_stop, out.take());
    }

    /**
     * Requests new chunk for specified file identifier @a fileid. Must be
     * called from caller if need the next chunk to send.
     *
     * @return @c true if there are more chunks or @c false otherwise.
     */
    bool request_chunk (universal_id addressee, universal_id fileid)
    {
        auto range = _ofile_pool.equal_range(fileid);

        for (auto pos = range.first; pos != range.second; ++pos) {
            if (pos->second.addressee == addressee) {
                pos->second.chunk_requested = true;
                return true;
            }
        }

        return false;
    }

    /**
     * Sends a dozen file chunks (fragments)
     *
     * @return non-zero value if there was a sending of chunks of files
     */
    int step ()
    {
        if (_ofile_pool.empty())
            return 0;

        int counter = 0;

        for (auto pos = _ofile_pool.begin(); pos != _ofile_pool.end();) {
            if (!addressee_ready(pos->second.addressee)) {
                ++pos;
                continue;
            }

            auto * p = & pos->second;

            // Chunks sending stopped temporary
            if (!p->chunk_requested) {
                ++pos;
                continue;
            }

            // Temporary stop sending chunks
            p->chunk_requested = false;

            counter++;

            auto fileid = pos->first;
            auto offset = p->data_file.offset();

            file_chunk fc;
            fc.chunk = read_chunk(p->data_file, _opts.file_chunk_size);

            // End of file
            if (fc.chunk.empty()) {
                // File send completely, send `file_end` packet

                file_end fe { fileid/*, p->hash.digest()*/ };

                typename Serializer::ostream_type out;
                out << fe;

                ready_send(pos->second.addressee, fileid, packet_type_enum::file_end, out.take());

                // Remove file from output pool
                pos = _ofile_pool.erase(pos);
            } else if (fc.chunk.size() > 0) {
                fc.fileid = pos->first;
                fc.offset = offset;
                fc.chunksize = static_cast<filesize_t>(fc.chunk.size());

                typename Serializer::ostream_type out;
                out << fc;

                LOG_TRACE_3("Send file chunk: {} (offset={}, chunk size={})"
                    , pos->first, offset, fc.chunk.size());

                ready_send(pos->second.addressee, fileid, packet_type_enum::file_chunk, out.take());

                //p->hash.update(fc.chunk.data(), fc.chunk.size());
                ++pos;
            }
        }

        return counter;
    }

    /**
     * @deprecated Use step instead.
     */
    inline int loop ()
    {
        return step();
    }

    /**
     * Wipes download folder.
     */
    void wipe (error * perr = nullptr)
    {
        wipe(_opts.download_directory, perr);
    }

public: // static
    /**
     * Wipes specified folder.
     */
    static void wipe (fs::path const & download_dir, error * perr = nullptr)
    {
        fs::directory_traversal dt;

        dt.on_leave_directory = [download_dir, perr] (fs::path const & dir) {
            if (dir != download_dir) {
                std::error_code ec;
                fs::remove_all(dir, ec);

                if (ec) {
                    error err = {
                        errc::filesystem_error
                        , tr::f_("remove directory failure: {}", dir)
                        , ec.message()
                    };

                    if (perr) {
                        *perr = std::move(err);
                    } else {
                        throw err;
                    }
                } else {
                    LOG_TRACE_1("Directory removed: {}", dir);
                }
            }
        };

        dt.on_entry = [perr] (pfs::filesystem::path const & path) {
            std::error_code ec;
            fs::remove(path, ec);

            if (ec) {
                error err = {
                    errc::filesystem_error
                    , tr::f_("remove file failure: {}", path)
                    , ec.message()
                };

                if (perr) {
                    *perr = std::move(err);
                } else {
                    throw err;
                }
            } else {
                LOG_TRACE_2("File removed: {}", path);
            }
        };

        dt.on_entry_error = [perr] (fs::path const & path, pfs::error const & ex) {
            error err {
                errc::filesystem_error
                , tr::f_("wipe failure on file: {}", path)
                , ex.what()
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }
        };

        dt.on_error = [perr] (pfs::error const & ex) {
            error err {
                errc::filesystem_error
                , tr::_("wipe failure")
                , ex.what()
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }
        };

        if (!download_dir.empty()) {
            dt.traverse(download_dir);
        }
    }
};

template <typename Serializer> constexpr filesize_t file_transporter<Serializer>::DEFAULT_FILE_CHUNK_SIZE;
template <typename Serializer> constexpr filesize_t file_transporter<Serializer>::MIN_FILE_CHUNK_SIZE;
template <typename Serializer> constexpr filesize_t file_transporter<Serializer>::MAX_FILE_CHUNK_SIZE;
template <typename Serializer> constexpr filesize_t file_transporter<Serializer>::MAX_FILE_SIZE;

}} // namespace netty::p2p
