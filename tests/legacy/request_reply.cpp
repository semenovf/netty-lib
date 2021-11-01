////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "command/hello.hpp"
#include "command/fin.hpp"
#include "pfs/emitter.hpp"
#include "pfs/fmt.hpp"
#include "pfs/uuid.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/net/p2p/legacy/envelope.hpp"
#include "pfs/net/p2p/hello_packet.hpp"
#include <cereal/types/string.hpp>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>

using namespace pfs::net;
using seq_number_t      = std::uint32_t;
using uuid_t            = pfs::uuid_t;
using ring_buffer_t     = pfs::ring_buffer_mt<std::string, 32>;
using output_envelope_t = p2p::output_envelope<>;
using input_envelope_t  = p2p::input_envelope<>;

static std::atomic_int COUNTER{0};

enum class message_type_enum: std::uint8_t
{
      request
    , reply
};

enum class RequestId: std::uint16_t
{
      INITIAL
    , FIN
    , HELLO
};

enum class ReplyCode: std::int8_t
{
      INITIAL
    , OK
    , ERROR
};

template <typename Command>
RequestId request_id ();

template <>
inline RequestId request_id<Fin> () { return RequestId::FIN; }

template <>
inline RequestId request_id<Hello> () { return RequestId::HELLO; }

class Channel;

class RequestHeader
{
    friend class Channel;

    seq_number_t _sn {0};
    RequestId _rqid {RequestId::INITIAL};

private:
    RequestHeader () = default;

public:
    RequestHeader (seq_number_t sn, RequestId rqid)
        : _sn(sn)
        , _rqid(rqid)
    {}

    template <typename Archive>
    void save (Archive & ar) const
    {
        ar(_sn, _rqid);
    }

    template <typename Archive>
    void load (Archive & ar)
    {
        ar(_sn, _rqid);
    }

    int32_t crc32 (std::int32_t initial = 0) const
    {
        return pfs::crc32_all_of(initial
            , _sn
            , static_cast<std::underlying_type<RequestId>::type>(_rqid));
    }
};

class ReplyHeader
{
    seq_number_t _sn {0};
    RequestId _rqid {RequestId::INITIAL};
    ReplyCode _code {ReplyCode::INITIAL};

private:
    ReplyHeader () = default;

public:
    ReplyHeader (seq_number_t sn, RequestId rqid, ReplyCode code)
        : _sn(sn)
        , _rqid(rqid)
        , _code(code)
    {}

    int32_t crc32 (std::int32_t initial = 0) const
    {
        return pfs::crc32_all_of(initial
            , _sn
            , static_cast<std::underlying_type<RequestId>::type>(_rqid)
            , static_cast<std::underlying_type<ReplyCode>::type>(_code));
    }
};

class Channel
{
    using input_callback_type = void (Channel::*)(input_envelope_t &);

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

    uuid_t _self_uuid {};
    uuid_t _peer_uuid {};

    ring_buffer_t * _out {nullptr};
    ring_buffer_t * _in {nullptr};

    input_callback_type _input_callback {& Channel::process_handshake};

    seq_number_t _self_sn{0};
    seq_number_t _peer_sn{0};

    mutable std::mutex _status_mtx;
    std::bitset<8> _status;

public:
    pfs::emitter_mt<Channel &, uuid_t> handshake_complete;
    pfs::emitter_mt<Channel &, std::string const &> handshake_failure;
    pfs::emitter_mt<Channel &, std::string const &> failure;

private:
    inline void set_status (std::size_t pos)
    {
        std::unique_lock<std::mutex> locker(_status_mtx);
        _status.set(pos);
    }

    inline bool get_status (std::size_t pos) const
    {
        std::unique_lock<std::mutex> locker(_status_mtx);
        return _status.test(pos);
    }

    inline bool send (output_envelope_t const & envelope)
    {
        _out->push(envelope.data());
        return true;
    }

