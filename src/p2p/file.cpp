////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/p2p/file.hpp"

#if _MSC_VER
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <fcntl.h>

#   ifndef S_IRUSR
#       define S_IRUSR _S_IREAD
#   endif

#   ifndef S_IWUSR
#       define S_IWUSR _S_IWRITE
#   endif
#else // _MSC_VER
#   include <sys/types.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif // !_MSC_VER

namespace netty {
namespace p2p {

namespace details {

file::handle_type open_read_only (filepath_t const & path, error * perr)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__
    if (!fs::exists(path)) {
        error err {
              make_error_code(std::errc::no_such_file_or_directory)
            , fs::utf8_encode(path)
        };

        if (*perr) {
            *perr = err;
            return file::INVALID_FILE_HANDLE;
        } else {
            throw err;
        }
    }

#   if _MSC_VER
    file::handle_type h;
    _sopen_s(& h, fs::utf8_encode(path).c_str(), O_RDONLY, _SH_DENYNO, 0);
#   else
    file::handle_type h = ::open(fs::utf8_encode(path).c_str(), O_RDONLY);
#   endif

    if (h < 0) {
        error err {
              std::error_code(errno, std::generic_category())
            , tr::f_("open read only file: {}", path)
        };

        if (perr) {
            *perr = err;
            return file::INVALID_FILE_HANDLE;
        } else {
            throw err;
        }
    }

    return h;
#endif // !__ANDROID__
}

file::handle_type open_write_only (fs::path const & path, truncate_enum trunc, error * perr)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

    int oflags = O_WRONLY | O_CREAT;

    if (trunc == truncate_enum::on)
        oflags |= O_TRUNC;

#   if _MSC_VER
    file::handle_type h;
    _sopen_s(& h, fs::utf8_encode(path).c_str(), oflags, _SH_DENYWR, S_IRUSR | S_IWUSR);
#   else
    file::handle_type h = ::open(fs::utf8_encode(path).c_str()
        , oflags, S_IRUSR | S_IWUSR);
#   endif

    if (h < 0) {
        error err {
              std::error_code(errno, std::generic_category())
            , tr::f_("open write only file: {}", path)
        };

        if (perr) {
            *perr = err;
            return file::INVALID_FILE_HANDLE;
        } else {
            throw err;
        }
    }

    return h;
#endif // !__ANDROID__
}

inline void close (file::handle_type h)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

#   if _MSC_VER
    _close(h);
#   else
    ::close(h);
#   endif
#endif // !__ANDROID__
}

filesize_t offset (file::handle_type h)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

#   if _MSC_VER
    return static_cast<filesize_t>(_lseek(h, 0, SEEK_CUR));
#   else
    return static_cast<filesize_t>(::lseek(h, 0, SEEK_CUR));
#   endif
#endif // !__ANDROID__
}

filesize_t read (file::handle_type h, char * buffer, filesize_t len, error * perr)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

#   if _MSC_VER
    auto n = _read(h, buffer, len);
#   else
    auto n = ::read(h, buffer, len);
#   endif

    if (n < 0) {
        error err {
              std::error_code(errno, std::generic_category())
            , tr::_("read from file")
        };

        if (perr) {
            *perr = err;
            return -1;
        } else {
            throw err;
        }
    }

    return n;
#endif // !__ANDROID__
}

filesize_t write (file::handle_type h, char const * buffer, filesize_t len, error * perr)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

#   if _MSC_VER
    auto n = _write(h, buffer, len);
#   else
    auto n = ::write(h, buffer, len);
#   endif

    if (n < 0) {
        error err {
              std::error_code(errno, std::generic_category())
            , tr::_("write into file")
        };

        if (perr) {
            *perr = err;
            return -1;
        } else {
            throw err;
        }
    }

    return n;
#endif // !__ANDROID__
}

bool set_pos (file::handle_type h, filesize_t offset, error * perr)
{
#if __ANDROID__
#   error Need to implement
#else // __ANDROID__

#   if _MSC_VER
    auto pos = static_cast<filesize_t>(_lseek(h, offset, SEEK_SET));
#   else
    auto pos = static_cast<filesize_t>(::lseek(h, offset, SEEK_SET));
#   endif

    if (pos < 0) {
        error err {
              std::error_code(errno, std::generic_category())
            , tr::_("set file position")
        };

        if (perr) {
            *perr = err;
            return false;
        } else {
            throw err;
        }
    }

    return true;
#endif // !__ANDROID__
}

} // namespace details

file::file (handle_type h) : _h(h) {}
file::file () {}

file::file (file && f)
{
    if (this != & f) {
        _h = f._h;
        f._h = INVALID_FILE_HANDLE;
    }
}

file & file::operator = (file && f)
{
    if (this != & f) {
        close();
        _h = f._h;
        f._h = INVALID_FILE_HANDLE;
    }

    return *this;
}

file::~file ()
{
    close();
}

void file::close () noexcept
{
    if (_h > 0)
        details::close(_h);

    _h = INVALID_FILE_HANDLE;
}

filesize_t file::offset () const
{
    return details::offset(_h);
}

#define READ_BUFFER_FROM_FILE_DESC tr::_("read buffer from file")

filesize_t file::read (char * buffer, filesize_t len, error * perr) const
{
    if (len == 0)
        return 0;

    if (len < 0) {
        error err {
              make_error_code(std::errc::invalid_argument)
            , tr::_("invalid buffer length")
        };

        if (perr) {
            *perr = err;
            return -1;
        } else {
            throw err;
        }
    }

    return details::read(_h, buffer, len, perr);
}

std::string file::read_all (error * perr) const
{
    std::string result;
    char buffer[512];

    auto n = details::read(_h, buffer, 512, perr);

    while (n > 0) {
        result.append(buffer, n);
        n = details::read(_h, buffer, 512, perr);
    }

    return n < 0 ? std::string{} : result;
}

filesize_t file::write (char const * buffer, filesize_t len, error * perr)
{
    return details::write(_h, buffer, len, perr);
}

bool file::set_pos (filesize_t offset, error * perr)
{
    return details::set_pos(_h, offset, perr);
}

file file::open_read_only (fs::path const & path, error * perr)
{
    return file{details::open_read_only(path, perr)};
}

file file::open_write_only (fs::path const & path, truncate_enum trunc, error * perr)
{
    return file{details::open_write_only(path, trunc, perr)};
}

bool file::rewrite (fs::path const & path, char const * buffer
    , filesize_t count, error * perr)
{
    file f = open_write_only(path, truncate_enum::on, perr);

    if (f) {
        auto n = f.write(buffer, count, perr);
        return n >= 0;
    }

    return false;
}

}} // namespace netty::p2p
