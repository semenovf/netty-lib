////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../speaker.hpp"
#include "../utils.hpp"
#include "endpoint.hpp"
#include <memory>
#include <unordered_map>
#include <cassert>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class speaker : public basic_speaker<speaker, endpoint>
{
    using base_class = basic_speaker<speaker, endpoint>;

    friend class basic_speaker<speaker, endpoint>;

private:
    std::unordered_map<QTcpSocket *, std::chrono::milliseconds> _pending_sockets;

protected:
    void connect_impl (uuid_t peer_uuid, inet4_addr const & addr, std::uint16_t port)
    {
        auto now = current_timepoint();
        auto expiration_timepoint = now + std::chrono::milliseconds{10000};

        auto socket = new QTcpSocket;
        auto res = _pending_sockets.insert({socket, expiration_timepoint});
        assert(res.second);

        QObject::connect(socket, & QTcpSocket::connected, [this, socket, peer_uuid] {
            auto it = _pending_sockets.find(socket);
            assert(it != _pending_sockets.end());
            assert(it->first == socket);
            _pending_sockets.erase(it);

            shared_endpoint ep {new endpoint(socket)};
            ep->set_peer_uuid(peer_uuid);

            QObject::connect(socket, & QTcpSocket::disconnected, [this, socket, ep] {
                auto it = _pending_sockets.find(socket);

                if (it != _pending_sockets.end())
                    _pending_sockets.erase(it);

                this->disconnected(ep);
            });

            QObject::connect(socket
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
                , & QTcpSocket::errorOccurred
#else
                , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
#endif
                , [this, socket, ep] (QTcpSocket::SocketError error_code) {
                    // RemoteHostClosedError will not consider an error
                    if (error_code != QAbstractSocket::RemoteHostClosedError) {
                        this->endpoint_failure(ep, socket->errorString().toStdString());
                    }
            });

            this->connected(ep);
        });

        auto listener_addr = QHostAddress{static_cast<std::uint32_t>(addr)};
        socket->connectToHost(listener_addr, port, QTcpSocket::ReadWrite);
    }

public:
    speaker () = default;
    ~speaker () = default;

    speaker (speaker const &) = delete;
    speaker & operator = (speaker const &) = delete;

    speaker (speaker &&) = default;
    speaker & operator = (speaker &&) = default;
};

}}}} // namespace pfs::net::p2p::qt5