    inline bool send_handshake_data (output_envelope_t const & envelope)
    {
        if (!send(envelope)) {
            set_status(handshake_failure_flag);
            handshake_failure(*this, "Sending handshake data failure");
            return false;
        }

        return true;
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

    void process_handshake (input_envelope_t & envelope)
    {
        std::underlying_type<handshake_phase>::type phase;
        envelope >> phase;

        switch (phase) {
            case syn_phase: {
                seq_number_t syn;
                uuid_t uuid;
                envelope >> syn >> uuid >> p2p::unseal;

                if (envelope.success()) {
                    output_envelope_t envlp;
                    _peer_sn = syn;
                    _peer_uuid = uuid;
                    phase = synack_phase;

                    envlp << phase << _self_sn << ++_peer_sn << p2p::seal;

                    if (send_handshake_data(envlp)) {
                        fmt::print("{} <--- SYN({}) ACK({}) --- {}\n"
                            , std::to_string(_self_uuid)
                            , _self_sn
                            , _peer_sn
                            , std::to_string(_peer_uuid));
                    }
                } else {
                    set_status(handshake_failure_flag);
                    handshake_failure(*this, "Bad SYN packet");
                }

                ++COUNTER;
                break;
            }

            case synack_phase: {
                seq_number_t syn;
                seq_number_t ack;

                envelope >> syn >> ack >> p2p::unseal;

                if (envelope.success()) {
                    if (ack == _self_sn + 1) {
                        output_envelope_t envlp;
                        _peer_sn = syn;
                        _self_sn = ack;

                        phase = ack_phase;
                        envlp << phase << ++_peer_sn << p2p::seal;

                        if (send_handshake_data(envlp)) {
                            fmt::print("{} ------ ACK({}) -------> {}\n"
                                , std::to_string(_self_uuid)
                                , _peer_sn
                                , std::to_string(_peer_uuid));
                        }

                    } else {
                        set_status(handshake_failure_flag);
                        handshake_failure(*this, "Bad SYN-ACK packet: unexpected ACK sequence number");
                    }
                } else {
                    set_status(handshake_failure_flag);
                    handshake_failure(*this, "Bad SYN-ACK packet");
                }

                ++COUNTER;
                break;
            }

            case ack_phase: {
                seq_number_t ack;

                envelope >> ack >> p2p::unseal;

                if (envelope.success()) {
                    if (ack == _self_sn) {
                        _self_sn = ack;

                        // Change input callback
                        _input_callback = & Channel::process_default;

                        set_status(handshake_complete_flag);
                        handshake_complete(*this, _peer_uuid);
                    } else {
                        set_status(handshake_failure_flag);
                        handshake_failure(*this, "Bad ACK packet: unexpected ACK sequence number");
                    }
                } else {
                    set_status(handshake_failure_flag);
                    handshake_failure(*this, "Bad ACK packet");
                }

                ++COUNTER;
                break;
            }

            default:
                set_status(handshake_failure_flag);
                handshake_failure(*this, fmt::format("Bad handshake phase: {}", phase));
                break;
        }
    }

    enum: std::underlying_type<message_type_enum>::type {
          request_message = static_cast<std::underlying_type<message_type_enum>::type>(message_type_enum::request)
        , reply_message = static_cast<std::underlying_type<message_type_enum>::type>(message_type_enum::reply)
    };

    void process_default (input_envelope_t & envelope)
    {
        std::underlying_type<message_type_enum>::type message_type;
        envelope >> message_type;

        switch (message_type) {
            case request_message:
                process_request(envelope);
                break;

            case reply_message:
                process_reply(envelope);
                break;

            default:
                failure(*this, fmt::format("Bad message type: {}\n", message_type));
                break;
        }

        ++COUNTER;
    }

    void process_request (input_envelope_t & envelope)
    {
        RequestHeader header;
        envelope >> header;

        switch (header._rqid) {
            case RequestId::INITIAL:
                break;

            case RequestId::FIN: {
                Fin fin;
                envelope >> fin >> p2p::unseal;

                if (envelope.success())
                    finish();
                else
                    failure(*this, "Bad data for FIN command");

                break;
            }

            case RequestId::HELLO: {
                Hello hello;
                envelope >> hello >> p2p::unseal;

                if (envelope.success()) {
                    fmt::print("{}: Hello command: {}\n"
                        , std::to_string(_self_uuid)
                        , hello.text);
                } else {
                    failure(*this, "Bad data for HELLO command");
                }

                break;
            }

            default:
                // TODO Unsupported command
                break;
        }
    }

    void process_reply (input_envelope_t & envelope)
    {}

public:
    Channel (ring_buffer_t & out, ring_buffer_t & in)
        : _out(& out)
        , _in(& in)
    {}

    ~Channel () = default;

    void finish ()
    {
        ++COUNTER;
        set_status(finish_flag);
    }

    bool finished () const noexcept
    {
        return get_status(finish_flag);
    }

    void start_handshake (uuid_t self_uuid)
    {
        _self_uuid = self_uuid;
        std::underlying_type<handshake_phase>::type phase = syn_phase;
        output_envelope_t envlp;

        envlp << phase << _self_sn << self_uuid << p2p::seal;

        if (send_handshake_data(envlp)) {
            fmt::print("{} ------ SYN({}) -------> ?\n"
                , std::to_string(_self_uuid)
                , _self_sn);
        }

        ++COUNTER;
    }

    void process_input ()
    {
        std::string packet;

        if (_in->try_pop(packet)) {
            p2p::input_envelope<> envelope {packet};
            (this->*_input_callback)(envelope);
        }
    }

    template <typename Command>
    bool send_command (Command const & cmd)
    {
        output_envelope_t envelope;
        envelope << static_cast<std::underlying_type<message_type_enum>::type>(message_type_enum::request)
            << RequestHeader{++_self_sn, request_id<Command>()}
            << cmd
            << p2p::seal;

        return send(envelope);
    }
};

class Client
{
    uuid_t _uuid;
    std::list<Channel *> _channels;

public:
    template <typename ...Channels>
    Client (std::string const & uuid, Channels &... channels)
        : _channels{& channels...}
    {
        _uuid = pfs::from_string<uuid_t>(uuid);
    }

