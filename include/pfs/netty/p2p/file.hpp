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
#include "pfs/filesystem.hpp"

#if _MSC_VER
#   include "windows.hpp"
#else
#   include <sys/types.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif

namespace netty {
namespace p2p {

namespace fs = pfs::filesystem;

using filesize_t     = std::int32_t;
using file_handle_t  = int;
using ifile_handle_t = file_handle_t;
using ofile_handle_t = file_handle_t;

namespace {
constexpr filesize_t CURRENT_OFFSET = -1;
constexpr filesize_t INVALID_FILE_HANDLE = -1;
}

enum class truncate_enum: std::int8_t { off, on };

// inline bool eof (file_handle_t fh)
// {
//     return ::lseek(fh, 0, SEEK_CUR) == ::lseek(fh, 0, SEEK_END);
// }

inline filesize_t file_offset (ifile_handle_t fh)
{
    return static_cast<filesize_t>(::lseek(fh, 0, SEEK_CUR));
}

inline void close_file (file_handle_t fh)
{
    if (fh > 0)
        ::close(fh);
}

/**
 * @brief Open file for reading.
 *
 * @return Input file handle or @c INVALID_FILE_HANDLE on error. In last case
 *         @a ec set to appropriate error code:
 *         - @c std::errc::no_such_file_or_directory if file not found;
 *         - @c std::errc::invalid_argument if @a offset has unsuitable value;
 *         - other POSIX-specific error codes returned by @c ::open and
 *           @c ::lseek calls.
 */
inline ifile_handle_t open_ifile (fs::path const & path, filesize_t offset
    , std::error_code & ec)
{
    if (!fs::exists(path)) {
        ec = make_error_code(std::errc::no_such_file_or_directory);
        return INVALID_FILE_HANDLE;
    }

    auto filesize = fs::file_size(path);

    // File offset greater than it's size
    if (offset >= filesize) {
        ec = make_error_code(std::errc::invalid_argument);
        return INVALID_FILE_HANDLE;
    }

    ifile_handle_t ifh = ::open(fs::utf8_encode(path).c_str(), O_RDONLY);

    if (ifh < 0) {
        ec = std::error_code(errno, std::generic_category());
        return INVALID_FILE_HANDLE;
    }

    if (offset >= 0) {
        filesize_t off = ::lseek(ifh, offset, SEEK_SET);

        if (off < 0) {
            ec = std::error_code(errno, std::generic_category());
            close_file(ifh);
            return INVALID_FILE_HANDLE;
        }
    }

    return ifh;
}

/**
 * @brief Open file for writing.
 *
 * @return Output file handle or @c INVALID_FILE_HANDLE on error. In last case
 *         @a ec set to appropriate error code:
 *         - POSIX-specific error codes returned by @c ::open and
 *           @c ::lseek calls.
 */
inline ofile_handle_t open_ofile (fs::path const & path, filesize_t offset
    , truncate_enum trunc, std::error_code & ec)
{
    int oflags = O_WRONLY | O_CREAT;

    if (trunc == truncate_enum::on)
        oflags |= O_TRUNC;

    ofile_handle_t ofh = ::open(fs::utf8_encode(path).c_str()
        , oflags, S_IRUSR | S_IWUSR);

    if (ofh < 0) {
        ec = std::error_code(errno, std::generic_category());
        return INVALID_FILE_HANDLE;
    }

    if (offset >= 0) {
        filesize_t off = ::lseek(ofh, offset, SEEK_SET);

        if (off < 0) {
            ec = std::error_code(errno, std::generic_category());
            close_file(ofh);
            return INVALID_FILE_HANDLE;
        }
    }

    return ofh;
}

/**
 * Read data chunk from file.
 *
 * @param ifh File descriptor.
 * @param buffer Pointer to input buffer.
 * @param size Input buffer size.
 * @param offset Offset from start to read (-1 for start read from current position).
 * @param ec Error code.
 *
 * @return Actually read chunk size or -1 on error.
 *
 * @note Limit maximum file size to 2,147,479,552 bytes. For transfer bigger
 *       files use another way/tools (scp, for example).
 */
inline filesize_t read_file (ifile_handle_t ifh, char * buffer, filesize_t count
    , filesize_t offset, std::error_code & ec)
{
    if (count == 0)
        return 0;

    if (count < 0) {
        ec = make_error_code(std::errc::invalid_argument);
        return -1;
    }

    if (ifh < 0) {
        ec = make_error_code(std::errc::invalid_argument);
        return -1;
    }

    if (offset < 0) {
        // Start from current position
    } else if (offset >= 0) {
        filesize_t curr_off = ::lseek(ifh, 0, SEEK_CUR);
        filesize_t off = ::lseek(ifh, offset, SEEK_SET);

        if (off < 0) {
            ec = std::error_code(errno, std::generic_category());
            return -1;
        }

        if (off != offset) {
            ec = make_error_code(std::errc::invalid_argument);
            ::lseek(ifh, curr_off, SEEK_SET);
            return -1;
        }
    }

    auto n = ::read(ifh, buffer, count);

    if (n < 0) {
        ec = std::error_code(errno, std::generic_category());
        return -1;
    }

    return n;
}

/**
 * Convenient function for read data chunk from file.
 *
 * @return Bytes read from file stored in vector of chars.
 */
inline std::vector<char> read_file (ifile_handle_t ifh, filesize_t count
    , filesize_t offset, std::error_code & ec)
{
    if (count == 0)
        return std::vector<char>{};

    if (count < 0) {
        ec = make_error_code(std::errc::invalid_argument);
        return std::vector<char>{};
    }

    std::vector<char> result(static_cast<std::size_t>(count));

    auto n = read_file(ifh, result.data(), result.size(), offset, ec);

    if (n >= 0)
        result.resize(static_cast<std::size_t>(n));
    else
        result.clear();

    return result;
}

/**
 * Read all content from file started from specified @a offset.
 */
inline std::string read_file (ifile_handle_t ifh, filesize_t offset
    , std::error_code & ec)
{
    std::string result;
    char buffer[512];

    auto n = read_file(ifh, buffer, 512, offset, ec);

    while (n > 0) {
        result.append(buffer, n);
        n = read_file(ifh, buffer, 512, -1, ec);
    }

    close_file(ifh);

    return ec ? std::string{} : result;
}

/**
 * Read all content from file started from specified @a offset.
 */
inline std::string read_file (fs::path const & path, filesize_t offset
    , std::error_code & ec)
{
    auto ifh = open_ifile(path, offset, ec);

    if (ifh < 0)
        return std::string{};

    auto result = read_file(ifh, offset, ec);

    return ec ? std::string{} : result;
}

/**
 * Rewrite file with content from @a text.
 */
inline void rewrite_file (fs::path const & path, std::string const & text
    , std::error_code & ec)
{
    auto ofh = open_ofile(path, 0, truncate_enum::on, ec);

    if (ofh > 0) {
        auto n = ::write(ofh, text.c_str(), text.size());

        if (n < 0)
            ec = std::error_code(errno, std::generic_category());

        close_file(ofh);
    }
}

/**
 * @brief Write chunk of data to file.
 */
inline void write_chunk (ofile_handle_t ofh, filesize_t offset
    , char const * data, filesize_t count, std::error_code & ec)
{
    if (offset >= 0) {
        filesize_t off = ::lseek(ofh, offset, SEEK_SET);

        if (off < 0) {
            ec = std::error_code(errno, std::generic_category());
            close_file(ofh);
            return;
        }
    }

    auto n = ::write(ofh, data, count);

    if (n < 0)
        ec = std::error_code(errno, std::generic_category());
}

}} // namespace netty::p2p
