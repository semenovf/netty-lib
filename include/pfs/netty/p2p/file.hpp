////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
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

namespace netty {
namespace p2p {

#if __ANDROID__
    // Can contain both file system path and content URI (content provider dependent).
    using filepath_t = std::string;
#else
    using filepath_t = pfs::filesystem::path;
#endif

namespace fs = pfs::filesystem;

using filesize_t = std::int32_t;

enum class truncate_enum: std::int8_t { off, on };

class file
{
public:
    static constexpr filesize_t const INVALID_FILE_HANDLE = -1;
    using handle_type = int;

private:
    handle_type _h {INVALID_FILE_HANDLE};

private:
    file (handle_type h);

public:
    file ();

    file (file const & f) = delete;
    file & operator = (file const & f) = delete;

    file (file && f);
    file & operator = (file && f);
    ~file ();

    operator bool () const noexcept
    {
        return _h >= 0;
    }

    void close () noexcept;
    filesize_t offset () const;

    /**
     * Read data chunk from file.
     *
     * @param ifh File descriptor.
     * @param buffer Pointer to input buffer.
     * @param len Input buffer size.
     * @param offset Offset from start to read (-1 for start read from current position).
     * @param perr Pointer to store error if not @c null. If @a perr is null
     *        function may throw an error
     *
     * @return Actually read chunk size or -1 on error.
     *
     * @note Limit maximum file size to 2,147,479,552 bytes. For transfer bigger
     *       files use another way/tools (scp, for example).
     */
    filesize_t read (char * buffer, filesize_t len, error * perr = nullptr) const;

    template <typename T>
    inline filesize_t read (T & value, error * perr = nullptr) const
    {
        return read(reinterpret_cast<char *>(& value), sizeof(T), perr);
    }

    /**
     * Read all content from file started from specified @a offset.
     */
    std::string read_all (error * perr = nullptr) const;

    /**
     * @brief Write buffer to file.
     */
    filesize_t write (char const * buffer, filesize_t count, error * perr = nullptr);

    /**
     * @brief Write value to file.
     */
    template <typename T>
    inline filesize_t write (T const & value, error * perr = nullptr)
    {
        return write(reinterpret_cast<char const *>(& value), sizeof(T), perr);
    }

    /**
     * Set file position by @a offset.
     */
    bool set_pos (filesize_t offset, error * perr = nullptr);

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
    static file open_read_only (filepath_t const & path, error * perr = nullptr);

    /**
    * @brief Open file for writing.
    *
    * @return Output file handle or @c INVALID_FILE_HANDLE on error. In last case
    *         @a ec set to appropriate error code:
    *         - POSIX-specific error codes returned by @c ::open and
    *           @c ::lseek calls.
    */
    static file open_write_only (filepath_t const & path, truncate_enum trunc
        , error * perr = nullptr);


    static file open_write_only (filepath_t const & path, error * perr = nullptr)
    {
        return open_write_only(path, truncate_enum::off, perr);
    }

    /**
     * Rewrite file with content from @a text.
     */
    static bool rewrite (filepath_t const & path, char const * buffer
        , filesize_t count, error * perr = nullptr);


    static void rewrite (filepath_t const & path, std::string const & text)
    {
        rewrite(path, text.c_str(), static_cast<filesize_t>(text.size()));
    }

    static std::string read_all (filepath_t const & path, error * perr = nullptr)
    {
        auto f = file::open_read_only(path, perr);

        if (f)
            return f.read_all(perr);

        return std::string{};
    }
};

using file_t  = file;
using ifile_t = file;
using ofile_t = file;

}} // namespace netty::p2p
