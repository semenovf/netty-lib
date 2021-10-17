////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/fmt.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/handshake_packet.hpp"
#include "pfs/net/p2p/utils.hpp"

#if defined(QT5_NETWORK_ENABLED)
#   include "pfs/net/p2p/qt5/udp_reader.hpp"
#   include "pfs/net/p2p/qt5/udp_writer.hpp"
#else
#   error "No other implementation yet"
#endif

#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace p2p = pfs::net::p2p;
using inet4_addr_t = pfs::net::inet4_addr;
static inet4_addr_t PEER1_ADDR {127,0,0,1};
static inet4_addr_t PEER2_ADDR {127,0,0,1};

static constexpr std::size_t PACKET_SIZE = p2p::CALCULATE_PACKET_SIZE(32);

static std::uint16_t PEER1_PORT = 4242;
static std::uint16_t PEER2_PORT = 4243;

using uuid_t = pfs::uuid_t;
using output_envelope_t = p2p::output_envelope<>;
using input_envelope_t = p2p::input_envelope<>;

#if defined(QT5_NETWORK_ENABLED)
using udp_reader = p2p::qt5::udp_reader;
using udp_writer = p2p::qt5::udp_writer;
#endif

template <typename Key, typename T>
class unordered_map_mt: protected std::unordered_map<Key, T>
{
    using base_class = std::unordered_map<Key, T>;
    mutable std::mutex _mtx;

public:
    using base_class::base_class;

    template <typename K, typename U>
    void store (K && key, U && data)
    {
        std::unique_lock<std::mutex> lk{_mtx};
        auto & item = base_class::operator [] (std::forward<K>(key));
        item = std::forward<U>(data);
    }

//     void store (Key && key, T && data)
//     {
//         std::unique_lock<std::mutex> lk{_mtx};
//         auto & item = base_class::operator [] (key);
//         item = data;
//     }

    void store_and_process (Key && key
        , T && data
        , std::function<void (T &)> && proc)
    {
        std::unique_lock<std::mutex> lk{_mtx};
        auto & item = base_class::operator [] (key);
        item = data;
        proc(item);
    }
};

struct peer_item
{
    inet4_addr_t  addr;
    std::uint16_t port {0};
    p2p::seqnum_t self_sn{0};
    p2p::seqnum_t peer_sn{0};
};

class peer
{
public:
    uuid_t uuid;
    udp_reader reader;
    udp_writer writer;

private:
    unordered_map_mt<uuid_t, peer_item> _peers;

public:
//     pfs::emitter_mt<peer &, uuid_t> handshake_complete;
    pfs::emitter_mt<std::string const &> handshake_failure;
//     pfs::emitter_mt<peer &, std::string const &> failure;

private:
    void process_handshake (p2p::handshake_packet const & pkt)
    {
//         auto peer_pos = _peers.find(pkt.uuid);
//
//         if (peer_pos != _peers.end()) {
//
//         } else {
//             fmt::print(stderr, "peer not found: {}\n", pkt.uuid);
//         }
    }

    void process_handshake_input (char const * data, std::streamsize size)
    {
        if (size != p2p::handshake_packet::PACKET_SIZE) {
            handshake_failure(fmt::format("bad packet size: {}, expected {}\n"
                , size, p2p::handshake_packet::PACKET_SIZE));
            return;
        }

        input_envelope_t in {data, size};
        p2p::handshake_packet pkt;

        auto result = in.unseal(pkt);

        if (!result.first) {
            handshake_failure(fmt::format("bad handshake packet: {}"
                , result.second));
            return;
        }

        process_handshake(pkt);
    }

public:
    void start_handshake (uuid_t peer_uuid
        , inet4_addr_t const & peer_addr
        , std::uint16_t peer_port)
    {
        reader.datagram_received.disconnect_all();
        reader.datagram_received.connect(*this, & peer::process_handshake_input);

        peer_item item {peer_addr, peer_port, 0, 0};
        _peers.store(peer_uuid, std::move(item));

        p2p::handshake_packet pkt;
        pkt.phase = p2p::syn_phase;
        pkt.sn = 0;
        pkt.uuid = uuid;

        output_envelope_t oe;
        oe.seal(pkt);

        auto bytes = oe.data();
        auto bytes_written = writer.write(peer_addr, peer_port
            , bytes.data(), bytes.size());

        if (bytes_written < 0) {
            handshake_failure(fmt::format("sending handshake data failure with: {} ({}:{})"
                , std::to_string(peer_uuid)
                , std::to_string(peer_addr)
                , peer_port));
            return;
        }

        // TRACE
        TRACE_1("{} ------ SYN({}) -------> {}\n"
            , std::to_string(uuid)
            , pkt.sn
            , std::to_string(pkt.uuid));
    }

};

void worker (peer & p)
{
    fmt::print("Peer started: {}\n", std::to_string(p.uuid));
//     client.process_input();
    fmt::print("Peer finished: {}\n", std::to_string(p.uuid));
}

auto handshake_failure_callback = [] (std::string const & s) {
    fmt::print(stderr, "ERROR: {}\n", s);
};

TEST_CASE("handshake")
{
    peer p1;
    peer p2;

    {
        p1.uuid = pfs::from_string<uuid_t>("01FH7H6YJB8XK9XNNZYR0WYDJ1");
        udp_reader::options opts;
        opts.listener_addr4 = PEER1_ADDR;
        opts.listener_port = PEER1_PORT;

        auto success = p1.reader.set_options(std::move(opts));

        REQUIRE(success);

        p1.handshake_failure.connect(handshake_failure_callback);
    }

    {
        p2.uuid = pfs::from_string<uuid_t>("01FH7HB19B9T1CTKE5AXPTN74M");
        udp_reader::options opts;
        opts.listener_addr4 = PEER2_ADDR;
        opts.listener_port = PEER2_PORT;

        auto success = p2.reader.set_options(std::move(opts));

        REQUIRE(success);

        p2.handshake_failure.connect(handshake_failure_callback);
    }

    std::thread peer1_worker;
    std::thread peer2_worker;

    peer1_worker = std::thread{worker, std::ref(p1)};
    peer2_worker = std::thread{worker, std::ref(p2)};

    p1.start_handshake(p2.uuid, PEER2_ADDR, PEER2_PORT);
    p2.start_handshake(p1.uuid, PEER1_ADDR, PEER1_PORT);

    peer1_worker.join();
    peer2_worker.join();
}

