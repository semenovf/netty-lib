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
// #include "pfs/fmt.hpp"
// #include "pfs/memory.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
//
// #if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
// #   include <QNetworkDatagram>
// #endif

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class listener : public basic_listener<listener>
{
    using base_class = basic_listener<listener>;
    using tcp_server_type = std::unique_ptr<QTcpServer>;
    using tcp_socket_type = std::unique_ptr<QTcpSocket>;
    //using connection_type = connection<backend_enum::qt5>;

    friend class basic_listener<listener>;

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
            base_class::failure("unable to set options during operation");
            return false;
        }

        if (opts.listener_addr4 == "*")
            _opts.listener_addr4 = QHostAddress::AnyIPv4;
        else
            _opts.listener_addr4 = QHostAddress{QString::fromStdString(opts.listener_addr4)};

        if (_opts.listener_addr4.isNull()) {
            base_class::failure("bad listener address");
            return false;
        }

        _opts.listener_port = opts.listener_port;

        if (!opts.listener_interface.empty() && opts.listener_interface != "*") {
            _opts.listener_interface = QNetworkInterface::interfaceFromName(
                QString::fromStdString(opts.listener_interface));

            if (!_opts.listener_interface.isValid()) {
                base_class::failure("bad listener interface specified");
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

                if (!_listener->listen(_opts.listener_addr4, _opts.listener_port)) {
                    base_class::failure(fmt::format("start listening failure: {}"
                        , _listener->errorString().toStdString()));
                    break;
                }

                _listener->connect(& *_listener, & QTcpServer::acceptError, [this] (
                    QAbstractSocket::SocketError /*socketError*/) {

                    base_class::failure(fmt::format("accept error: {}"
                        , _listener->errorString().toStdString()));

                });

                _listener->connect(& *_listener, & QTcpServer::newConnection, [this] {
                    QTcpSocket * peer = _listener->nextPendingConnection();

                    while (peer) {
                        base_class::accepted();
                        peer = _listener->nextPendingConnection();
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

public:
    listener () = default;

    ~listener ()
    {
        if (_started)
            stop_impl();
    }
};

}}}} // namespace pfs::net::p2p::qt5
