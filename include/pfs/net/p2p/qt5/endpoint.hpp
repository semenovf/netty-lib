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
#include "pfs/optional.hpp"

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class listener;
class speaker;

inline inet4_addr peer_address (QTcpSocket const * socket)
{
    return (socket && socket->state() == QTcpSocket::ConnectedState)
        ? inet4_addr{socket->peerAddress().toIPv4Address()}
        : inet4_addr{};
}

inline std::uint16_t peer_port (QTcpSocket const * socket)
{
    return (socket && socket->state() == QTcpSocket::ConnectedState)
        ? socket->peerPort()
        : 0;
}

class endpoint : public basic_endpoint<endpoint>
    , public std::enable_shared_from_this<endpoint>
{
    using base_class = basic_endpoint<endpoint>;

    template <typename O>
    using optional_type = pfs::optional<O>;

    friend class basic_endpoint<endpoint>;
    friend class listener;
    friend class speaker;

    QTcpSocket * _socket {nullptr};

protected:
    endpoint (QTcpSocket * socket)
        : base_class(qt5::peer_address(socket), qt5::peer_port(socket))
        , _socket(socket)
    {}

    endpoint_state state_impl () const
    {
        if (_socket) {
            switch (_socket->state()) {
                case QTcpSocket::UnconnectedState:
                    return endpoint_state::disconnected;
                case QTcpSocket::HostLookupState:
                    return endpoint_state::hostlookup;
                case QTcpSocket::ConnectingState:
                    return endpoint_state::connecting;
                case QTcpSocket::ConnectedState:
                    return endpoint_state::connected;
                case QTcpSocket::BoundState:
                    return endpoint_state::bound;
                case QTcpSocket::ClosingState:
                    return endpoint_state::closing;
                default:
                    break;
            }
        }

        return endpoint_state::disconnected;
    }

    void disconnect_impl ()
    {
        if (_socket)
            _socket->disconnectFromHost();
    }

    std::int32_t send_impl (char const * data, std::int32_t size)
    {
        Q_ASSERT(_socket);

        auto bytes_sent = _socket->write(data, size);
        return bytes_sent < 0
            ? -1
            : static_cast<std::int32_t>(bytes_sent);
    }

    std::int32_t recv_impl (char * data, std::int32_t size)
    {
        Q_ASSERT(_socket);

        auto bytes_received = _socket->read(data, size);
        return bytes_received < 0
            ? -1
            : static_cast<std::int32_t>(bytes_received);
    }

public:
    ~endpoint ()
    {
        // NOTE for peer socket.
        // The socket is created as a child of the server, which means that it
        // is automatically deleted when the QTcpServer object is destroyed.
        // It is still a good idea to delete the object explicitly when you are
        // done with it, to avoid wasting memory.

        if (_socket) {
            _socket->disconnectFromHost();
            delete _socket;
        }
    }

    endpoint (endpoint const &) = delete;
    endpoint & operator = (endpoint const &) = delete;

    endpoint (endpoint && other)
        : base_class(std::move(other))
        , _socket(other._socket)
    {
        other._socket = nullptr;
    }

    endpoint & operator = (endpoint && other)
    {
        if (this != & other) {
            this->~endpoint();
            base_class::operator = (std::move(other));
            _socket = other._socket;
            other._socket = nullptr;
        }

        return *this;
    }
};

using shared_endpoint = std::shared_ptr<endpoint>;

}}}} // namespace pfs::net::p2p::qt5
