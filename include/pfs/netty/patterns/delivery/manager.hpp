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
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
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
    , typename MessageIdTraits
    , typename IncomingProcessor
    , typename OutgoingProcessor
    , typename WriterMutex
    , typename CallbackSuite>
class manager
{
    using transport_type = Transport;
    using incoming_processor_type = IncomingProcessor;
    using outgoing_processor_type = OutgoingProcessor;

public:
    using address_type = typename transport_type::address_type;
    using message_id_traits = MessageIdTraits;
    using message_id = typename message_id_traits::type;
    using writer_mutex_type = WriterMutex;
    using callback_suite = CallbackSuite;

private:
    transport_type * _transport {nullptr};

    // Processors to handle incoming messages.
    std::unordered_map<address_type, incoming_processor_type> _inprocs;

    // Processors to track outgoing messages.
    std::unordered_map<address_type, outgoing_processor_type> _outprocs;

    callback_suite _callbacks;

    std::atomic_bool _interrupted {false};

    writer_mutex_type _writer_mtx;

public:
    manager (transport_type & transport, callback_suite && callbacks)
        : _transport(& transport)
        , _callbacks(std::move(callbacks))
    {}

    manager (manager const &) = delete;
    manager (manager &&) = delete;
    manager & operator = (manager const &) = delete;
    manager & operator = (manager &&) = delete;

    ~manager () {}

private:
    incoming_processor_type * ensure_inproc (address_type addr)
    {
        PFS__ASSERT(_transport != nullptr, "");

        auto pos = _inprocs.find(addr);

        if (pos != _inprocs.end())
            return & pos->second;

        auto res = _inprocs.emplace(addr, incoming_processor_type{});

        PFS__ASSERT(res.second, "");

        return & res.first->second;
    }

    outgoing_processor_type * ensure_outproc (address_type addr)
    {
        auto pos = _outprocs.find(addr);

        if (pos != _outprocs.end())
            return & pos->second;

        auto res = _outprocs.insert({addr, outgoing_processor_type{}});

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

        auto outproc = ensure_outproc(addr);
        return outproc->enqueue_message(msgid, priority, force_checksum, std::move(msg));
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

        auto outproc = ensure_outproc(addr);
        return outproc->enqueue_static_message(msgid, priority, force_checksum, msg, length);
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum, char const * data
        , std::size_t length)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = outgoing_processor_type::serialize_report(data, length);
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

    bool enqueue_report (address_type addr, int priority, bool force_checksum
        , std::vector<char> && data)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        if (!_transport->is_reachable(addr))
            return false;

        auto report = outgoing_processor_type::serialize_report(std::move(data));
        return _transport->enqueue(addr, priority, force_checksum, std::move(report));
    }

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
            auto outproc = ensure_outproc(sender_addr);
            outproc->set_synchronized(true);
            _callbacks.on_receiver_ready(sender_addr);
        };

        auto on_acknowledged = [this, sender_addr] (serial_number sn) {
            auto outproc = ensure_outproc(sender_addr);
            outproc->acknowledge(sn);
        };

        auto on_again = [this, sender_addr] (serial_number sn) {
            auto outproc = ensure_outproc(sender_addr);
            outproc->again(sn);
        };

        auto on_report_received = [this, sender_addr] (std::vector<char> && report) {
            _callbacks.on_report_received(sender_addr, std::move(report));
        };

        auto inproc = ensure_inproc(sender_addr);
        inproc->process_packet(std::move(data), std::move(on_send), std::move(on_ready)
            , std::move(on_acknowledged), std::move(on_again), std::move(on_report_received));
    }

    void interrupt ()
    {
        _interrupted = true;
    }

    bool interrupted () const noexcept
    {
        return _interrupted.load();
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        unsigned int n = 0;

        for (auto & x: _outprocs) {
            auto & addr = x.first;
            auto & outproc = x.second;

            auto on_send = [this, addr] (int priority, bool force_checksum, std::vector<char> && data) {
                // Enqueue result is not important. In the worst case, data will be retransmitted.
                /*auto success = */
                _transport->enqueue(addr, priority, force_checksum, std::move(data));
            };

            auto on_dispatched = [this, addr] (message_id msgid) {
                _callbacks.on_message_dispatched(addr, msgid);
            };

            n += outproc.step(std::move(on_send), std::move(on_dispatched));
        }

        for (auto & x: _inprocs) {
            auto & addr = x.first;
            auto & inporoc = x.second;

            auto on_message_received = [this, addr] (message_id msgid, std::vector<char> && msg) {
                _callbacks.on_message_received(addr, msgid, std::move(msg));
            };

            n += inporoc.step(std::move(on_message_received));
        }

        n += _transport->step();

        return n;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        _interrupted.store(false);

        while (!_interrupted) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
