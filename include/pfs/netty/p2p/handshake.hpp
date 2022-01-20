////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
// #include "envelope.hpp"
// #include "hello_packet.hpp"
// #include "packet.hpp"
// #include "timing.hpp"
// #include "trace.hpp"
// #include "pfs/ring_buffer.hpp"
// #include "pfs/uuid.hpp"
// #include "pfs/emitter.hpp"
// #include "pfs/netty/inet4_addr.hpp"
// #include <unordered_map>
// #include <utility>
// #include <cassert>

namespace netty {
namespace p2p {

// Three-Way Handshake Process
//
//   client                           server
//    ---                              ---
//     |            SYN=N0              |
//     |------------------------------->| (1)
//     |                                |
//     |        SYN=N1 ACK=N0+1         |
//     |<-------------------------------| (2)
//     |                                |
//     |            ACK=N1+1            |
//     |------------------------------->| (3)
//     |                                |
//
// (1) `client` begins the connection by sending the SYN packet. The packets
//     contain a random sequence number that indicates the beginning of the
//     sequence numbers for data that the `client` should transmit.
// (2) After that, the `server` will receive the packet, and it responds with
//     its sequence number. It’s response also includes the acknowledgment
//     number, that is `client`’s sequence number incremented with 1.
// (3) `client` responds to the `server` by sending the acknowledgment number
//     that is mostly `server`’s sequence number that is incremented by 1.

class handshake
{
public:
    void process_handshake (input_envelope_t & envelope)
    {
//         std::underlying_type<handshake_phase>::type phase;
//         envelope >> phase;
//
//         switch (phase) {
//             case syn_phase: {
//                 seq_number_t syn;
//                 uuid_t uuid;
//                 envelope >> syn >> uuid >> p2p::unseal;
//
//                 if (envelope.success()) {
//                     output_envelope_t envlp;
//                     _peer_sn = syn;
//                     _peer_uuid = uuid;
//                     phase = synack_phase;
//
//                     envlp << phase << _self_sn << ++_peer_sn << p2p::seal;
//
//                     if (send_handshake_data(envlp)) {
//                         fmt::print("{} <--- SYN({}) ACK({}) --- {}\n"
//                             , std::to_string(_self_uuid)
//                             , _self_sn
//                             , _peer_sn
//                             , std::to_string(_peer_uuid));
//                     }
//                 } else {
//                     set_status(handshake_failure_flag);
//                     handshake_failure(*this, "Bad SYN packet");
//                 }
//
//                 ++COUNTER;
//                 break;
//             }
//
//             case synack_phase: {
//                 seq_number_t syn;
//                 seq_number_t ack;
//
//                 envelope >> syn >> ack >> p2p::unseal;
//
//                 if (envelope.success()) {
//                     if (ack == _self_sn + 1) {
//                         output_envelope_t envlp;
//                         _peer_sn = syn;
//                         _self_sn = ack;
//
//                         phase = ack_phase;
//                         envlp << phase << ++_peer_sn << p2p::seal;
//
//                         if (send_handshake_data(envlp)) {
//                             fmt::print("{} ------ ACK({}) -------> {}\n"
//                                 , std::to_string(_self_uuid)
//                                 , _peer_sn
//                                 , std::to_string(_peer_uuid));
//                         }
//
//                     } else {
//                         set_status(handshake_failure_flag);
//                         handshake_failure(*this, "Bad SYN-ACK packet: unexpected ACK sequence number");
//                     }
//                 } else {
//                     set_status(handshake_failure_flag);
//                     handshake_failure(*this, "Bad SYN-ACK packet");
//                 }
//
//                 ++COUNTER;
//                 break;
//             }
//
//             case ack_phase: {
//                 seq_number_t ack;
//
//                 envelope >> ack >> p2p::unseal;
//
//                 if (envelope.success()) {
//                     if (ack == _self_sn) {
//                         output_envelope_t envlp;
//                         _self_sn = ack;
//
//                         // Change input callback
//                         _input_callback = & Channel::process_default;
//
//                         set_status(handshake_complete_flag);
//                         handshake_complete(*this, _peer_uuid);
//                     } else {
//                         set_status(handshake_failure_flag);
//                         handshake_failure(*this, "Bad ACK packet: unexpected ACK sequence number");
//                     }
//                 } else {
//                     set_status(handshake_failure_flag);
//                     handshake_failure(*this, "Bad ACK packet");
//                 }
//
//                 ++COUNTER;
//                 break;
//             }
//
//             default:
//                 set_status(handshake_failure_flag);
//                 handshake_failure(*this, fmt::format("Bad handshake phase: {}", phase));
//                 break;
//         }
    }
};

}} // namespace netty::p2p

