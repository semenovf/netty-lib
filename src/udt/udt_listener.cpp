////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
//      2024.07.29 `basic_udt_listener` renamed to `basic_udt_listener`.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/netty/udt/udt_listener.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"

namespace netty {
namespace udt {

void udt_listener::init (socket4_addr const & saddr, error * perr)
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
        pfs::throw_or(perr,  error {
              errc::socket_error
            , tr::f_("bind name to socket failure: {}", to_string(saddr))
            , UDT::getlasterror_desc()
        });

        return;
    }

    _saddr = saddr;
}

udt_listener::udt_listener ()
    : udt_socket(uninitialized{})
{}

udt_listener::udt_listener (socket4_addr const & saddr, int mtu, int exp_max_counter
    , std::chrono::milliseconds exp_threshold
    , error * perr)
    : udt_socket(type_enum::dgram, mtu, exp_max_counter, exp_threshold)
{
    init(saddr);
}

udt_listener::udt_listener (socket4_addr const & saddr, int backlog
    , int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold, error * perr)
    : udt_listener(saddr, mtu, exp_max_counter, exp_threshold, perr)
{
    if (perr && *perr)
        return;

    listen(backlog, perr);
}

udt_listener::udt_listener (socket4_addr const & saddr, property_map_t const & props, error * perr)
    : udt_socket(props, perr)
{
    init(saddr);
}

udt_listener::udt_listener (socket4_addr const & addr, int backlog, property_map_t const & props, error * perr)
    : udt_listener(addr, props, perr)
{
    if (perr && *perr)
        return;

    listen(backlog, perr);
}

udt_listener::~udt_listener () = default;

udt_listener::native_type udt_listener::native () const noexcept
{
    return udt_socket::native();
}

bool udt_listener::listen (int backlog, error * perr)
{
    auto rc = UDT::listen(_socket, backlog);

    if (rc == UDT::ERROR) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("listen failure")
            , UDT::getlasterror_desc()
        });

        return false;
    }

    return true;
}

// [static]
udt_socket udt_listener::accept (native_type listener_sock , error * perr)
{
    return accept_nonblocking(listener_sock, perr);
}

// [static]
udt_socket udt_listener::accept_nonblocking (native_type listener_sock, error * perr)
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

            return udt_socket{u, socket4_addr{addr, port}};
        } else {
            pfs::throw_or(perr, error {
                  errc::socket_error
                , tr::_("socket accept failure: unsupported sockaddr family"
                  " (AF_INET supported only)")
            });
        }
    } else {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("socket accept failure")
            , UDT::getlasterror_desc()
        });
    }

    return udt_socket{uninitialized{}};
}

}} // namespace netty::udt
