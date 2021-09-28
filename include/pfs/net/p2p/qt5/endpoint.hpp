////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../endpoint.hpp"
#include <QHostAddress>
#include <QTcpSocket>

#include <QDebug>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class origin_endpoint : public basic_origin_endpoint<origin_endpoint>
{
    using base_class = basic_origin_endpoint<origin_endpoint>;

    friend class basic_origin_endpoint<origin_endpoint>;

    QTcpSocket * _socket {nullptr};

protected:
    void connect_impl (inet4_addr const & addr, std::uint16_t port)
    {
        qDebug() << "--CONNECTING TO HOST---" << std::to_string(addr).c_str() << port;

        _socket = new QTcpSocket;
        auto listener_addr = QHostAddress{static_cast<std::uint32_t>(addr)};
        _socket->connectToHost(listener_addr, port, QTcpSocket::ReadWrite);
    }

public:
    origin_endpoint () : base_class()
    {
        QObject::connect(_socket, & QTcpSocket::connected, [this] {
            qDebug("---CONNECTED---");
            base_class::connected();
        });

        QObject::connect(_socket, & QTcpSocket::disconnected, [this] {
            qDebug("---DISCONNECTED---");
            base_class::disconnected();
        });

        QObject::connect(_socket
            , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
            , [this] (QTcpSocket::SocketError) {
                qDebug("---FAILURE---");
                base_class::failure(_socket->errorString().toStdString());
        });
    }

    ~origin_endpoint ()
    {
        if (_socket) {
            _socket->disconnectFromHost();
            delete _socket;
        }
    }

    origin_endpoint (origin_endpoint const &) = delete;
    origin_endpoint & operator = (origin_endpoint const &) = delete;

    origin_endpoint (origin_endpoint && other)
        : _socket(other._socket)
    {
        other._socket = nullptr;
    }

    origin_endpoint & operator = (origin_endpoint && other)
    {
        if (this != & other) {
            this->~origin_endpoint();
            _socket = other._socket;
            other._socket = nullptr;
        }

        return *this;
    }
};

//
// template <backend_enum Backend>
// class PFS_NET_LIB_DLL_EXPORT channel
// {
// //     socket      _origin;
// //     peer_socket _peer;
// //
// // public:
// //     channel ();
// //     ~channel ();
// //
// //     channel (channel const &) = delete;
// //     channel & operator = (channel const &) = delete;
// //
// //     channel (channel &&);
// //     channel & operator = (channel &&);
// //
// //     void set_origin (socket && s);
// //     void set_peer (peer_socket && s);
// };

}}}} // namespace pfs::net::p2p::qt5

