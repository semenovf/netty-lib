////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
//      2025.07.02 Refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../tag.hpp"
#include "../../trace.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace delivery {

/**
 * @param Transport Underlying transport.
 * @param MessageId Message identifier.
 * @param DeliveryController Delivery controller class (@see delivery_controller).
 * @param RecursiveWriterMutex Recursive mutex for write operations (@see std::recursive_mutex)
 */
template <typename Transport
    , typename MessageId
    , typename DeliveryController
    , typename RecursiveWriterMutex = std::recursive_mutex>
class manager
{
    friend DeliveryController;

    using controller_type = DeliveryController;

public:
    using transport_type = Transport;
    using serializer_traits_type = typename DeliveryController::serializer_traits_type;
    using archive_type = typename serializer_traits_type::archive_type;
    using address_type = typename transport_type::address_type;
    using message_id = MessageId;
    using writer_mutex_type = RecursiveWriterMutex;

private:
    transport_type * _transport {nullptr};
    std::unordered_map<address_type, controller_type> _controllers;
    writer_mutex_type _writer_mtx;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    callback_t<void (address_type)> _on_receiver_ready;
    callback_t<void (address_type, message_id, int, archive_type)> _on_message_received;
    callback_t<void (address_type, message_id)> _on_message_delivered;
    callback_t<void (address_type, message_id)> _on_message_lost;
    callback_t<void (address_type, int, archive_type)> _on_report_received;
    callback_t<void (address_type, message_id, std::size_t)> _on_message_begin;
    callback_t<void (address_type, message_id, std::size_t, std::size_t)> _on_message_progress;

public:
    manager (transport_type & transport)
        : _transport(& transport)
    {}

    manager (manager const &) = delete;
    manager (manager &&) = delete;
    manager & operator = (manager const &) = delete;
    manager & operator = (manager &&) = delete;

    ~manager () {}

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    manager & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    /**
     * @details Callback @a f signature must match:
     *          void (address_type)
     */
    template <typename F>
    manager & on_receiver_ready (F && f)
    {
        _on_receiver_ready = std::forward<F>(f);
        return *this;
    }

    /**
     * @details Callback @a f signature must match:
     *          void (address_type, message_id, int priority, archive_type msg)
     */
    template <typename F>
    manager & on_message_received (F && f)
    {
        _on_message_received = std::forward<F>(f);
        return *this;
    }

    /**
     * @details Callback @a f signature must match:
     *          void (address_type, message_id)
     */
    template <typename F>
    manager & on_message_delivered (F && f)
    {
        _on_message_delivered = std::forward<F>(f);
        return *this;
    }

