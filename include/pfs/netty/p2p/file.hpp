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
#include "pfs/i18n.hpp"
#include "pfs/universal_id.hpp"
#include "pfs/netty/error.hpp"

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

using filesize_t = std::int32_t;

enum class truncate_enum: std::int8_t { off, on };

class file
{
    static constexpr filesize_t const INVALID_FILE_HANDLE = -1;
    using handle_type = int;

private:
    handle_type _h {INVALID_FILE_HANDLE};

private:
    file (handle_type h): _h(h) {}

public:
    file () {}

    file (file const & f) = delete;
    file & operator = (file const & f) = delete;

    file (file && f)
    {
        if (this != & f) {
            _h = f._h;
            f._h = INVALID_FILE_HANDLE;
        }
    }

    file & operator = (file && f)
    {
        if (this != & f) {
            close();
            _h = f._h;
            f._h = INVALID_FILE_HANDLE;
        }

        return *this;
    }

    ~file ()
    {
        close();
    }

    operator bool () const noexcept
    {
        return _h >= 0;
    }

    inline void close () noexcept
    {
        if (_h > 0)
            ::close(_h);

        _h = INVALID_FILE_HANDLE;
    }

    filesize_t offset () const
    {
        return static_cast<filesize_t>(::lseek(_h, 0, SEEK_CUR));
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
    filesize_t read (char * buffer, filesize_t count, std::error_code & ec) const noexcept
    {
        if (count == 0)
            return 0;

        if (count < 0) {
            ec = make_error_code(std::errc::invalid_argument);
            return -1;
        }

        auto n = ::read(_h, buffer, count);

        if (n < 0) {
            ec = std::error_code(errno, std::generic_category());
            return -1;
        }

        return n;
    }

    filesize_t read (char * buffer, filesize_t count) const
    {
        std::error_code ec;
        auto n = read(buffer, count, ec);

        if (ec)
            throw pfs::error{ec, tr::_("read buffer from file")};

        return n;
    }

    template <typename T>
    inline filesize_t read (T & value, std::error_code & ec) const noexcept
    {
        return read(reinterpret_cast<char *>(& value), sizeof(T), ec);
    }

    template <typename T>
    inline filesize_t read (T & value) const noexcept
    {
        return read(reinterpret_cast<char *>(& value), sizeof(T));
    }

    /**
     * Read all content from file started from specified @a offset.
     */
    std::string read_all (std::error_code & ec) const noexcept
    {
        std::string result;
        char buffer[512];

        auto n = read(buffer, 512, ec);

        while (n > 0) {
            result.append(buffer, n);
            n = read(buffer, 512, ec);
        }

        return ec ? std::string{} : result;
    }

    std::string read_all () const
    {
        std::error_code ec;
        auto result = read_all(ec);

        if (ec)
            throw pfs::error{ec, tr::_("read all from file")};

        return result;
    }

    /**
     * @brief Write buffer to file.
     */
    filesize_t write (char const * buffer, filesize_t count, std::error_code & ec)
    {
        auto n = ::write(_h, buffer, count);

        if (n < 0)
            ec = std::error_code(errno, std::generic_category());

        return n;
    }

    /**
     * @brief Write buffer to file.
     */
    template <typename T>
    inline filesize_t write (T const & value, std::error_code & ec)
    {
        return write(reinterpret_cast<char const *>(& value), sizeof(T), ec);
    }

    filesize_t write (char const * buffer, filesize_t count)
    {
        std::error_code ec;
        auto n = write(buffer, count, ec);

        if (ec)
            throw pfs::error{ec, tr::_("write file")};

        return n;
    }

    template <typename T>
    inline filesize_t write (T const & value)
    {
        return write(reinterpret_cast<char const *>(& value), sizeof(T));
    }

    /**
     * Set file position by @a offset.
     */
    bool set_pos (filesize_t offset, std::error_code & ec)
    {
        auto pos = static_cast<filesize_t>(::lseek(_h, offset, SEEK_SET));

        if (pos < 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }

        return true;
    }

    /**
     * Set file position by @a offset.
     *
     * @throw pfs::error on set file position failure.
     */
    void set_pos (filesize_t offset)
    {
        std::error_code ec;

        if (!set_pos(offset, ec))
            throw pfs::error{ec, tr::_("set file position")};
    }

public: // static
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
    static file open_read_only (fs::path const & path, std::error_code & ec)
    {
        if (!fs::exists(path)) {
            ec = make_error_code(std::errc::no_such_file_or_directory);
            return file{};
        }

        handle_type h = ::open(fs::utf8_encode(path).c_str(), O_RDONLY);

        if (h < 0) {
            ec = std::error_code(errno, std::generic_category());
            return file{};
        }

        return file{h};
    }

    static file open_read_only (fs::path const & path)
    {
        std::error_code ec;
        auto f = open_read_only(path, ec);

        if (ec)
            throw pfs::error{ec, tr::f_("open read only file: {}", path)};

        return f;
    }

    /**
    * @brief Open file for writing.
    *
    * @return Output file handle or @c INVALID_FILE_HANDLE on error. In last case
    *         @a ec set to appropriate error code:
    *         - POSIX-specific error codes returned by @c ::open and
    *           @c ::lseek calls.
    */
    static file open_write_only (fs::path const & path, truncate_enum trunc
        , std::error_code & ec)
    {
        int oflags = O_WRONLY | O_CREAT;

        if (trunc == truncate_enum::on)
            oflags |= O_TRUNC;

        handle_type h = ::open(fs::utf8_encode(path).c_str()
            , oflags, S_IRUSR | S_IWUSR);

        if (h < 0) {
            ec = std::error_code(errno, std::generic_category());
            return file{};
        }

        return file{h};
    }

    static file open_write_only (fs::path const & path, truncate_enum trunc)
    {
        std::error_code ec;
        auto f = open_write_only(path, trunc, ec);

        if (ec)
            throw pfs::error{ec, tr::f_("open write only file: {}", path)};

        return f;
    }

    static file open_write_only (fs::path const & path, std::error_code & ec)
    {
        return open_write_only(path, truncate_enum::off, ec);
    }

    static file open_write_only (fs::path const & path)
    {
        return open_write_only(path, truncate_enum::off);
    }

    /**
     * Rewrite file with content from @a text.
     */
    static bool rewrite (fs::path const & path, char const * buffer
        , filesize_t count, std::error_code & ec)
    {
        file f = open_write_only(path, truncate_enum::on, ec);

        if (f) {
            auto n = f.write(buffer, count, ec);
            return n >= 0;
        }

        return false;
    }

    static void rewrite (fs::path const & path, char const * buffer
        , filesize_t count)
    {
        std::error_code ec;
        rewrite(path, buffer, count, ec);

        if (ec)
            throw pfs::error{ec, tr::f_("rewrite file: {}", path)};
    }

    static void rewrite (fs::path const & path, std::string const & text)
    {
        rewrite(path, text.c_str(), text.size());
    }

    static std::string read_all (fs::path const & path, std::error_code & ec)
    {
        auto f = file::open_read_only(path, ec);

        if (ec)
            return f.read_all(ec);

        return std::string{};
    }

    static std::string read_all (fs::path const & path)
    {
        auto f = file::open_read_only(path);
        return f.read_all();
    }
};

using file_t  = file;
using ifile_t = file;
using ofile_t = file;

}} // namespace netty::p2p
