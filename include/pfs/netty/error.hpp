////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/error.hpp"

namespace netty {

////////////////////////////////////////////////////////////////////////////////
// Error codes, category, exception class
////////////////////////////////////////////////////////////////////////////////
using error_code = std::error_code;
using error = pfs::error;

enum class errc
{
      success = 0
    , engine_error       // General purpose errors.
    , system_error       // More information can be obtained using errno (Linux) or
                         // WSAGetLastError (Windows)
    , device_not_found
    , permissions_denied
    , name_too_long
    , socket_error       // Socket operation error
    , poller_error       // Errors occurred in poller
    , filesystem_error
    , unexpected_error
};

class error_category : public std::error_category
{
public:
    virtual char const * name () const noexcept override
    {
        return "netty::category";
    }

    virtual std::string message (int ev) const override
    {
        switch (ev) {
            case static_cast<int>(errc::success):
                return std::string{"no error"};
            case static_cast<int>(errc::system_error):
                return std::string{"system specific error, check errno value"};
            case static_cast<int>(errc::device_not_found):
                return std::string{"device not found"};
            case static_cast<int>(errc::permissions_denied):
                return std::string{"permissions denied"};
            case static_cast<int>(errc::name_too_long):
                return std::string{"name too long"};
            case static_cast<int>(errc::socket_error):
                return std::string{"socket error"};
            case static_cast<int>(errc::poller_error):
                return std::string{"poller error"};
            case static_cast<int>(errc::filesystem_error):
                return std::string{"filesystem error"};
            case static_cast<int>(errc::unexpected_error):
                return std::string{"unexpected error"};

            default: return std::string{"unknown net error"};
        }
    }
};

inline std::error_category const & get_error_category ()
{
    static error_category instance;
    return instance;
}

inline std::error_code make_error_code (errc e)
{
    return std::error_code(static_cast<int>(e), get_error_category());
}

inline std::system_error make_exception (errc e)
{
    return std::system_error(make_error_code(e));
}

} // namespace netty