    /**
     * @details Callback @a f signature must match:
     *          void (address_type, message_id)
     */
    template <typename F>
    manager & on_message_lost (F && f)
    {
        _on_message_lost = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify receiver about of starting the message receiving.
     *
     * @details Callback @a f signature must match:
     *          void (address_type, message_id, std::size_t total_size)
     */
    template <typename F>
    manager & on_message_begin (F && f)
    {
        _on_message_begin = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify receiver about message receiving progress (optional).
     *
     * @details Callback @a f signature must match:
     *          void (address_type, message_id, std::size_t received_size, std::size_t total_size)
     */
    template <typename F>
    manager & on_message_progress (F && f)
    {
        _on_message_progress = std::forward<F>(f);
        return *this;
    }

    /**
     * @details Callback @a f signature must match:
     *          void (address_type, int priority, archive_type report)
     */
    template <typename F>
    manager & on_report_received (F && f)
    {
        _on_report_received = std::forward<F>(f);
        return *this;
    }

private:
    controller_type & ensure_controller (address_type addr)
    {
        PFS__THROW_UNEXPECTED(_transport != nullptr, "Fix delivery::manager algorithm");

        auto pos = _controllers.find(addr);

        if (pos != _controllers.end())
            return pos->second;

        // TODO Bellow parameters must be configurable
        auto part_size = std::uint32_t{16384U};
        auto exp_timeout = std::chrono::milliseconds{3000};

        auto res = _controllers.emplace(addr, controller_type{addr, part_size, exp_timeout});

        PFS__THROW_UNEXPECTED(res.second, "Fix delivery::manager algorithm");

        controller_type & dc = res.first->second;

        return dc;
    }

    // For:
    //      * send SYN and ACK packets (priority = 0)
    //      * send message parts (by outgoing controller)
    bool enqueue_private (address_type sender_addr, archive_type && data, int priority = 0)
    {
        return _transport->enqueue(sender_addr, priority, std::move(data));
    }

    void process_error (std::string const & errstr)
    {
        _on_error(errstr);
    }

    void process_peer_ready (address_type sender_addr)
    {
        if (_on_receiver_ready)
            _on_receiver_ready(sender_addr);
    }

    void process_message_lost (address_type sender_addr, message_id msgid)
    {
        if (_on_message_lost)
            _on_message_lost(sender_addr, msgid);
    }

    void process_message_delivered (address_type sender_addr, message_id msgid)
    {
        if (_on_message_delivered)
            _on_message_delivered(sender_addr, msgid);
    }

    void process_message_received (address_type sender_addr, message_id msgid, int priority
        , archive_type && msg)
    {
        if (_on_message_received)
            _on_message_received(sender_addr, msgid, priority, std::move(msg));
    };

    void process_report_received (address_type sender_addr, int priority, archive_type && report)
    {
        if (_on_report_received)
            _on_report_received(sender_addr, priority, std::move(report));
    }

    void process_message_begin (address_type sender_addr, message_id msgid
        , std::size_t total_size)
    {
        if (_on_message_begin)
            _on_message_begin(sender_addr, msgid, total_size);
    }

    void process_message_progress (address_type sender_addr, message_id msgid
        , std::size_t received_size, std::size_t total_size)
    {
        if (_on_message_progress)
            _on_message_progress(sender_addr, msgid, received_size, total_size);
    }

public:
    void pause (address_type addr)
    {
        auto pos = _controllers.find(addr);

        if (pos != _controllers.end()) {
            auto & dc = pos->second;
            dc.pause();
        }
    }

    void resume (address_type addr)
    {
        auto pos = _controllers.find(addr);

        if (pos != _controllers.end()) {
            auto & dc = pos->second;
            dc.resume();
        }
    }

    bool enqueue_message (address_type addr, message_id msgid, int priority, archive_type msg)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto & dc = ensure_controller(addr);
        return dc.enqueue_message(msgid, priority, std::move(msg));
    }

    bool enqueue_message (address_type addr, message_id msgid, int priority, char const * msg
        , std::size_t length)
    {
        return enqueue_message(addr, msgid, priority, archive_type(msg, length));
    }

    bool enqueue_static_message (address_type addr, message_id msgid, int priority
        , char const * msg, std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto & dc = ensure_controller(addr);
        return dc.enqueue_static_message(msgid, priority, msg, length);
    }

    bool enqueue_report (address_type addr, int priority, char const * data
        , std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = controller_type::serialize_report(data, length);
        return _transport->enqueue(addr, priority, std::move(report));
    }

    bool enqueue_report (address_type addr, int priority, archive_type data)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = controller_type::serialize_report(data);
        return _transport->enqueue(addr, priority, std::move(report));
    }

    /**
     * Incoming packet handler.
     *
     * @details Must be called when data received by underlying transport (e.g. from transport
     *          callback for handle incoming data)
     */
    void process_input (address_type sender_addr, int priority, archive_type data)
    {
        if (!data.empty()) {
            auto & dc = ensure_controller(sender_addr);
            dc.process_input(this, priority, std::move(data));
        }
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        unsigned int n = 0;

        for (auto & x: _controllers) {
            auto & dc = x.second;

            if (!dc.paused())
                n += dc.step(this);
        }

        n += _transport->step();

        return n;
    }
};

} // namespace delivery

NETTY__NAMESPACE_END
