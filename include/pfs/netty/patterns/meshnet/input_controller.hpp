////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.05 Initial version.
//      2025.05.07 Renamed from basic_input_processor.hpp to basic_input_controller.hpp.
//                 Class `basic_input_processor` renamed to `basic_input_controller`.
//      2025.11.20 Added callbacks.
//                 Merged with input_account.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "priority_frame.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/utility.hpp>
#include <array>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <int PriorityCount, typename SocketId, typename NodeId, typename SerializerTraits>
class input_controller
{
    using socket_id = SocketId;
    using node_id = NodeId;
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using priority_frame_type = priority_frame<PriorityCount, serializer_traits_type>;

    class account
    {
        archive_type _raw; // Buffer to accumulate raw data

    public:
        std::array<archive_type, PriorityCount> pool;

    public:
        // argument need to properly call from unordered_map::emplace prior to C++17
        account (int) {}

        account (account const&) = delete;
        account (account &&) = delete;
        account & operator = (account const &) = delete;
        account & operator = (account &&) = delete;
        ~account () = default;

        // Called from input_controller while process input
        void append_chunk (archive_type && chunk)
        {
            _raw.append(std::move(chunk));

            while (priority_frame_type::parse(pool, _raw)) {
                ; // empty body
            }
        }
    };

protected:
    std::unordered_map<socket_id, account> _accounts;

public:
    mutable callback_t<void (socket_id, handshake_packet<node_id> &&)> on_handshake;
    mutable callback_t<void (socket_id, heartbeat_packet &&)> on_heartbeat;
    mutable callback_t<void (socket_id, unreachable_packet<node_id> &&)> on_unreachable;
    mutable callback_t<void (socket_id, route_packet<node_id> &&)> on_route;
    mutable callback_t<void (socket_id, int /*priority*/, archive_type &&)> on_ddata;
    mutable callback_t<void (socket_id, int /*priority*/, gdata_packet<node_id> &&, archive_type &&)> on_gdata;

public:
    input_controller () = default;

private:
    account * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;
        return & acc;
    }

public:
    void add (socket_id sid)
    {
        auto * pacc = locate_account(sid);

        if (pacc != nullptr)
            remove(sid);

        _accounts.emplace(sid, 0);
    }

    void remove (socket_id sid)
    {
        _accounts.erase(sid);
    }

    void process_input (socket_id sid, archive_type && chunk)
    {
        if (chunk.empty())
            return;

        auto * pacc = locate_account(sid);

        PFS__THROW_UNEXPECTED(pacc != nullptr, "account not found, fix");

        pacc->append_chunk(std::move(chunk));

        for (int priority = 0; priority < PriorityCount; priority++) {
            auto & ar = pacc->pool[priority];

            if (ar.empty())
                continue;

            deserializer_type in {ar.data(), ar.size()};
            bool has_more_packets = true;

            while (has_more_packets && in.available() > 0) {
                in.start_transaction();
                header h {in};

                // Incomplete header
                if (!in.is_good()) {
                    has_more_packets = false;
                    continue;
                }

                switch (h.type()) {
                    case packet_enum::handshake: {
                        handshake_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            on_handshake(sid, std::move(pkt));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::heartbeat: {
                        heartbeat_packet pkt {h, in};

                        if (in.commit_transaction())
                            on_heartbeat(sid, std::move(pkt));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::unreach: {
                        unreachable_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            on_unreachable(sid, std::move(pkt));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::route: {
                        route_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            on_route(sid, std::move(pkt));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::ddata: {
                        archive_type bytes_in;
                        ddata_packet pkt {h, in, bytes_in};

                        if (in.commit_transaction())
                            on_ddata(sid, priority, std::move(bytes_in));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::gdata: {
                        archive_type bytes_in;
                        gdata_packet<node_id> pkt {h, in, bytes_in};

                        if (in.commit_transaction())
                            on_gdata(sid, priority, std::move(pkt), std::move(bytes_in));
                        else
                            has_more_packets = false;

                        break;
                    }

                    default:
                        throw error {
                              make_error_code(pfs::errc::unexpected_error)
                            , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        has_more_packets = false;
                        break;
                }
            }

            if (in.available() == 0) {
                ar.clear();
            } else {
                auto n = ar.size() - in.available();
                ar.erase_front(n);
            }
        }
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
