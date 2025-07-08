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
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
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

namespace patterns {
namespace delivery {

/**
 * @param Transport Underlying transport.
 * @param MessageIdTraits Message identifier traits (identifier type definition, to_string() converter, etc).
 */
template <typename Transport
    , typename MessageId
    , typename ExchangeController
    , typename MessageQueue
    , typename RecursiveWriterMutex>
class manager
{
    friend ExchangeController;

    using controller_type = ExchangeController;

public:
    using transport_type = Transport;
    using address_type = typename transport_type::address_type;
    using message_id = MessageId;
    using writer_mutex_type = RecursiveWriterMutex;

private:
    transport_type * _transport {nullptr};
    std::unordered_map<address_type, controller_type> _controllers;
    std::atomic_bool _interrupted {false};
    writer_mutex_type _writer_mtx;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    callback_t<void (address_type)> _on_receiver_ready;
    callback_t<void (address_type, message_id, int, std::vector<char>)> _on_message_received;
    callback_t<void (address_type, message_id)> _on_message_delivered;
    callback_t<void (address_type, message_id)> _on_message_lost;
    callback_t<void (address_type, int, std::vector<char>)> _on_report_received;
    callback_t<void (address_type, message_id, std::size_t)> _on_message_receiving_begin;
    callback_t<void (address_type, message_id, std::size_t, std::size_t)> _on_message_receiving_progress;

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
     *          void (address_type, message_id, int priority, std::vector<char> msg)
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
     * @details Callback @a f signature must match:
     *          void (address_type, int priority, std::vector<char> report)
     */
    template <typename F>
    manager & on_report_received (F && f)
    {
        _on_report_received = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify receiver about of starting the message receiving.
     *
     * @details Callback @a f signature must match:
     *          void (address_type, message_id, std::size_t total_size)
     */
    template <typename F>
    manager & on_message_receiving_begin (F && f)
    {
        _on_message_receiving_begin = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify receiver about message receiving progress (optional).
     *
     * @details Callback @a f signature must match:
     *          void (address_type, message_id, std::size_t received_size, std::size_t total_size)
     */
    template <typename F>
    manager & on_message_receiving_progress (F && f)
    {
        _on_message_receiving_progress = std::forward<F>(f);
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

        controller_type & c = res.first->second;

        return c;
    }

    // For:
    //      * send SYN and ACK packets (priority = 0 and force_checksum = true)
    //      * send message parts (by outgoing controller)
    bool enqueue_private (address_type sender_addr, std::vector<char> && data, int priority = 0
        , bool force_checksum = true)
    {
        return _transport->enqueue(sender_addr, priority, force_checksum, std::move(data));
    }

    void process_error (std::string const & errstr)
    {
        _on_error(errstr);
    }

    void process_ready (address_type sender_addr)
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
        , std::vector<char> && msg)
    {
        if (_on_message_received)
            _on_message_received(sender_addr, msgid, priority, std::move(msg));
    };

    void process_report_received (address_type sender_addr, int priority, std::vector<char> && report)
    {
        if (_on_report_received)
            _on_report_received(sender_addr, priority, std::move(report));
    }

    void process_message_receiving_begin (address_type sender_addr, message_id msgid
        , std::size_t total_size)
    {
        if (_on_message_receiving_begin)
            _on_message_receiving_begin(sender_addr, msgid, total_size);
    }

    void process_message_receiving_progress (address_type sender_addr, message_id msgid
        , std::size_t received_size, std::size_t total_size)
    {
        if (_on_message_receiving_progress)
            _on_message_receiving_progress(sender_addr, msgid, received_size, total_size);
    }

public:
    void pause (address_type addr)
    {
        auto pos = _controllers.find(addr);

        if (pos != _controllers.end()) {
            auto & c = pos->second;
            c.pause();
        }
    }

    void resume (address_type addr)
    {
        auto pos = _controllers.find(addr);

        if (pos != _controllers.end()) {
            auto & c = pos->second;
            c.resume();
        }
    }

    bool enqueue_message (address_type addr, message_id msgid, int priority, bool force_checksum
        , std::vector<char> msg)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto & c = ensure_controller(addr);
        return c.enqueue_message(msgid, priority, force_checksum, std::move(msg));
    }

    bool enqueue_message (address_type addr, message_id msgid, int priority, bool force_checksum
        , char const * msg, std::size_t length)
    {
        return enqueue_message(addr, msgid, priority, force_checksum, std::vector<char>(msg, msg + length));
    }

    bool enqueue_static_message (address_type addr, message_id msgid, int priority
        , bool force_checksum, char const * msg, std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto & c = ensure_controller(addr);
        return c.enqueue_static_message(msgid, priority, force_checksum, msg, length);
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum, char const * data
        , std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = controller_type::serialize_report(data, length);
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum, std::vector<char> data)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = controller_type::serialize_report(std::move(data));
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

    /**
     * Incoming packet handler.
     *
     * @details Must be called when data received by underlying transport (e.g. from transport
     *          callback for handle incoming data)
     */
    void process_input (address_type sender_addr, int priority, std::vector<char> data)
    {
        if (!data.empty()) {
            auto & c = ensure_controller(sender_addr);
            c.process_input(this, priority, std::move(data));
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
            auto & c = x.second;

            if (!c.paused())
                n += c.step(this);
        }

        n += _transport->step();

        return n;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
