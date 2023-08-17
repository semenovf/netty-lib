////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/file_transporter.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include <cmath>
#include <string>

namespace netty {
namespace p2p {

namespace fs = pfs::filesystem;

constexpr filesize_t const file_transporter::DEFAULT_FILE_CHUNK_SIZE;
constexpr filesize_t const file_transporter::MIN_FILE_CHUNK_SIZE;
constexpr filesize_t const file_transporter::MAX_FILE_CHUNK_SIZE;
constexpr filesize_t const file_transporter::MAX_FILE_SIZE;

file_transporter::file_transporter (options const & opts)
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
              errc::invalid_argument
            , invalid_argument_desc
        };

        throw err;
    }
}

file_transporter::~file_transporter ()
{}

bool file_transporter::ensure_directory (fs::path const & dir, error * perr) const
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

std::vector<char> file_transporter::read_chunk (file const & data_file
    , filesize_t count)
{
    std::vector<char> chunk(count);
    auto n = data_file.read(chunk.data(), count);

    if (n == 0)
        chunk.clear();
    else if (n < count)
        chunk.resize(n);

    return chunk;
}

fs::path file_transporter::make_transientfilepath (universal_id addresser
    , universal_id fileid
    , std::string const & ext) const
{
    auto dir = _opts.download_directory
        / fs::utf8_decode(to_string(addresser))
        / PFS__LITERAL_PATH("transient");

    if (!fs::exists(dir))
        ensure_directory(dir);

    return dir / fs::utf8_decode(to_string(fileid) + ext);
}

fs::path file_transporter::make_descfilepath (universal_id addresser
    , universal_id fileid) const
{
    return make_transientfilepath(addresser, fileid, ".desc");
}

fs::path file_transporter::make_datafilepath (universal_id addresser, universal_id fileid) const
{
    return make_transientfilepath(addresser, fileid, ".data");
}

fs::path file_transporter::make_donefilepath (universal_id addresser, universal_id fileid) const
{
    return make_transientfilepath(addresser, fileid, ".done");
}

fs::path file_transporter::make_errfilepath (universal_id addresser, universal_id fileid) const
{
    return make_transientfilepath(addresser, fileid, ".err");
}

fs::path file_transporter::make_cachefilepath (universal_id fileid) const
{
    auto dir = _opts.download_directory / PFS__LITERAL_PATH(".cache");

    if (!fs::exists(dir))
        ensure_directory(dir);

    return dir / fs::utf8_decode(to_string(fileid) + ".desc");
}

fs::path file_transporter::make_targetfilepath (universal_id addresser
    , std::string const & filename) const
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

void file_transporter::remove_transient_files (universal_id addresser, universal_id fileid)
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

file_transporter::ifile_item * file_transporter::locate_ifile_item (
    universal_id addresser, universal_id fileid, bool ensure)
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

void file_transporter::remove_ifile_item (universal_id fileid)
{
    _ifile_pool.erase(fileid);
}

void file_transporter::remove_ofile_item (universal_id addressee, universal_id fileid)
{
    auto range = _ofile_pool.equal_range(fileid);

    for (auto pos = range.first; pos != range.second; ++pos) {
        if (pos->second.addressee == addressee) {
            _ofile_pool.erase(pos);
        }
    }
}

void file_transporter::notify_file_status (universal_id addressee
    ,  universal_id fileid, file_status state)
{
    output_envelope_type out;
    out << file_state {fileid, state};

    ready_to_send(addressee, fileid, packet_type::file_state
        , out.data().data(), out.data().size());
}

