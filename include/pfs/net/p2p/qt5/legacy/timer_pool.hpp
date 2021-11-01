////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/emitter.hpp"
#include <QTimer>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class timer_pool
{
    QTimer _observer_timer;
    QTimer _discovery_timer;
//     QTimer _pending_timer; // FIXME DELETE LATER

public: // signals
    emitter_mt<> discovery_timer_timeout;
    emitter_mt<> observer_timer_timeout;
    //emitter_mt<> pending_timer_timeout; // FIXME DELETE LATER

public:
    timer_pool ()
    {
        _observer_timer.setTimerType(Qt::PreciseTimer);
        _observer_timer.setSingleShot(true);

        QObject::connect(& _observer_timer, & QTimer::timeout, [this] {
            this->observer_timer_timeout();
        });

        _discovery_timer.setTimerType(Qt::PreciseTimer);
        _discovery_timer.setSingleShot(false);

        QObject::connect(& _discovery_timer, & QTimer::timeout, [this] {
            this->discovery_timer_timeout();
        });

       // FIXME DELETE LATER
//         _pending_timer.setTimerType(Qt::PreciseTimer);
//         _pending_timer.setSingleShot(false);
//
//         QObject::connect(& _pending_timer, & QTimer::timeout, [this] {
//             this->pending_timer_timeout();
//         });
    }

    void start_discovery_timer (std::chrono::milliseconds millis)
    {
        _discovery_timer.start(millis.count());
    }

    void start_observer_timer (std::chrono::milliseconds millis)
    {
        _observer_timer.start(millis.count());
    }

    // FIXME DELETE LATER
//     void start_pending_timer (std::chrono::milliseconds millis)
//     {
//         _pending_timer.start(millis.count());
//     }
};

}}}} // namespace pfs::net::p2p::qt5
