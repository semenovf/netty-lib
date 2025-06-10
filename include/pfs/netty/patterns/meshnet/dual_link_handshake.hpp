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
#include "tag.hpp"
#include "../../trace.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

#if FIXME // Need fixing (see single_link_handshake)

template <typename Node>
class dual_link_handshake: public basic_handshake<Node>
{
    using base_class = basic_handshake<Node>;
    using socket_id = typename base_class::socket_id;
    using node_id = typename base_class::node_id;

public:
    dual_link_handshake (Node * node)
        : base_class(node)
    {}

public:
    void process (socket_id sid, handshake_packet<node_id> const & pkt)
    {
        if (pkt.is_response()) {
            auto pos = this->_cache.find(sid);

            if (pos != this->_cache.end()) {
                // Finalize handshake (erase socket from cache)
                this->cancel(sid);

                // Check Node ID duplication. Requester must be an initiator of the socket closing.
                if (this->_node->id() == pkt.id) {
                    NETTY__TRACE(MESHNET_TAG, "DUPLICATED: sid={}", sid)
                    this->_channels->close_channel(sid);
                    this->on_completed(pkt.id, sid, pkt.is_gateway(), handshake_result_enum::duplicated);
                } else {
                    NETTY__TRACE(MESHNET_TAG, "RESPONSE: sid={}", sid)

                    // Response received by connected socket (writer)
                    auto success = this->_channels->insert_writer(pkt.id, sid);
                    PFS__THROW_UNEXPECTED(success, "Fix meshnet::dual_link_handshake algorithm");

                    // Check if reader and writer handshaked
                    if (this->_channels->channel_complete_for(pkt.id))
                        this->on_completed(pkt.id, sid, pkt.is_gateway(), handshake_result_enum::success);
                }
            } else {
                // The socket is already expired
                // Usually this can't happen because socket must be closed by expired callback
                PFS__THROW_UNEXPECTED(false, "socket must be closed by handshake `expired` callback");
            }
        } else { // Request received
            NETTY__TRACE(MESHNET_TAG, "REQUEST: sid={}", sid)

            // Send response
            this->enqueue(sid, packet_way_enum::response);

            // Request received by accepted socket (reader)
            auto success = this->_channels->insert_reader(pkt.id, sid);
            PFS__THROW_UNEXPECTED(success, "Fix meshnet::dual_link_handshake algorithm");

            // Check if reader and writer handshaked
            if (this->_channels->channel_complete_for(pkt.id))
                this->on_completed(pkt.id, sid, pkt.is_gateway(), handshake_result_enum::success);
        }
    }
};

#endif

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
