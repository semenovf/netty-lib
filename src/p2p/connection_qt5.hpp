////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/net/p2p/connection.hpp"
#include <QHostAddress>
#include <QTcpSocket>

#include <QDebug>

namespace pfs {
namespace net {
namespace p2p {

template <>
class connection<backend_enum::qt5>::backend
{
    connection<backend_enum::qt5> * _holder_ptr {nullptr};
    QTcpSocket * _socket {nullptr};

public:
    explicit backend (connection<backend_enum::qt5> & holder)
        : _holder_ptr(& holder)
    {
        _socket = new QTcpSocket;

        QObject::connect(_socket, & QTcpSocket::connected, [this] {
            qDebug("---CONNECTED---");
            _holder_ptr->connected(*_holder_ptr);
        });

        QObject::connect(_socket, & QTcpSocket::disconnected, [this] {
            qDebug("---DISCONNECTED---");
            _holder_ptr->disconnected(*_holder_ptr);
        });

        QObject::connect(_socket
            , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
            , [this] (QTcpSocket::SocketError) {
                qDebug("---FAILURE---");
                _holder_ptr->failure(*_holder_ptr, _socket->errorString().toStdString());
        });
    }

    ~backend ()
    {
        if (_socket) {
            _socket->disconnectFromHost();
            delete _socket;
        }
    }

    void accept (QTcpSocket * socket)
    {
        _socket = socket;
    }

    void connect (inet4_addr const & addr, std::uint16_t port)
    {
        auto listener_addr = QHostAddress{static_cast<std::uint32_t>(addr)};
        qDebug() << "--CONNECTING TO HOST---" << listener_addr << port;
        _socket->connectToHost(listener_addr, port, QTcpSocket::ReadWrite);
    }
};

}}} // namespace pfs::net::p2p
