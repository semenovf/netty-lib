////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "exports.hpp"
#include "error.hpp"
#include <string>

namespace netty {

/**
 * Returns MTU (Maximum Transfer Unit) value of a device specified
 * by @a iface, or @c -1 if error occured. In last case @c ec will be set to
 * appropriate error code:
 *     * @c errc::permissions_denied if underlying system call(s) need specific
 *       privileges;
 *     * @c errc::name_too_long if @a interface name is too long than allowed by
 *       underlying system call(s);
 *     * @c errc::device_not_found if @a interface specifies bad device;
 *     * @c errc::system_error if system specific call returns error,
 *       check @c errno value.
 */
NETTY__EXPORT int mtu (std::string const & iface, std::error_code & ec) noexcept;

/**
 * Returns MTU (Maximum Transfer Unit) value of a device specified
 * by @a iface.
 *
 * @throws std::system_error
 */
inline int mtu (std::string const & iface)
{
    std::error_code ec;
    auto result = mtu(iface, ec);

    if (ec)
        throw std::system_error(ec);

    return result;
}

} // namespace netty
