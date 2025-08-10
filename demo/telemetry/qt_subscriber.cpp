////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "qt_subscriber.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/timer_pool.hpp>
#include <chrono>

static pfs::timer_pool s_timer_pool;

qt_subscriber::qt_subscriber (netty::socket4_addr saddr)
{
    QObject::connect(& _sub, & QTcpSocket::connected, & _loop, & QEventLoop::quit);
    QObject::connect(& _sub, & QTcpSocket::disconnected, & _loop, & QEventLoop::quit);

    QObject::connect(& _sub, & QTcpSocket::errorOccurred, & _loop, [this] (QAbstractSocket::SocketError) {
        PFS__THROW_UNEXPECTED(false, tr::f_("Error on socket: {}", _sub.errorString().toStdString()));
    });

    QObject::connect(& _sub, & QTcpSocket::readyRead, [this] () {
        _buf.append(_sub.readAll());
        _step_result = 1;
    });

    s_timer_pool.create(std::chrono::milliseconds{1000}, [this] () { _loop.quit(); });

    _sub.connectToHost(QString::fromStdString(to_string(saddr.addr)), saddr.port);
    _loop.exec();

    PFS__THROW_UNEXPECTED(_sub.state() == QAbstractSocket::ConnectedState
        , tr::f_("Connection failed: {}", _sub.errorString().toStdString()));
}

unsigned int qt_subscriber::step (std::vector<char> & buf)
{
    s_timer_pool.create(std::chrono::milliseconds{100}, [this] () { _loop.quit(); });
    _loop.exec();

    // zmq::message_t zmsg;
    // zmq::recv_result_t rc = _sub.recv(zmsg, zmq::recv_flags::dontwait);
    //
    // if (!rc.has_value())
    //     return 0;
    //
    // auto off = buf.size();
    // buf.resize(off + zmsg.size());
    // std::memcpy(buf.data() + off, static_cast<char const *>(zmsg.data()), zmsg.size());

    unsigned int n = _step_result;
    _step_result = 0;
    return n;
}
