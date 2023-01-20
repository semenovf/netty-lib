////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/i18n.hpp"

namespace netty {

char const * error_category::name () const noexcept
{
    return "netty::category";
}

std::string error_category::message (int ev) const
{
    switch (static_cast<errc>(ev)) {
        case errc::success:
            return tr::_("no error");
        case errc::system_error:
            return tr::_("system error");
        case errc::invalid_argument:
            return tr::_("invalid argument");
        case errc::device_not_found:
            return tr::_("device not found");
        case errc::permissions_denied:
            return tr::_("permissions denied");
        case errc::name_too_long:
            return tr::_("name too long");
        case errc::poller_error:
            return tr::_("poller error");
        case errc::socket_error:
            return tr::_("socket error");
        case errc::filesystem_error:
            return tr::_("filesystem error");
        case errc::unexpected_error:
            return tr::_("unexpected error");

        default: return tr::_("unknown net error");
    }
}

} // namespace netty

