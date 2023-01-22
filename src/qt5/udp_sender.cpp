////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/qt5/udp_sender.hpp"
#include <QHostAddress>
#include <QNetworkInterface>

namespace netty {
namespace qt5 {

extern QNetworkInterface iface_by_inet4_addr (inet4_addr addr);

udp_sender::udp_sender () : udp_socket() {}

udp_sender::udp_sender (udp_sender && s)
    : udp_socket(std::move(s))
{}

udp_sender & udp_sender::operator = (udp_sender && s)
{
    udp_sender::operator = (std::move(s));
    return *this;
}

udp_sender::~udp_sender () = default;

bool udp_sender::set_multicast_interface (inet4_addr const & local_addr
    , error * perr)
{
    QUdpSocket::BindMode bind_mode = QUdpSocket::ShareAddress
        | QUdpSocket::ReuseAddressHint;

    auto hostaddr = (local_addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(local_addr)};

    auto success = _socket->bind(hostaddr, 0, bind_mode);

    if (!success) {
        error err {
              make_error_code(errc::socket_error)
            , tr::f_("bind UDP socket failure to: {}", to_string(local_addr))
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return false;
        } else {
            throw err;
        }
    }

    auto iface = iface_by_inet4_addr(local_addr);

    if (!iface.isValid()) {
        error err {
              make_error_code(errc::socket_error)
            , tr::f_("interface not found  for local address: {}"
                , to_string(local_addr))
        };

        if (perr) {
            *perr = std::move(err);
            return false;
        } else {
            throw err;
        }
    }

    _socket->setMulticastInterface(iface);

    return true;
}

bool udp_sender::enable_broadcast (bool enable, error * perr)
{
    return udp_socket::enable_broadcast(enable, perr);
}

}} // namespace netty::posix


