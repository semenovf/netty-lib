////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "connection_qt5.hpp"
#include "pfs/net/p2p/listener.hpp"
#include "pfs/memory.hpp"
#include "pfs/fmt.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <cassert>

// #if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
// #   include <QNetworkDatagram>
// #endif

namespace pfs {
namespace net {
namespace p2p {

////////////////////////////////////////////////////////////////////////////////
// Backend implemenation
////////////////////////////////////////////////////////////////////////////////
// Options
struct backend_options {
    QHostAddress listener_addr4;
    quint16 listener_port;
    QNetworkInterface listener_interface;
};

template <>
class listener<backend_enum::qt5>::backend
{
    using tcp_server_type = std::unique_ptr<QTcpServer>;
    using tcp_socket_type = std::unique_ptr<QTcpSocket>;
    using connection_type = connection<backend_enum::qt5>;

    listener<backend_enum::qt5> * _holder_ptr {nullptr};
    bool _started {false};
    backend_options _opts;
    tcp_server_type _listener;

public:
    backend (listener<backend_enum::qt5> & holder)
        : _holder_ptr(& holder)
    {}

    ~backend ()
    {
        if (_started)
            stop();
    }

    bool set_options (options && opts)
    {
        if (_started) {
            _holder_ptr->failure("unable to set options during operation");
            return false;
        }

        if (opts.listener_addr4 == "*")
            _opts.listener_addr4 = QHostAddress::AnyIPv4;
        else
            _opts.listener_addr4 = QHostAddress{QString::fromStdString(opts.listener_addr4)};

        if (_opts.listener_addr4.isNull()) {
            _holder_ptr->failure("bad listener address");
            return false;
        }

        _opts.listener_port = opts.listener_port;

        if (!opts.listener_interface.empty() && opts.listener_interface != "*") {
            _opts.listener_interface = QNetworkInterface::interfaceFromName(
                QString::fromStdString(opts.listener_interface));

            if (!_opts.listener_interface.isValid()) {
                _holder_ptr->failure("bad listener interface specified");
                return false;
            }
        }

        return true;
    }

    bool start ()
    {
        auto rc = false;

        if (!_started) {
            do {
                assert(_listener.get() == nullptr);
                _listener = pfs::make_unique<QTcpServer>();

                if (!_listener->listen(_opts.listener_addr4, _opts.listener_port)) {
                    _holder_ptr->failure(fmt::format("start listening failure: {}"
                        , _listener->errorString().toStdString()));
                    break;
                }

                _listener->connect(& *_listener, & QTcpServer::acceptError, [this] (
                    QAbstractSocket::SocketError /*socketError*/) {

                    _holder_ptr->failure(fmt::format("accept error: {}"
                        , _listener->errorString().toStdString()));

                });

                _listener->connect(& *_listener, & QTcpServer::newConnection, [this] {
                    QTcpSocket * peer = _listener->nextPendingConnection();

                    while (peer) {
                        // TODO emit new connection
                        connection_type conn {pfs::make_unique<connection_type::backend>(peer)};
                        _holder_ptr->connected(conn);

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

    void stop ()
    {
        if (_started) {
            _listener.reset();
            _started = false;
        }
    }

    bool started () const noexcept
    {
        return _started;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

template <>
listener<backend_enum::qt5>::listener ()
    : _p(pfs::make_unique<backend>(*this))
{}

template <>
listener<backend_enum::qt5>::listener (listener &&) = default;

template <>
listener<backend_enum::qt5> & listener<backend_enum::qt5>::operator = (listener &&) = default;

template <>
listener<backend_enum::qt5>::~listener ()
{}

template<>
bool listener<backend_enum::qt5>::set_options (options && opts)
{
    return _p->set_options(std::move(opts));
}

template<>
bool listener<backend_enum::qt5>::start ()
{
    return _p->start();
}

template<>
void listener<backend_enum::qt5>::stop ()
{
    _p->stop();
}

template<>
bool listener<backend_enum::qt5>::started () const noexcept
{
    return _p->started();
}

}}} // namespace pfs::net::p2p

