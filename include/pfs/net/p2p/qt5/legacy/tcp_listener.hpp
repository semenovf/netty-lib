////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
//      2021.09.28 Reimplemented (using CRTP).
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../listener.hpp"
#include "../utils.hpp"
#include "endpoint.hpp"
#include "pfs/fmt.hpp"
#include "pfs/memory.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class tcp_listener : public basic_listener<tcp_listener, endpoint>
{
    using base_class = basic_listener<tcp_listener, endpoint>;
    using tcp_server_type = std::unique_ptr<QTcpServer>;
    using tcp_socket_type = std::unique_ptr<QTcpSocket>;

    friend class basic_listener<tcp_listener, endpoint>;

private:
    struct internal_options {
        QHostAddress listener_addr4;
        quint16 listener_port;
        QNetworkInterface listener_interface;
    };

    bool _started {false};
    internal_options _opts;
    tcp_server_type _listener;

protected:
    bool set_options_impl (options && opts)
    {
        if (_started) {
            failure("unable to set options during operation");
            return false;
        }

        if (opts.listener_addr4 == inet4_addr{})
            _opts.listener_addr4 = QHostAddress::AnyIPv4;
        else
            _opts.listener_addr4 = QHostAddress{static_cast<quint32>(opts.listener_addr4)};

        if (_opts.listener_addr4.isNull()) {
            failure("bad listener address");
            return false;
        }

        _opts.listener_port = opts.listener_port;

        if (!opts.listener_interface.empty() && opts.listener_interface != "*") {
            _opts.listener_interface = QNetworkInterface::interfaceFromName(
                QString::fromStdString(opts.listener_interface));

            if (!_opts.listener_interface.isValid()) {
                failure("bad listener interface specified");
                return false;
            }
        }

        return true;
    }

    bool start_impl ()
    {
        auto rc = false;

        if (!_started) {
            do {
                assert(_listener.get() == nullptr);
                _listener = pfs::make_unique<QTcpServer>();

                // There is no standard Qt-way (API) to configure socket for QTcpServer.
                // Here (https://stackoverflow.com/questions/47268023/how-to-set-so-reuseaddr-on-the-socket-used-by-qtcpserver)
                // and hear (https://stackoverflow.com/questions/54216722/qtcpserver-bind-socket-with-custom-options)
                // are the instructions to do this.
                // These sources may help later.

                if (!_listener->listen(_opts.listener_addr4, _opts.listener_port)) {
                    failure(fmt::format("start listening failure: {}"
                        , _listener->errorString().toStdString()));
                    break;
                }

                QObject::connect(& *_listener, & QTcpServer::acceptError, [this] (
                    QAbstractSocket::SocketError /*socketError*/) {

                    failure(fmt::format("accept error: {}"
                        , _listener->errorString().toStdString()));
                });

                QObject::connect(& *_listener, & QTcpServer::newConnection, [this] {
                    QTcpSocket * socket = _listener->nextPendingConnection();

                    while (socket) {
                        shared_endpoint ep {new endpoint(socket)};

                        QObject::connect(socket, & QTcpSocket::readyRead, [ep] {
                            ep->ready_read();
                        });

                        QObject::connect(socket, & QTcpSocket::disconnected, [this, ep] {
                            this->disconnected(ep);
                        });

                        QObject::connect(socket
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
                            , & QTcpSocket::errorOccurred
#else
                            , static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(& QTcpSocket::error)
#endif
                            , [this, ep, socket] (QTcpSocket::SocketError error_code) {
                                // RemoteHostClosedError will not consider an error
                                if (error_code != QAbstractSocket::RemoteHostClosedError) {
                                    this->endpoint_failure(ep, socket->errorString().toStdString());
                                }
                        });

                        accepted(ep);

                        socket = _listener->nextPendingConnection();
                    }
                });

                _started = true;
                rc = true;
            } while (false);
        } else {
            rc = true;
        }

        if (!rc) {
            _listener.reset();
        }

        return rc;
    }

    void stop_impl ()
    {
        if (_started) {
            _listener.reset();
            _started = false;
        }
    }

    bool started_impl () const noexcept
    {
        return _started;
    }

    inet4_addr address_impl () const noexcept
    {
        return inet4_addr{_opts.listener_addr4.toIPv4Address()};
    }

    std::uint16_t port_impl () const noexcept
    {
        return _opts.listener_port;
    }

public:
    tcp_listener () = default;

    ~tcp_listener ()
    {
        if (_started)
            stop_impl();
    }

    tcp_listener (tcp_listener const &) = delete;
    tcp_listener & operator = (tcp_listener const &) = delete;

    tcp_listener (tcp_listener &&) = default;
    tcp_listener & operator = (tcp_listener &&) = default;
};

}}}} // namespace pfs::net::p2p::qt5
