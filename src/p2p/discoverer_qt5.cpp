////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/p2p/discoverer.hpp"
#include "pfs/fmt.hpp"
#include "pfs/memory.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <iostream>
#include <cassert>

#if QT_VERSION >= QT_VERSION_CHECK(5,8,0)
#   include <QNetworkDatagram>
#endif

namespace pfs {
namespace net {
namespace p2p {

enum class muticast_group_op { join, leave };

static bool is_remote_addr (QHostAddress const & addr)
{
    assert(!addr.isNull());

    if (addr.isLoopback() || QNetworkInterface::allAddresses().contains(addr))
        return false;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Backend implemenation
////////////////////////////////////////////////////////////////////////////////
// Options
struct backend_options {
    QHostAddress listener_addr4;
    quint16 listener_port;
    QNetworkInterface listener_interface;
    QHostAddress peer_addr4;
};

template <>
class discoverer<backend_enum::qt5>::backend
{
    using udp_socket_type = std::unique_ptr<QUdpSocket>;

    discoverer<backend_enum::qt5> * _holder_ptr {nullptr};
    bool _started {false};
    backend_options _opts;
    udp_socket_type _listener;
    udp_socket_type _radio;

private:
    bool process_multicast_group (muticast_group_op op)
    {
        QHostAddress group_addr4 {_opts.peer_addr4};

        if (_listener->state() != QUdpSocket::BoundState) {
            _holder_ptr->failure("listener is not bound");
            return false;
        }

        if (op == muticast_group_op::join) {
            auto joining_result = _opts.listener_interface.isValid()
                ? _listener->joinMulticastGroup(group_addr4, _opts.listener_interface)
                : _listener->joinMulticastGroup(group_addr4);

            if (!joining_result) {
                _holder_ptr->failure(fmt::format("joining listener to multicast group failure: {}"
                    , group_addr4.toString().toStdString()));
                return false;
            }
        } else {
            auto leaving_result = _opts.listener_interface.isValid()
                ? _listener->leaveMulticastGroup(group_addr4, _opts.listener_interface)
                : _listener->leaveMulticastGroup(group_addr4);

            if (!leaving_result) {
                _holder_ptr->failure(fmt::format("leaving listener from multicast group failure: {}"
                    , group_addr4.toString().toStdString()));
                return false;
            }
        }

        return true;
    }

    void process_incoming_datagrams ()
    {
        while (_listener->hasPendingDatagrams()) {

#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
            // using QUdpSocket::readDatagram (API since Qt 4)
            QByteArray datagram;
            QHostAddress sender_hostaddr;
            datagram.resize(static_cast<int>(_listener->pendingDatagramSize()));
            _listener->readDatagram(datagram.data(), datagram.size(), & sender_hostaddr);
            auto request = std::string(datagram.data(), datagram.size());
#else
            // using QUdpSocket::receiveDatagram (API since Qt 5.8)
            QNetworkDatagram datagram = _listener->receiveDatagram();
            QByteArray bytes = datagram.data();
            auto request = std::string(bytes.data(), bytes.size());
            auto sender_hostaddr = datagram.senderAddress();
#endif

            bool ok {true};

            // IPv6 address (TODO implement support of IPv6)
            inet4_addr sender_inet4_addr = sender_hostaddr.toIPv4Address(& ok);

            if (!ok) {
                _holder_ptr->failure("bad remote address (expected IPv4)");
            } else {
                _holder_ptr->incoming_data_received(sender_inet4_addr
                    , !is_remote_addr(sender_hostaddr)
                    , request);
            }
        }
    }

public:
    backend (discoverer<backend_enum::qt5> & holder)
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

        if (opts.peer_addr4 == "*")
            _opts.peer_addr4 = QHostAddress{"255.255.255.255"};
        else
            _opts.peer_addr4 = QHostAddress{QString::fromStdString(opts.peer_addr4)};

        if (_opts.peer_addr4.isNull()) {
            _holder_ptr->failure("bad radio address");
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
                _listener = pfs::make_unique<QUdpSocket>();

                QUdpSocket::BindMode bind_mode = QUdpSocket::ShareAddress
                    | QUdpSocket::ReuseAddressHint;

                if (!_listener->bind(_opts.listener_addr4, _opts.listener_port, bind_mode)) {
                    _holder_ptr->failure("listener socket binding failure");
                    break;
                }

#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
                if (_opts.peer_addr4.isBroadcast()) {
#else
                if (_opts.peer_addr4 == QHostAddress{"255.255.255.255"}) {
#endif
                    // TODO Implement
                } else if (_opts.peer_addr4.isMulticast()) {
                    if (! process_multicast_group(muticast_group_op::join))
                        break;
                } else {
                    // Unicast
                    ;
                }

                _listener->connect(& *_listener, & QUdpSocket::readyRead, [this] {
                    this->process_incoming_datagrams();
                });

                assert(_radio.get() == nullptr);
                _radio = pfs::make_unique<QUdpSocket>();

                _started = true;
                rc = true;
            } while (false);
        } else {
            rc = true;
        }

        if (!rc) {
            _listener.reset();
            _radio.reset();
        }

        return rc;
    }

    void stop ()
    {
        if (_started) {
            if (_opts.peer_addr4.isMulticast())
                process_multicast_group(muticast_group_op::leave);

            _listener.reset();
            _radio.reset();
            _started = false;
        }
    }

    bool started () const noexcept
    {
        return _started;
    }

    void radiocast (std::string const & data)
    {
        if (_radio) {
            _radio->writeDatagram(data.data(), data.size()
                , _opts.peer_addr4, _opts.listener_port);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

template <>
discoverer<backend_enum::qt5>::discoverer ()
    : _p(pfs::make_unique<backend>(*this))
{}

template <>
discoverer<backend_enum::qt5>::discoverer (discoverer &&) = default;

template <>
discoverer<backend_enum::qt5> & discoverer<backend_enum::qt5>::operator = (discoverer &&) = default;

template <>
discoverer<backend_enum::qt5>::~discoverer ()
{
    // Explicitly disconnect all detectors to avoid sigfault while distruction
    incoming_data_received.disconnect_all();
    failure.disconnect_all();
}

template<>
bool discoverer<backend_enum::qt5>::set_options (options && opts)
{
    return _p->set_options(std::move(opts));
}

template<>
bool discoverer<backend_enum::qt5>::start ()
{
    return _p->start();
}

template<>
void discoverer<backend_enum::qt5>::stop ()
{
    _p->stop();
}

template<>
bool discoverer<backend_enum::qt5>::started () const noexcept
{
    return _p->started();
}

template<>
void discoverer<backend_enum::qt5>::radiocast (std::string const & data)
{
    _p->radiocast(data);
}

}}} // namespace pfs::net::p2p
