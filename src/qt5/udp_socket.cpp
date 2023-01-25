////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/netty/qt5/udp_socket.hpp"
#include <QNetworkInterface>

#if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
#   include <QNetworkDatagram>
#endif

namespace netty {
namespace qt5 {

QNetworkInterface iface_by_inet4_addr (inet4_addr addr)
{
    QNetworkInterface defaultInterface;
    QList<QNetworkInterface> ifaces(QNetworkInterface::allInterfaces());

    LOG_TRACE_3("Searching network interface by address: {}", addr);
    LOG_TRACE_3("Scan available network interfaces: {}", ifaces.size());

    for (int i = 0; i < ifaces.size(); i++) {
        LOG_TRACE_3("    Network interface: {} [{}]"
            , ifaces[i].humanReadableName().toStdString()
            , ifaces[i].hardwareAddress().toStdString());

        if (!defaultInterface.isValid()) {
            auto flags = ifaces[i].flags();
            auto canBeDefault = flags & QNetworkInterface::IsUp
                && flags & QNetworkInterface::IsRunning
                && !(flags & QNetworkInterface::IsLoopBack);

            if (canBeDefault) {
                defaultInterface = ifaces[i];
            }
        }

        QList<QNetworkAddressEntry> address_entries(ifaces[i].addressEntries());

        for (int j = 0; j < address_entries.size(); j++) {
            bool match = false;

            if (address_entries[j].ip().toIPv4Address() == addr)
                match = true;

            LOG_TRACE_3("        Address entry {} {} {}"
                , address_entries[j].ip().toString().toStdString()
                , match ? "match" : "not match"
                , addr);

            if (match)
                return ifaces[i];
        }
    }

    if (defaultInterface.isValid()) {
        LOG_TRACE_3("No network interface matches, use default: {} [{}]"
            , defaultInterface.humanReadableName().toStdString()
            , defaultInterface.hardwareAddress().toStdString());
    }

    return defaultInterface;
}

udp_socket::udp_socket (uninitialized) {}
udp_socket::udp_socket (): _socket(new QUdpSocket) {}
udp_socket::udp_socket (udp_socket &&) = default;
udp_socket & udp_socket::operator = (udp_socket &&) = default;
udp_socket::~udp_socket () = default;

bool udp_socket::join (socket4_addr const & group_saddr
    , inet4_addr const & local_addr, error * perr)
{
    auto group_addr = (group_saddr.addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(group_saddr.addr)};

    auto iface = iface_by_inet4_addr(local_addr);

    auto rc = iface.isValid()
        ? _socket->joinMulticastGroup(group_addr, iface)
        : _socket->joinMulticastGroup(group_addr);

    if (!rc) {
        error err {
              errc::socket_error
            , tr::f_("join multicast group error: {}"
                , to_string(group_saddr.addr))
            , _socket->errorString().toStdString()
        };

        if (perr) {
            *perr = std::move(err);
            return false;
        } else {
            throw err;
        }
    }

    return true;
}

bool udp_socket::leave (socket4_addr const & group_saddr
    , inet4_addr const & local_addr, error * perr)
{
    auto group_addr = (group_saddr.addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(group_saddr.addr)};

    auto iface = iface_by_inet4_addr(local_addr);

    auto rc = iface.isValid()
        ? _socket->leaveMulticastGroup(group_addr, iface)
        : _socket->leaveMulticastGroup(group_addr);

    if (!rc) {
        error err {
              errc::socket_error
            , tr::f_("join multicast group error: {}"
                , to_string(group_saddr.addr))
            , _socket->errorString().toStdString()
        };

        if (perr) {
            *perr = std::move(err);
            return false;
        } else {
            throw err;
        }
    }

    return true;
}

bool udp_socket::enable_broadcast (bool enable, error * perr)
{
    // TODO Implement
    return true;
}

udp_socket::operator bool () const noexcept
{
    return _socket.get() != nullptr;
}

udp_socket::native_type udp_socket::native () const noexcept
{
    return _socket.get() == nullptr ? -1 : _socket->socketDescriptor();
}

int udp_socket::available (error * /*perr*/) const
{
    return _socket.get() == nullptr
        ? 0
        : _socket->pendingDatagramSize();
}

int udp_socket::recv_from (char * data, int len, socket4_addr * saddr, error * perr)
{
// #if QT_VERSION < QT_VERSION_CHECK(5,8,0)
    // using QUdpSocket::readDatagram (API since Qt 4)
    QHostAddress sender_hostaddr;
    std::uint16_t sender_port;
    auto n = _socket->readDatagram(data, len, & sender_hostaddr, & sender_port);

    if (n < 0) {
        error err {
              errc::socket_error
            , tr::_("receive data failure")
            , _socket->errorString().toStdString()
        };

        if (perr) {
            *perr = std::move(err);
            return n;
        } else {
            throw err;
        }
    }

    if (saddr) {
        saddr->addr = sender_hostaddr.toIPv4Address();
        saddr->port = sender_port;
    }

// #else
//     // using QUdpSocket::receiveDatagram (API since Qt 5.8)
//     QNetworkDatagram datagram = _socket.receiveDatagram();
//     QByteArray packet_data = datagram.data();
//     auto sender_hostaddr = datagram.senderAddress();
//
//         if (datagram.senderPort() < 0) {
//             // FIXME
// //             throw error {
// //                   errc::socket_error
// //                 , tr::_("no sender port associated with datagram")
// //             };
//         }
//
//         auto sender_port = static_cast<std::uint16_t>(datagram.senderPort());
// #endif

    return n;
}

send_result udp_socket::send_to (socket4_addr const & dest, char const * data, int len, error * perr)
{
    auto hostaddr = (dest.addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(dest.addr)};

    auto n = _socket->writeDatagram(data, len, hostaddr, dest.port);
    return send_result{send_status::good, static_cast<int>(n)};
}

}} // namespace netty::qt5
