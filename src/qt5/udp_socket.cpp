////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/qt5/udp_socket.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include <array>

namespace netty {
namespace p2p {
namespace qt5 {

udp_socket::udp_socket () = default;
udp_socket::~udp_socket () = default;

inet4_addr udp_socket::addr () const noexcept
{
    // FIXME Is this true?
    return (state() == udp_socket::BOUND)
        ? _socket.localAddress().toIPv4Address()
        : _socket.peerAddress().toIPv4Address();
}

std::uint16_t udp_socket::port () const noexcept
{
    // FIXME Is this true?
    return (state() == udp_socket::BOUND)
        ? _socket.localPort()
        : _socket.peerPort();
}

socket4_addr udp_socket::saddr () const noexcept
{
    return socket4_addr{addr(), port()};
}

QNetworkInterface udp_socket::iface_by_address (std::string const & addr)
{
    QNetworkInterface defaultInterface;
    QList<QNetworkInterface> ifaces(QNetworkInterface::allInterfaces());
    QString addrstr = QString::fromStdString(addr);

    LOG_TRACE_3("Searching network interface by address: {}"
        , addr.empty() ? "<empty>" : addr);
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

            if (address_entries[j].ip().toString() == addrstr)
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

void udp_socket::process_multicast_group (inet4_addr addr
    , std::string const & network_iface
    , muticast_group_op op)
{
    auto group_addr = (addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(addr)};

    if (!group_addr.isMulticast()) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::f_("bad multicast address: {}", to_string(addr))
        };
    }

    QNetworkInterface iface;

    if (!network_iface.empty() && network_iface != "*") {
        iface = QNetworkInterface::interfaceFromName(QString::fromStdString(network_iface));
    } else {
        iface = iface_by_address(network_iface);
    }

    if (!iface.isValid()) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("bad interface specified")
        };
    }

    LOG_TRACE_2("{} multicast interface: {} [{}]"
        , op == muticast_group_op::join ? "Join" : "Leave"
        , iface.humanReadableName().toStdString()
        , iface.hardwareAddress().toStdString());

    if (_socket.state() != QUdpSocket::BoundState) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("listener is not in bound state")
        };
    }

    if (op == muticast_group_op::join) {
        auto joining_result = iface.isValid()
            ? _socket.joinMulticastGroup(group_addr, iface)
            : _socket.joinMulticastGroup(group_addr);

        if (!joining_result) {
            throw error {
                  make_error_code(errc::socket_error)
                , tr::f_("joining listener to multicast group failure: {}"
                    , group_addr.toString().toStdString())
            };
        }
    } else {
        auto leaving_result = iface.isValid()
            ? _socket.leaveMulticastGroup(group_addr, iface)
            : _socket.leaveMulticastGroup(group_addr);

        if (!leaving_result) {
            throw error {
                  make_error_code(errc::socket_error)
                , tr::f_("leaving listener from multicast group failure: {}"
                    , group_addr.toString().toStdString())
            };
        }
    }
}

void udp_socket::bind (socket4_addr saddr)
{
    QUdpSocket::BindMode bind_mode = QUdpSocket::ShareAddress
        | QUdpSocket::ReuseAddressHint;

    auto hostaddr = (saddr.addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(saddr.addr)};

    if (!_socket.bind(hostaddr, saddr.port, bind_mode)) {
        throw error {
                make_error_code(errc::socket_error)
            , tr::f_("listener socket binding failure: {}:{}"
                , to_string(saddr.addr), saddr.port)
        };
    }

    if (_socket.state() != QUdpSocket::BoundState) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::f_("listener socket expected in bound state: {}:{}"
                , to_string(saddr.addr), saddr.port)
        };
    }
}

void udp_socket::process_incoming_data ()
{
    while (_socket.hasPendingDatagrams()) {

#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        // using QUdpSocket::readDatagram (API since Qt 4)
        QByteArray packet_data;
        QHostAddress sender_hostaddr;
        std::uint16_t sender_port;
        packet_data.resize(static_cast<int>(_socket.pendingDatagramSize()));
        _socket.readDatagram(datagram.data(), datagram.size()
            , & sender_hostaddr, & sender_port);
#else
        // using QUdpSocket::receiveDatagram (API since Qt 5.8)
        QNetworkDatagram datagram = _socket.receiveDatagram();
        QByteArray packet_data = datagram.data();
        auto sender_hostaddr = datagram.senderAddress();

        if (datagram.senderPort() < 0) {
            // FIXME
//             throw error {
//                   make_error_code(errc::socket_error)
//                 , tr::_("no sender port associated with datagram")
//             };
            continue;
        }

        auto sender_port = static_cast<std::uint16_t>(datagram.senderPort());
#endif

        bool ok {true};
        inet4_addr sender_inet4_addr = sender_hostaddr.toIPv4Address(& ok);

        if (!ok) {
            // FIXME
//             throw error {
//                   make_error_code(errc::socket_error)
//                 , tr::_("bad remote address (expected IPv4)")
//             };

            continue;
        }

        data_ready(socket4_addr{sender_inet4_addr, sender_port}
            , packet_data.data()
            , static_cast<std::size_t>(packet_data.size()));
    }
}

std::streamsize udp_socket::send (char const * data, std::streamsize size
    , socket4_addr saddr)
{
    auto hostaddr = (saddr.addr == inet4_addr{})
        ? QHostAddress::AnyIPv4
        : QHostAddress{static_cast<quint32>(saddr.addr)};

    return _socket.writeDatagram(data, size, hostaddr, saddr.port);
}

std::string udp_socket::state_string (state_enum status)
{
    static std::array<char const *, std::size_t{LISTENING + 1}> __status_strings = {
          "UNCONNECTED"
        , "HOSTLOOKUP"
        , "CONNECTING"
        , "CONNECTED"
        , "BOUND"
        , "CLOSING"
        , "LISTENING"
    };

    return std::string{__status_strings[status]};
}

}}} // namespace netty::p2p::qt5
