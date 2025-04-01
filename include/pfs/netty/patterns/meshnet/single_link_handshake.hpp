////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
//      2025.03.30 Renamed to single_link_handshake and refactored totally.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_handshake.hpp"
#include <pfs/assert.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class single_link_handshake: public basic_handshake<Node>
{
    using base_class = basic_handshake<Node>;
    using channel_collection_type = typename base_class::channel_collection_type;
    using node_id_rep = typename base_class::node_id_rep;
    using socket_id = typename base_class::socket_id;

public:
    single_link_handshake (Node * node, channel_collection_type * channels)
        : base_class(node, channels)
    {}

private:
    void complete_channel_success (node_id_rep const & id_rep, socket_id sid
        , std::string const & name, bool is_gateway)
    {
        auto success = this->_channels->insert_reader(id_rep, sid);
        success = success && this->_channels->insert_writer(id_rep, sid);

        PFS__ASSERT(success, "Fix meshnet::single_link_handshake algorithm");

        this->_on_completed(id_rep, sid, name, is_gateway, handshake_result_enum::success);
    }

public:
    void process (socket_id sid, handshake_packet const & pkt)
    {
        if (pkt.is_response()) {
            auto pos = this->_cache.find(sid);

            if (pos != this->_cache.end()) {
                // Finalize handshake (erase socket from cache)
                this->cancel(sid);

                if (this->_node->id_rep() == pkt.id_rep) {
                    // Check Node ID duplication. Requester must be an initiator of the socket closing.

                    this->_on_completed(pkt.id_rep, sid, pkt.name, pkt.is_gateway()
                        , handshake_result_enum::duplicated);
                } else {
                    // Response received by connected socket (writer)

                    if (pkt.accepted()) {
                        complete_channel_success(pkt.id_rep, sid, pkt.name, pkt.is_gateway());
                    } else {
                        this->_on_completed(pkt.id_rep, sid, pkt.name, pkt.is_gateway()
                            , handshake_result_enum::reject);
                    }
                }
            } else {
                // The socket is already expired
                // Usually this can't happen because socket must be closed by expired callback
                PFS__ASSERT(false, "socket must be closed by handshake `expired` callback");
            }
        } else { // Request received
            // Send response

            if (pkt.behind_nat()) {
                this->enqueue_response(sid, pkt.behind_nat(), true);
                complete_channel_success(pkt.id_rep, sid, pkt.name, pkt.is_gateway());
            } else {
                if (this->_node->id_rep() > pkt.id_rep) {
                    this->enqueue_response(sid, pkt.behind_nat(), true);
                    complete_channel_success(pkt.id_rep, sid, pkt.name, pkt.is_gateway());
                } else {
                    this->enqueue_response(sid, pkt.behind_nat(), false);
                }
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
