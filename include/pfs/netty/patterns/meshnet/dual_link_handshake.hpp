////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_handshake.hpp"

#include "../../trace.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class dual_link_handshake: public basic_handshake<Node>
{
    using base_class = basic_handshake<Node>;
    using channel_collection_type = typename base_class::channel_collection_type;
    using socket_id = typename base_class::socket_id;

public:
    dual_link_handshake (Node * node, channel_collection_type * channels)
        : base_class(node, channels)
    {}

public:
    void process (socket_id sid, handshake_packet const & pkt)
    {
        if (pkt.is_response()) {
            auto pos = this->_cache.find(sid);

            if (pos != this->_cache.end()) {
                // Finalize handshake (erase socket from cache)
                this->cancel(sid);

                // Check Node ID duplication. Requester must be an initiator of the socket closing.
                if (this->_node->id_rep() == pkt.id_rep) {
                    NETTY__TRACE("[handshake]", "{}: DUPLICATED: sid={}", pkt.name, sid)
                    this->_channels->close_channel(sid);
                    this->_on_completed(pkt.id_rep, sid, pkt.name, pkt.is_gateway(), handshake_result_enum::duplicated);
                } else {
                    NETTY__TRACE("[handshake]", "{}: RESPONSE: sid={}", pkt.name, sid)

                    // Response received by connected socket (writer)
                    auto success = this->_channels->insert_writer(pkt.id_rep, sid);
                    PFS__ASSERT(success, "Fix meshnet::dual_link_handshake algorithm");

                    // Check if reader and writer handshaked
                    if (this->_channels->channel_complete_for(pkt.id_rep))
                        this->_on_completed(pkt.id_rep, sid, pkt.name, pkt.is_gateway(), handshake_result_enum::success);
                }
            } else {
                // The socket is already expired
                // Usually this can't happen because socket must be closed by expired callback
                PFS__ASSERT(false, "socket must be closed by handshake `expired` callback");
            }
        } else { // Request received
            NETTY__TRACE("[handshake]", "{}: REQUEST: sid={}", pkt.name, sid)

            // Send response
            this->enqueue(sid, packet_way_enum::response);

            // Request received by accepted socket (reader)
            auto success = this->_channels->insert_reader(pkt.id_rep, sid);
            PFS__ASSERT(success, "Fix meshnet::dual_link_handshake algorithm");

            // Check if reader and writer handshaked
            if (this->_channels->channel_complete_for(pkt.id_rep))
                this->_on_completed(pkt.id_rep, sid, pkt.name, pkt.is_gateway(), handshake_result_enum::success);
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