    uuid_t uuid () const noexcept
    {
        return _uuid;
    }

    void start_handshake ()
    {
        for (auto pchannel: _channels)
            pchannel->start_handshake(_uuid);
    }

    void process_input ()
    {
        while (!_channels.empty()) {
            for (auto it = _channels.begin(); it != _channels.end();) {
                if ((*it)->finished()) {
                    it = _channels.erase(it);
                } else {
                    (*it)->process_input();
                    ++it;
                }
            }
        }
    }
};

static ring_buffer_t out_1_2;
static ring_buffer_t in_1_2;
static ring_buffer_t out_1_3;
static ring_buffer_t in_1_3;
static ring_buffer_t out_2_3;
static ring_buffer_t in_2_3;

static Channel channel_1_2 { out_1_2,  in_1_2 };
static Channel channel_1_3 { out_1_3,  in_1_3 };
static Channel channel_2_3 { out_2_3,  in_2_3 };
static Channel channel_2_1 { in_1_2 , out_1_2 };
static Channel channel_3_1 { in_1_3 , out_1_3 };
static Channel channel_3_2 { in_2_3 , out_2_3 };

std::atomic_bool quit {false};

void worker (Client & client)
{
    fmt::print("Client started: {}\n", std::to_string(client.uuid()));
    client.start_handshake();
    client.process_input();
    fmt::print("Client finished: {}\n", std::to_string(client.uuid()));
}

TEST_CASE("Request-Reply") {

    auto handshake_complete_callback = [] (Channel & chan, uuid_t peer_uuid) {
        fmt::print("Handshake complete with: {}\n", std::to_string(peer_uuid));
        chan.send_command(Hello{"World!"});
        chan.send_command(Fin{});
        //chan.finish();
    };

    auto handshake_failure_callback = [] (Channel & chan, std::string const & s) {
        fmt::print(stderr, "{}\n", s);
        chan.finish();
        //chan.send_command(Fin{});
    };

    auto failure_callback = [] (Channel & chan, std::string const & s) {
        fmt::print(stderr, "{}\n", s);
        chan.finish();
    };

    for (auto pchan: {& channel_1_2, & channel_1_3, & channel_2_3
            , & channel_2_1, & channel_3_1, & channel_3_2}) {
        pchan->handshake_complete.connect(handshake_complete_callback);
        pchan->handshake_failure.connect(handshake_failure_callback);
        pchan->failure.connect(failure_callback);
    }

    std::thread worker1;
    std::thread worker2;
    std::thread worker3;

    Client client1("01FH7H6YJB8XK9XNNZYR0WYDJ1", channel_1_2, channel_1_3);
    Client client2{"01FH7HB19B9T1CTKE5AXPTN74M", channel_2_1, channel_2_3};
    Client client3{"01FH7HBC13YX4VS4DKVWCZEKV4", channel_3_1, channel_3_2};
//     Client client1("01FH7H6YJB8XK9XNNZYR0WYDJ1", channel_1_2);
//     Client client2{"01FH7HB19B9T1CTKE5AXPTN74M", channel_2_1};

    worker1 = std::thread{worker, std::ref(client1)};
    worker2 = std::thread{worker, std::ref(client2)};
    worker3 = std::thread{worker, std::ref(client3)};

    worker1.join();
    worker2.join();
    worker3.join();

    CHECK_EQ(COUNTER, 42);
}

namespace pfs {
template <>
inline int32_t crc32_of<RequestHeader> (RequestHeader const & data
    , int32_t initial)
{
    return data.crc32(initial);
}

template <>
inline int32_t crc32_of<ReplyHeader> (ReplyHeader const & data
    , int32_t initial)
{
    return data.crc32(initial);
}
} // namespace pfs
