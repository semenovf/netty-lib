////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
//      2025.03.30 Renamed to single_link_handshake and refactored totally.
//      2025.06.06 Algorithm fixed.
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
    using node_id = typename base_class::node_id;
    using socket_id = typename base_class::socket_id;

public:
    single_link_handshake (Node * node)
        : base_class(node)
    {}

private:
    // TODO Methods process_behind_nat() and process_exclusive() can be merged into one method
    // if algorithm will be successfully tested.

    void process_behind_nat (socket_id sid, handshake_packet<node_id> const & pkt)
    {
        // Check Node ID duplication.
        bool is_duplicated = this->_node->id() == pkt.id;

        if (pkt.is_response()) {
            // `sid` is a connected socket.
            // Finalize handshake (erase socket from cache)
            bool canceled = this->cancel(sid);

            PFS__ASSERT(canceled, "socket must be closed by handshake `expired` callback");

            if (is_duplicated)
                this->on_duplicate_id(pkt.id, sid, true);
        } else { // Request
            // `sid` is an accepted socket
            // Send response
            this->enqueue_response(sid, pkt.behind_nat(), true);

            // NOTE. Accepted socket can't be closed if ID duplacated because need to send response.
            // Requester (connected socket) will be an initiator of the socket closing.

            if (is_duplicated)
                this->on_duplicate_id(pkt.id, sid, false);
        }

        if (!is_duplicated)
            this->on_completed(pkt.id, sid, sid, pkt.is_gateway());
    }

    void process_exclusive (socket_id sid, handshake_packet<node_id> const & pkt)
    {
        // Check Node ID duplication.
        bool is_duplicated = this->_node->id() == pkt.id;

        if (pkt.is_response()) {
            // `sid` is a connected socket.
            // Finalize handshake (erase socket from cache)
            bool canceled = this->cancel(sid);

            PFS__ASSERT(canceled, "socket must be closed by handshake `expired` callback");

            if (is_duplicated) {
                this->on_duplicate_id(pkt.id, sid, true);
            } else {
                if (this->_node->id() > pkt.id) {
                    this->on_completed(pkt.id, sid, sid, pkt.is_gateway());
                } else {
                    this->on_discarded(pkt.id, sid);
                }
            }
        } else { // Request
            // `sid` is an accepted socket
            // Send response
            this->enqueue_response(sid, pkt.behind_nat(), true);

            if (is_duplicated) {
                this->on_duplicate_id(pkt.id, sid, false);
            } else {
                if (this->_node->id() < pkt.id) {
                    this->on_completed(pkt.id, sid, sid, pkt.is_gateway());
                }
            }
        }
    }

public:
    void process (socket_id sid, handshake_packet<node_id> const & pkt)
    {
        if (pkt.behind_nat()) {
            process_behind_nat(sid, pkt);
            return;
        } else {
            process_exclusive(sid, pkt);
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
