////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "packet.hpp"
#include "pfs/uuid.hpp"
#include "pfs/ring_buffer.hpp"
#include <unordered_map>
#include <cstring>

namespace pfs {
namespace net {
namespace p2p {

template <std::size_t PacketSize>
class queue
{
    using packet_type = packet<PacketSize>;
    using seqnum_type = typename packet_type::seqnum_type;
    using internal_queue_type = ring_buffer_mt<packet_type, 256>;

    internal_queue_type _q;

private:
    seqnum_type split (uuid_t sender_uuid
        , seqnum_type initial_sn
        , char const * data, std::size_t len)
    {
        return split_into_packets<PacketSize>(sender_uuid, initial_sn
            , data, len, [this] (packet_type && pkt) {
                _q.push(std::move(pkt));
        });
    }

public:
    seqnum_type push (uuid_t sender_uuid
        , seqnum_type initial_sn
        , std::string const & msg)
    {
        return split(sender_uuid, initial_sn, msg.data(), msg.size());
    }
};

template <std::size_t PacketSize>
class delivery_manager final
{
    using packet_type = packet<PacketSize>;
    using queue_type = ring_buffer_mt<packet_type, 256>;
    using queue_container_type = std::unordered_map<uuid_t, queue_type>;
};

}}} // namespace pfs::net::p2p