void file_transporter::commit_incoming_file (universal_id addresser
    , universal_id fileid/*, checksum_type checksum*/)
{
    auto ensure = false;
    auto * p = locate_ifile_item(addresser, fileid, ensure);

    if (!p) {
        // May be double commit: file is completely downloaded and transient
        // files are removed. Seems it shouldn't be a fatal error (exception).

        // throw error {
        //       errc::filesystem_error
        //     , tr::f_("ifile item not found by fileid: {}", fileid)
        // };

        on_error(tr::f_("`ifile` item not found by file identifier ({}) "
            "while commiting incoming file, it may be the result of double or"
            " more commits, need to remove the cause."
            , fileid));
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

void file_transporter::cache_incoming_file_credentials (universal_id addresser
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

file_credentials file_transporter::incoming_file_credentials (
      universal_id addresser
    , universal_id fileid)
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

void file_transporter::cache_file_credentials (universal_id fileid
    , std::string const & abspath)
{
    auto cachefilepath = make_cachefilepath(fileid);
    file::rewrite(cachefilepath, abspath);
}

void file_transporter::uncache_file_credentials (universal_id fileid)
{
    auto cachefilepath = make_cachefilepath(fileid);
    fs::remove(cachefilepath);
}

void file_transporter::commit_chunk (universal_id addresser, file_chunk const & fc)
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

// Complete or stop file sending
void file_transporter::complete_file (universal_id addressee, universal_id fileid, bool /*success*/)
{
    uncache_file_credentials(fileid);
    remove_ofile_item(addressee, fileid);
}

void file_transporter::process_file_credentials (universal_id sender
    , std::vector<char> const & data)
{
    auto fc = input_envelope_type::unseal<file_credentials>(data);

    // Cache incoming if file credentials if not exists
    cache_incoming_file_credentials(sender, fc);
    send_file_request(sender, fc.fileid);
}

void file_transporter::process_file_request (universal_id sender
    , std::vector<char> const & data)
{
    auto fr = input_envelope_type::unseal<file_request>(data);

    auto cachefilepath = make_cachefilepath(fr.fileid);
    auto orig_path = file::read_all(cachefilepath);

    if (!orig_path.empty()) {
        auto data_file = open_outcome_file(orig_path);

        data_file.set_pos(fr.offset);

        _ofile_pool.emplace(fr.fileid, ofile_item{
            sender, std::move(data_file), true});

        // Send file_begin packet
        file_begin fb { fr.fileid, fr.offset };

        output_envelope_type out;
        out << fb;

        ready_to_send(sender, fr.fileid, packet_type::file_begin
            , out.data().data(), out.data().size());
    }
}

void file_transporter::process_file_stop (universal_id sender
    , std::vector<char> const & data)
{
    auto fs = input_envelope_type::unseal<file_stop>(data);
    remove_ofile_item(sender, fs.fileid);
    upload_stopped(sender, fs.fileid);
}

void file_transporter::process_file_begin (universal_id addresser
    , std::vector<char> const & data)
{
    auto fb = input_envelope_type::unseal<file_begin>(data);

    // Returns non-null pointer or throws an exception
    auto ensure = false;
    auto * p = locate_ifile_item(addresser, fb.fileid, ensure);

    // May be file downloading is stopped
    if (!p)
        return;

    download_progress(addresser, fb.fileid, fb.offset, p->filesize);
}

void file_transporter::process_file_chunk (universal_id sender
    , std::vector<char> const & data)
{
    try {
        auto fc = input_envelope_type::unseal<file_chunk>(data);

        //LOG_TRACE_3("File chunk received from: {} ({}; offset={}; chunk size={})"
        //    , sender, fc.fileid, fc.offset, fc.chunk.size());

        commit_chunk(sender, fc);
    } catch (...) { // may be cereal::Exception
        auto fch = input_envelope_type::unseal<file_chunk_header>(data);

        auto * p = locate_ifile_item(sender, fch.fileid, false);

        if (p) {
            on_error(tr::f_("file chunk from {} may be corrupted, stopping file receive: {}"
                , sender, fch.fileid));
            stop_file(sender, fch.fileid);
        } else if (_ifile_pool.size() == 1) {
            // Only one file is downloading
            auto pos = _ifile_pool.begin();

            on_error(tr::f_("file chunk from {} may be corrupted, stopping file receive: {}"
                , sender, pos->first));

            stop_file(sender, pos->first);
        } else {
            on_error(tr::f_("file chunk from {} may be corrupted"
                ", stopping all files from specified sender", sender));

            // Unknown file, stop all from specified sender
            for (auto const & x: _ifile_pool) {
                if (x.second.addresser == sender) {
                    stop_file(sender, x.first);
                }
            }
        }
    }
}

void file_transporter::process_file_end (universal_id sender
    , std::vector<char> const & data)
{
    auto fe = input_envelope_type::unseal<file_end>(data);

    LOG_TRACE_3("File received completely from: {} ({})", sender, fe.fileid);
    commit_incoming_file(sender, fe.fileid/*, fe.checksum*/);
}

void file_transporter::process_file_state (universal_id sender
    , std::vector<char> const & data)
{
    auto fs = input_envelope_type::unseal<file_state>(data);

    switch (fs.status) {
        // File received successfully by receiver
        case file_status::success:
            complete_file(sender, fs.fileid, true);
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

void file_transporter::expire_addressee (universal_id addressee)
{
    // Erase all items associated with specified addressee
    for (auto pos = _ofile_pool.begin(); pos != _ofile_pool.end();) {
        if (pos->second.addressee == addressee)
            pos = _ofile_pool.erase(pos);
        else
            ++pos;
    }
}

void file_transporter::expire_addresser (universal_id addresser)
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

void file_transporter::send_file_request (universal_id addressee_id, universal_id fileid)
{
    auto fc = incoming_file_credentials(addressee_id, fileid);
    auto datafilepath = make_datafilepath(addressee_id, fileid);
    auto filesize = fs::file_size(datafilepath);

    // Original file size is less than offset stored in description file
    if (fc.offset > filesize)
        fc.offset = filesize;

    auto * p = locate_ifile_item(addressee_id, fileid, true);

    if (p)
        p->filesize = fc.filesize;

    file_request fr { fileid, fc.offset };
    output_envelope_type out;
    out << fr;

    ready_to_send(addressee_id, fileid, packet_type::file_request
        , out.data().data(), out.data().size());

    LOG_TRACE_3("Send file request: addressee={}; file={}; offset={}"
        , addressee_id, fr.fileid, fr.offset);
}

universal_id file_transporter::send_file (universal_id addressee
    , universal_id fileid, pfs::filesystem::path const & path)
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

    output_envelope_type out;
    out << fc;

    ready_to_send(addressee, fileid, packet_type::file_credentials
        , out.data().data(), out.data().size());

    return fileid;
}

universal_id file_transporter::send_file (universal_id addressee
    , universal_id fileid, std::string const & path
    , std::string const & display_name
    , std::int64_t filesize)
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

    output_envelope_type out;
    out << fc;

    ready_to_send(addressee, fileid, packet_type::file_credentials
        , out.data().data(), out.data().size());

    return fileid;
}

void file_transporter::stop_file (universal_id addressee, universal_id fileid)
{
    // Do not process incoming file chunks
    remove_ifile_item(fileid);

    file_stop fs { fileid };
    output_envelope_type out;
    out << fs;

    LOG_TRACE_3("Send file stop to: {} ({})", addressee, fs.fileid);

    ready_to_send(addressee, fileid, packet_type::file_stop
        , out.data().data(), out.data().size());
}

bool file_transporter::request_chunk (universal_id addressee, universal_id fileid)
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

// Send chunk of file packets
int file_transporter::loop ()
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

            output_envelope_type out;
            out << fe;

            ready_to_send(pos->second.addressee, fileid, packet_type::file_end
                , out.data().data(), out.data().size());

            // Remove file from output pool
            pos = _ofile_pool.erase(pos);
        } else if (fc.chunk.size() > 0) {
            fc.fileid = pos->first;
            fc.offset = offset;
            fc.chunksize = static_cast<filesize_t>(fc.chunk.size());

            output_envelope_type out;
            out << fc;

            LOG_TRACE_3("Send file chunk: {} (offset={}, chunk size={})"
                , pos->first, offset, fc.chunk.size());

            ready_to_send(pos->second.addressee, fileid, packet_type::file_chunk
                , out.data().data(), out.data().size());

            //p->hash.update(fc.chunk.data(), fc.chunk.size());
            ++pos;
        }
    }

    return counter;
}

void file_transporter::wipe (fs::path const & download_dir, error * perr)
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
}} // namespace netty::p2p
