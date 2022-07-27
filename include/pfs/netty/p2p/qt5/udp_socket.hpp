////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/fmt.hpp"
#include "pfs/memory.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <functional>
#include <string>
#include <cassert>

#if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
#   include <QNetworkDatagram>
#endif

namespace netty {
namespace p2p {
namespace qt5 {

class udp_socket
{
    enum class muticast_group_op { join, leave };

public:
    // Must be same as `QAbstractSocket::SocketState`
    enum state_enum {
          UNCONNECTED = 0
        , HOSTLOOKUP
        , CONNECTING
        , CONNECTED
        , BOUND
        , CLOSING
        , LISTENING
    };
private:
    QUdpSocket _socket;

public:
    std::function<void (std::string const &)> failure;

private:
    // If `addr` is empty select first running and non-loopback interface
    QNetworkInterface iface_by_address (std::string const & addr)
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

    bool process_multicast_group (inet4_addr const & addr
        , std::string const & network_iface
        , muticast_group_op op)
    {
        auto group_addr = (addr == inet4_addr{})
            ? QHostAddress::AnyIPv4
            : QHostAddress{static_cast<quint32>(addr)};

        if (!group_addr.isMulticast()) {
            failure(fmt::format("bad multicast address: {}"
                , to_string(addr)));
            return false;
        }

        QNetworkInterface iface;

        if (!network_iface.empty() && network_iface != "*") {
            iface = QNetworkInterface::interfaceFromName(QString::fromStdString(network_iface));
        } else {
            iface = iface_by_address(network_iface);
        }

        if (!iface.isValid()) {
            failure("bad interface specified");
            return false;
        }

        LOG_TRACE_2("{} multicast interface: {} [{}]"
            , op == muticast_group_op::join ? "Join" : "Leave"
            , iface.humanReadableName().toStdString()
            , iface.hardwareAddress().toStdString());

        if (_socket.state() != QUdpSocket::BoundState) {
            failure("listener is not bound");
            return false;
        }

        if (op == muticast_group_op::join) {
            auto joining_result = iface.isValid()
                ? _socket.joinMulticastGroup(group_addr, iface)
                : _socket.joinMulticastGroup(group_addr);

            if (!joining_result) {
                failure(fmt::format("joining listener to multicast group failure: {}"
                    , group_addr.toString().toStdString()));
                return false;
            }
        } else {
            auto leaving_result = iface.isValid()
                ? _socket.leaveMulticastGroup(group_addr, iface)
                : _socket.leaveMulticastGroup(group_addr);

            if (!leaving_result) {
                failure(fmt::format("leaving listener from multicast group failure: {}"
                    , group_addr.toString().toStdString()));
                return false;
            }
        }

        return true;
    }

public:
    inline std::string backend_string () const noexcept
    {
        return "Qt5";
    }

    inline state_enum state () const
    {
        return static_cast<state_enum>(_socket.state());
    }

    bool bind (inet4_addr const & addr, std::uint16_t port)
    {
        QUdpSocket::BindMode bind_mode = QUdpSocket::ShareAddress
            | QUdpSocket::ReuseAddressHint;

        auto hostaddr = (addr == inet4_addr{})
            ? QHostAddress::AnyIPv4
            : QHostAddress{static_cast<quint32>(addr)};

        if (!_socket.bind(hostaddr, port, bind_mode)) {
            failure(fmt::format("listener socket binding failure: {}:{}"
                , to_string(addr), port));
            return false;
        }

        assert(_socket.state() == QUdpSocket::BoundState);

        LOG_TRACE_1("listener socket in bound state: {}:{}"
            , to_string(addr), port);

        return true;
    }

    inline bool join_multicast_group (inet4_addr const & addr
        , std::string const & network_iface = "")
    {
        return process_multicast_group(addr, network_iface, muticast_group_op::join);
    }

    inline bool leave_multicast_group (inet4_addr const & addr
        , std::string const & network_iface = "")
    {
        return process_multicast_group(addr, network_iface, muticast_group_op::leave);
    }

    inline bool has_pending_data () const
    {
        return _socket.hasPendingDatagrams();
    }

    template <typename Callback>
    void process_incoming_data (Callback && callback)
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
                failure("no sender port associated with datagram");
                continue;
            }

            auto sender_port = static_cast<std::uint16_t>(datagram.senderPort());
#endif

            bool ok {true};
            inet4_addr sender_inet4_addr = sender_hostaddr.toIPv4Address(& ok);

            if (!ok) {
                failure("bad remote address (expected IPv4)");
                continue;
            }

            callback(sender_inet4_addr, sender_port, packet_data.data(), packet_data.size());
        }
    }

    std::streamsize send (char const * data, std::streamsize size
        , inet4_addr const & addr, std::uint16_t port)
    {
        auto hostaddr = (addr == inet4_addr{})
            ? QHostAddress::AnyIPv4
            : QHostAddress{static_cast<quint32>(addr)};

        return _socket.writeDatagram(data, size, hostaddr, port);
    }

    std::string error_string () const
    {
        return (_socket.error())
            ? _socket.errorString().toStdString()
            : std::string{};
    }

    static std::string state_string (state_enum status)
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

    inline std::string state_string () const noexcept
    {
        return state_string(state());
    }

public:
    udp_socket () = default;
    ~udp_socket () = default;

    udp_socket (udp_socket const &) = delete;
    udp_socket & operator = (udp_socket const &) = delete;

    udp_socket (udp_socket &&) = delete;
    udp_socket & operator = (udp_socket &&) = delete;
};

}}} // namespace netty::p2p::qt5
