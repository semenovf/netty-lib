////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include <memory>

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace netty {
namespace posix {

tcp_socket::tcp_socket () : inet_socket(type_enum::stream) {}

tcp_socket::tcp_socket (bool) : inet_socket() {}

// Accepted socket
tcp_socket::tcp_socket (native_type sock, socket4_addr const & saddr)
    : inet_socket(sock, saddr)
{
    _state = state_enum::connected;
}

tcp_socket::tcp_socket (tcp_socket && other)
    : inet_socket(std::move(other))
{}

tcp_socket & tcp_socket::operator = (tcp_socket && other)
{
    inet_socket::operator = (std::move(other));
    _state = other._state;
    other._state = state_enum::unconnected;
    return *this;
}

tcp_socket::~tcp_socket () = default;

bool tcp_socket::connect (socket4_addr const & saddr, error * perr)
{
    sockaddr_in addr_in4;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = AF_INET;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(saddr.port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(saddr.addr));

    auto rc = ::connect(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    bool in_progress = false;

    if (rc < 0) {
        in_progress = (errno == EINPROGRESS);

        if (!in_progress) {
            error err {
                make_error_code(errc::socket_error)
                , tr::_("socket connect error")
                , pfs::system_error_text()
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }

            return false;
        }
    }

    if (in_progress) {
        _state = state_enum::connecting;
    } else {
        _state = state_enum::connected;
    }

    _saddr = saddr;

    return (_state == state_enum::connected);
}

bool tcp_socket::connected () const
{
    return _state == state_enum::connected;
}

bool tcp_socket::ensure_connected ()
{
    if (_state == state_enum::connected)
        return true;

    if (_state == state_enum::connecting) {
        int val;
        socklen_t len = sizeof(val);

        auto rc = getsockopt(_socket, SOL_SOCKET, SO_ERROR, & val, & len);

        if (rc != 0) {
            error err {
                make_error_code(errc::socket_error)
                , tr::_("get socket option failure")
                , pfs::system_error_text()
            };

            throw err;
        }

        if (val == 0)
            _state = state_enum::connected;
    }

    return (_state == state_enum::connected);
}

}} // namespace netty::posix
