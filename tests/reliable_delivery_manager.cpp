////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <pfs/assert.hpp>
#include <pfs/log.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/patterns/serializer_traits.hpp>
#include <pfs/netty/patterns/reliable_delivery/in_memory_processor.hpp>
#include <pfs/netty/patterns/reliable_delivery/manager.hpp>
#include <pfs/netty/patterns/reliable_delivery/protocol.hpp>
#include <atomic>
#include <chrono>
#include <queue>
#include <thread>
#include <vector>

static constexpr char const * TAG  = "---";
static constexpr char const * RTAG = "RCV";
static constexpr char const * STAG = "SND";

std::atomic_bool s_quit_flag {false};

class delivery_engine
{
    pfs::synchronized<std::queue<std::vector<char>>> _q;

public:
    delivery_engine ()
    {}

public:
    void send (std::vector<char> && msg)
    {
        _q.wlock()->push(std::move(msg));
    }

    bool received (std::vector<char> & msg)
    {
        if (_q.rlock()->empty())
            return false;

        msg = _q.rlock()->front();
        _q.wlock()->pop();
        return true;
    }
};

class callbacks
{
    delivery_engine * _de {nullptr};

public:
    callbacks (delivery_engine & de)
        : _de(& de)
    {}

public:
    void on_payload (std::vector<char> && payload)
    {
        auto text = std::string(payload.data(), payload.size());

        // LOGD(RTAG, "Payload received: {}", text);

        if (text == ".quit")
            s_quit_flag = true;
    }

    void on_report (std::vector<char> && payload)
    {
        auto text = std::string(payload.data(), payload.size());

        // LOGD(RTAG, "Report received: {}", text);

        if (text == ".quit")
            s_quit_flag = true;
    }

    void dispatch (std::vector<char> && msg)
    {
        _de->send(std::move(msg));
    }
};

using income_processor_t = netty::patterns::reliable_delivery::im_income_processor;
using outcome_processor_t = netty::patterns::reliable_delivery::im_outcome_processor;
using reliable_delivery_manager_t = netty::patterns::reliable_delivery::manager<
      income_processor_t
    , outcome_processor_t
    , netty::patterns::serializer_traits_t
    , callbacks>;

class endpoint
{
    delivery_engine * _in {nullptr};
    delivery_engine * _out {nullptr};

    income_processor_t _inproc {0};
    outcome_processor_t _outproc {0, 0};
    reliable_delivery_manager_t _dm;

public:
    explicit endpoint (std::string name, delivery_engine & in, delivery_engine & out)
        : _in(& in)
        , _out(& out)
        , _dm(std::move(name), _inproc, _outproc, callbacks{out})
    {}

    endpoint (endpoint const & other) = delete;
    endpoint (endpoint && other) = delete;
    endpoint & operator = (endpoint const & other) = delete;
    endpoint & operator = (endpoint && other) = delete;

private:
    void send_payload (std::string const & msg)
    {
        auto data = _dm.payload(msg.data(), msg.size());
        _out->send(std::move(data));
    }

    void send_report (std::string const & msg)
    {
        auto data = _dm.report(msg.data(), msg.size());
        _out->send(std::move(data));
    }

    void emulate_payload_loss (std::string const & msg)
    {
        auto data = _dm.payload(msg.data(), msg.size());

        // Do not send
    }

    void check_and_process_received ()
    {
        std::vector<char> msg;

        while (_in->received(msg))
            _dm.process_packet(std::move(msg));
    }

public:
    void run_receiver ()
    {
        while (!s_quit_flag) {
            check_and_process_received();
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

    void run_sender ()
    {
        lorem::lorem_ipsum para_generator;
        para_generator.set_paragraph_count(5);
        para_generator.set_sentence_count(1);
        para_generator.set_word_range(10, 20);
        auto random_texts = para_generator();

        for (auto const & x: random_texts) {
            send_payload(x[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }

        check_and_process_received();

        // Emulate payload loss
        para_generator.set_paragraph_count(2);
        random_texts = para_generator();

        for (auto const & x: random_texts) {
            emulate_payload_loss(x[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }

        check_and_process_received();

        // Send next payloads
        para_generator.set_paragraph_count(2);
        random_texts = para_generator();

        for (auto const & x: random_texts) {
            send_payload(x[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }

        check_and_process_received();

        send_report(".quit");

        check_and_process_received();

        while (_dm.has_waiting())
            _dm.step();
    }
};

TEST_CASE("reliable delivery") {
    delivery_engine pipe1;
    delivery_engine pipe2;

    std::thread receiver_thread { [& pipe1, & pipe2] () {
        endpoint worker {"A", pipe1, pipe2};
        worker.run_receiver();
    }};

    std::thread sender_thread { [& pipe1, & pipe2] () {
        endpoint worker {"B", pipe2, pipe1};
        worker.run_sender();
    }};

    receiver_thread.join();
    sender_thread.join();
}
