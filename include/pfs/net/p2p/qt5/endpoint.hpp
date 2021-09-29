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
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            , & QTcpSocket::errorOccurred
#else
            , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
#endif
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

class peer_endpoint : public basic_peer_endpoint<peer_endpoint>
{
    using base_class = basic_peer_endpoint<peer_endpoint>;

    friend class basic_peer_endpoint<peer_endpoint>;

    QTcpSocket * _socket {nullptr};

public:
    peer_endpoint (QTcpSocket * socket)
        : base_class()
        , _socket(socket)
    {
        QObject::connect(_socket, & QTcpSocket::disconnected, [this] {
            qDebug("---DISCONNECTED---");
            base_class::disconnected();
        });

        QObject::connect(_socket
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            , & QTcpSocket::errorOccurred
#else
            , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
#endif
            , [this] (QTcpSocket::SocketError) {
                qDebug("---FAILURE---");
                base_class::failure(_socket->errorString().toStdString());
        });
    }

    ~peer_endpoint ()
    {
        // The socket is created as a child of the server, which means that it
        // is automatically deleted when the QTcpServer object is destroyed.
        // It is still a good idea to delete the object explicitly when you are
        // done with it, to avoid wasting memory.
        if (_socket)
            delete _socket;
    }

    peer_endpoint (peer_endpoint const &) = delete;
    peer_endpoint & operator = (peer_endpoint const &) = delete;

    peer_endpoint (peer_endpoint && other)
        : _socket(other._socket)
    {
        other._socket = nullptr;
    }

    peer_endpoint & operator = (peer_endpoint && other)
    {
        if (this != & other) {
            this->~peer_endpoint();
            _socket = other._socket;
            other._socket = nullptr;
        }

        return *this;
    }
};

}}}} // namespace pfs::net::p2p::qt5

