////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/linux/netlink_socket.hpp"
#include "pfs/i18n.hpp"
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>

namespace netty {
namespace linux_os {

netlink_socket::netlink_socket () = default;

netlink_socket::netlink_socket (type_enum netlinktype)
{
    switch (netlinktype) {
        case type_enum::route:
            _socket = mnl_socket_open(NETLINK_ROUTE);
            break;
        default: {
            throw error {
                  errc::socket_error
                , tr::f_("bad/unsupported Netlink socket type: {}", netlinktype)
            };
        }
    }

    if (_socket == nullptr) {
        throw error {
              errc::socket_error
            , tr::_("create Netlink socket failure")
            //, pfs::system_error_text() // FIXME Need error description
        };
    }

    auto rc = mnl_socket_bind(_socket, RTMGRP_LINK | RTMGRP_IPV4_IFADDR
        /* | RTMGRP_NOTIFY | RTMGRP_IPV6_IFADDR */
        , MNL_SOCKET_AUTOPID);

    if (rc < 0) {
        throw error {
              errc::socket_error
            , tr::_("bind Netlink socket failure")
            //, pfs::system_error_text() // FIXME Need error description
        };
    }
}

netlink_socket::~netlink_socket ()
{
    if (_socket != nullptr)
        mnl_socket_close(_socket);

    _socket = nullptr;
}

netlink_socket::netlink_socket (netlink_socket && other)
{
    this->operator = (std::move(other));
}

netlink_socket & netlink_socket::operator = (netlink_socket &&)
{
    return *this;
}

netlink_socket::operator bool () const noexcept
{
    return _socket != nullptr;
}

netlink_socket::native_type netlink_socket::native () const noexcept
{
    if (_socket == nullptr)
        return kINVALID_SOCKET;

    return mnl_socket_get_fd(_socket);
}

int netlink_socket::recv (char * data, int len, error * perr)
{
    auto n = mnl_socket_recvfrom(_socket, data, len);

    if (n < 0) {
        error err {
              errc::socket_error
            , tr::_("receive data from Netlink socket failure")
            // , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
                return n;
        } else {
            throw err;
        }
    }

    return n;
}

int netlink_socket::send (char const * req, int len, error * perr)
{
    auto n = mnl_socket_sendto(_socket, req, len);

    if (n < 0) {
        error err {
              errc::socket_error
            , tr::_("send Netlink request failure")
            // , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
                return n;
        } else {
            throw err;
        }
    }

    return n;
}

}} // namespace netty::linux_os

