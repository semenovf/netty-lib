////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/netty/udt/udt_server.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"

namespace netty {
namespace udt {

basic_udt_server::basic_udt_server (socket4_addr const & saddr, int mtu, int exp_max_counter
    , std::chrono::milliseconds exp_threshold)
    : basic_socket(type_enum::dgram, mtu, exp_max_counter, exp_threshold)
{
    sockaddr_in addr_in4;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = AF_INET;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(saddr.port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(saddr.addr));

    auto rc = UDT::bind(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (rc == UDT::ERROR) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::f_("bind name to socket failure: {}", to_string(saddr))
            , UDT::getlasterror_desc()
        };
    }

    _saddr = saddr;
}

basic_udt_server::basic_udt_server (socket4_addr const & saddr, int backlog
    , int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold)
    : basic_udt_server(saddr, mtu, exp_max_counter, exp_threshold)
{
    listen(backlog);
}

basic_udt_server::~basic_udt_server () = default;

bool basic_udt_server::listen (int backlog, error * perr)
{
    auto rc = UDT::listen(_socket, backlog);

    if (rc == UDT::ERROR) {
        error err {
              make_error_code(errc::socket_error)
            , tr::_("listen failure")
            , UDT::getlasterror_desc()
        };

        if (perr) {
            *perr = std::move(err);
        } else {
            throw err;
        }

        return false;
    }

    return true;
}

void basic_udt_server::accept (native_type listener_sock
    , basic_udt_socket & result,  error * perr)
{
    sockaddr sa;
    int addrlen;

    auto u = UDT::accept(listener_sock, & sa, & addrlen);

    if (u > 0) {
        if (sa.sa_family == AF_INET) {
            auto addr_in4_ptr = reinterpret_cast<sockaddr_in *>(& sa);

            auto addr = pfs::to_native_order(
                static_cast<std::uint32_t>(addr_in4_ptr->sin_addr.s_addr));

            auto port = pfs::to_native_order(
                static_cast<std::uint16_t>(addr_in4_ptr->sin_port));

            //UDT::setsockopt(_socket, 0, UDT_LINGER, new linger{1, 3}, sizeof(linger));

            int exp_max_counter = 16;
            int exp_max_counter_size = sizeof(exp_max_counter);
            UDT::getsockopt(listener_sock, 0, UDT_EXP_MAX_COUNTER, & exp_max_counter, & exp_max_counter_size);

            std::uint64_t exp_threshold = 5000000;
            int exp_threshold_size = sizeof(exp_threshold);
            int mtu = 1500;
            UDT::getsockopt(listener_sock, 0, UDT_EXP_THRESHOLD, & exp_threshold, & exp_threshold_size);

            UDT::setsockopt(u, 0, UDT_EXP_MAX_COUNTER, & exp_max_counter, sizeof(exp_max_counter));
            UDT::setsockopt(u, 0, UDT_EXP_THRESHOLD  , & exp_threshold, sizeof(exp_threshold));
            UDT::setsockopt(u, 0, UDT_MSS            , & mtu, sizeof(mtu));

            result = basic_udt_socket{u, socket4_addr{addr, port}};
            return;
        } else {
            error err {
                  make_error_code(errc::socket_error)
                , tr::_("socket accept failure: unsupported sockaddr family"
                  " (AF_INET supported only)")
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }
        }
    } else {
        error err {
              make_error_code(errc::socket_error)
            , tr::_("socket accept failure")
            , UDT::getlasterror_desc()
        };

        if (perr) {
            *perr = std::move(err);
        } else {
            throw err;
        }
    }

    result = basic_udt_socket{uninitialized{}};
}

void basic_udt_server::accept (basic_udt_socket & result, error * perr)
{
    accept(_socket, result, perr);
}

}} // namespace netty::udt
