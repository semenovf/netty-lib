////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.21 Initial version.
//      2025.03.11 Refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/error.hpp"

namespace netty {

using error_code = std::error_code;

enum class errc
{
      success = 0
    , protocol_version_error // Protocol version does not match
};

class error_category : public std::error_category
{
public:
    virtual char const * name () const noexcept override
    {
        return "pfs::netty::category";
    }

    virtual std::string message (int ev) const override
    {
        switch (static_cast<errc>(ev)) {
            case errc::success:
                return std::string{"no error"};

            case errc::protocol_version_error:
                return std::string{"protocol version does not match"};

            default: return std::string{"unknown error"};
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

class error: public pfs::error
{
public:
    using pfs::error::error;
};

} // namespace netty
