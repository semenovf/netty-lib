////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../tag.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/log.hpp>
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
    , typename MessageIdTraits
    , typename IncomingController
    , typename OutgoingController
    , typename WriterMutex>
class manager
{
    using incoming_controller_type = IncomingController;
    using outgoing_controller_type = OutgoingController;

public:
    using transport_type = Transport;
    using address_type = typename transport_type::address_type;
    using message_id_traits = MessageIdTraits;
    using message_id = typename message_id_traits::type;
    using writer_mutex_type = WriterMutex;

private:
    transport_type * _transport {nullptr};

    // Processors to handle incoming messages.
    std::unordered_map<address_type, incoming_controller_type> _icontrollers;

    // Controllers to track outgoing messages.
    std::unordered_map<address_type, outgoing_controller_type> _ocontrollers;

    std::atomic_bool _interrupted {false};

    writer_mutex_type _writer_mtx;

public:
    mutable callback_t<void (std::string const &)> on_error
        = [] (std::string const & msg) { LOGE(TAG, "{}", msg); };

    mutable callback_t<void (address_type)> on_receiver_ready = [] (address_type) {};

    mutable callback_t<void (address_type, message_id, std::vector<char>)> on_message_received
        = [] (address_type, message_id, std::vector<char>) {};

    mutable callback_t<void (address_type, message_id)> on_message_delivered
        = [] (address_type, message_id) {};

    mutable callback_t<void (address_type, std::vector<char>)> on_report_received
        = [] (address_type, std::vector<char>) {};

public:
    manager (transport_type & transport)
        : _transport(& transport)
    {}

    manager (manager const &) = delete;
    manager (manager &&) = delete;
    manager & operator = (manager const &) = delete;
    manager & operator = (manager &&) = delete;

    ~manager () {}

private:
    incoming_controller_type * ensure_incoming_controller (address_type addr)
    {
        PFS__ASSERT(_transport != nullptr, "");

        auto pos = _icontrollers.find(addr);

        if (pos != _icontrollers.end())
            return & pos->second;

        auto res = _icontrollers.emplace(addr, incoming_controller_type{});

        PFS__ASSERT(res.second, "");

        return & res.first->second;
    }

    outgoing_controller_type * ensure_outgoing_controller (address_type addr)
    {
        auto pos = _ocontrollers.find(addr);

        if (pos != _ocontrollers.end())
            return & pos->second;

        auto res = _ocontrollers.insert({addr, outgoing_controller_type{}});

        PFS__ASSERT(res.second, "");

        return & res.first->second;
    }

public:
    bool enqueue_message (address_type addr, message_id msgid, int priority, bool force_checksum
        , std::vector<char> && msg)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto outc = ensure_outgoing_controller(addr);
        return outc->enqueue_message(msgid, priority, force_checksum, std::move(msg));
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

        auto outc = ensure_outgoing_controller(addr);
        return outc->enqueue_static_message(msgid, priority, force_checksum, msg, length);
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum, char const * data
        , std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = outgoing_controller_type::serialize_report(data, length);
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum
        , std::vector<char> && data)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = outgoing_controller_type::serialize_report(std::move(data));
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

    /**
     * Incoming packet handler.
     *
     * @details Must be called when data received by underlying transport (e.g. from transport
     *          callback for handle incoming data)
     */
    void process_packet (address_type sender_addr, int /*priority*/, std::vector<char> && data)
    {
        PFS__ASSERT(data.size() > 0, "");

        // For sending SYN and ACK packets
        auto on_send = [this, sender_addr] (std::vector<char> && data) { // On send
            // See start_syn()
            int priority = 0; // High priority
            bool force_checksum = true; // Need checksum

            // Enqueue result is not important. In the worst case, the request will
            // be repeated.
            /*auto success = */
            _transport->enqueue(sender_addr, priority, force_checksum, std::move(data));
        };

        auto on_ready = [this, sender_addr] () { // On ready
            auto outc = ensure_outgoing_controller(sender_addr);
            outc->set_synchronized(true);
            this->on_receiver_ready(sender_addr);
        };

        auto on_acknowledged = [this, sender_addr] (serial_number sn) {
            auto outc = ensure_outgoing_controller(sender_addr);
            outc->acknowledge(sn);
        };

        auto on_again = [this, sender_addr] (serial_number sn) {
            auto outc = ensure_outgoing_controller(sender_addr);
            outc->again(sn);
        };

        auto on_report_received = [this, sender_addr] (std::vector<char> && report) {
            this->on_report_received(sender_addr, std::move(report));
        };

        auto inpc = ensure_incoming_controller(sender_addr);
        inpc->process_packet(std::move(data), std::move(on_send), std::move(on_ready)
            , std::move(on_acknowledged), std::move(on_again), std::move(on_report_received));
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        unsigned int n = 0;

        for (auto & x: _ocontrollers) {
            auto & addr = x.first;
            auto & outc = x.second;

            auto on_send = [this, addr] (int priority, bool force_checksum, std::vector<char> && data) {
                // Enqueue result is not important. In the worst case, data will be retransmitted.
                /*auto success = */
                _transport->enqueue(addr, priority, force_checksum, std::move(data));
            };

            auto on_delivered = [this, addr] (message_id msgid) {
                this->on_message_delivered(addr, msgid);
            };

            n += outc.step(std::move(on_send), std::move(on_delivered));
        }

        for (auto & x: _icontrollers) {
            auto & addr = x.first;
            auto & inpc = x.second;

            auto on_message_received = [this, addr] (message_id msgid, std::vector<char> && msg) {
                this->on_message_received(addr, msgid, std::move(msg));
            };

            n += inpc.step(std::move(on_message_received));
        }

        n += _transport->step();

        return n;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
