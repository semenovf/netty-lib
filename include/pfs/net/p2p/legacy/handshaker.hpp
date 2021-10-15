////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "endpoint.hpp"

#include "pfs/fmt.hpp"

namespace pfs {
namespace net {
namespace p2p {

template <typename SharedEndpoint, typename OutputEnvelope, typename InputEnvelope>
class handshaker
{
    enum handshake_phase: std::uint8_t {
          syn_phase = 42
        , synack_phase
        , ack_phase
    };

    enum {
          handshake_complete_flag
        , handshake_failure_flag
        , finish_flag
    };

    using seq_number_type      = std::uint32_t;
    using shared_endpoint_type = SharedEndpoint;
    using output_envelope_type = OutputEnvelope;
    using input_envelope_type  = InputEnvelope;

private:
    seq_number_type _self_sn{0};
    seq_number_type _peer_sn{0};

public: // signals
    pfs::emitter_mt<shared_endpoint_type> handshake_complete;
    pfs::emitter_mt<shared_endpoint_type, std::string const &> failure;

private:
    bool send (shared_endpoint_type & ep, output_envelope_type const & envelope)
    {
        auto bytes_sent = ep->send(envelope.data(), envelope.data().size());

        if (bytes_sent < 0) {
            failure(ep, "Sending handshake data failure");
            return false;
        }

        return true;
    }

public:
    handshaker () = default;

    // ep - outgoing connection
    void start_handshake (shared_endpoint_type ep)
    {
        typename std::underlying_type<handshake_phase>::type phase = syn_phase;
        output_envelope_type envlp;

        envlp << phase << _self_sn << ep->uuid() << seal;

        if (send(ep, envlp)) {
            fmt::print("{} ------ SYN({}) -------> ?\n"
                , std::to_string(ep->uuid())
                , _self_sn);
        }
    }

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

    void process_input (shared_endpoint_type ep, input_envelope_type & envelope)
    {
        typename std::underlying_type<handshake_phase>::type phase;
        envelope >> phase;

        switch (phase) {
            case syn_phase: {
                seq_number_type syn;
                uuid_t peer_uuid;
                envelope >> syn >> peer_uuid >> p2p::unseal;

                if (envelope.success()) {
                    output_envelope_type envlp;
                    _peer_sn = syn;
                    ep->set_peer_uuid(peer_uuid);
                    phase = synack_phase;

                    envlp << phase << _self_sn << ++_peer_sn << p2p::seal;

                    if (send(ep, envlp)) {
                        fmt::print("{} <--- SYN({}) ACK({}) --- {}\n"
                            , std::to_string(ep->uuid())
                            , _self_sn
                            , _peer_sn
                            , std::to_string(ep->peer_uuid()));
                    }
                } else {
                    handshake_failure(*this, "Bad SYN packet");
                }

                break;
            }

            case synack_phase: {
                seq_number_type syn;
                seq_number_type ack;

                envelope >> syn >> ack >> p2p::unseal;

                if (envelope.success()) {
                    if (ack == _self_sn + 1) {
                        output_envelope_type envlp;
                        _peer_sn = syn;
                        _self_sn = ack;

                        phase = ack_phase;
                        envlp << phase << ++_peer_sn << p2p::seal;

                        if (send(ep, envlp)) {
                            fmt::print("{} ------ ACK({}) -------> {}\n"
                                , std::to_string(ep->uuid())
                                , _peer_sn
                                , std::to_string(ep->peer_uuid()));
                        }
                    } else {
                        handshake_failure(*this, "Bad SYN-ACK packet: unexpected ACK sequence number");
                    }
                } else {
                    handshake_failure(*this, "Bad SYN-ACK packet");
                }

                break;
            }

            case ack_phase: {
                seq_number_type ack;

                envelope >> ack >> p2p::unseal;

                if (envelope.success()) {
                    if (ack == _self_sn) {
                        output_envelope_type envlp;
                        _self_sn = ack;

                        handshake_complete(ep);
                    } else {
                        handshake_failure(ep, "Bad ACK packet: unexpected ACK sequence number");
                    }
                } else {
                    handshake_failure(*this, "Bad ACK packet");
                }

                break;
            }

            default:
                failure(ep, fmt::format("Bad handshake phase: {}", phase));
                break;
        }
    }
};

}}} // namespace pfs::net::p2p
