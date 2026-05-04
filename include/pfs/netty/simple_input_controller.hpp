////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "callback.hpp"
#include "simple_frame.hpp"
#include <pfs/assert.hpp>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

template <typename SocketId, typename SerializerTraits>
class simple_input_controller
{
public:
    using socket_id = SocketId;
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;

private:
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using frame_type = simple_frame<serializer_traits_type>;

    struct account
    {
        // Buffer to accumulate raw data
        archive_type raw;

        // Buffer to accumulate payloads
        archive_type ar;
    };

private:
    std::unordered_map<socket_id, account> _accounts;

public: // Callbacks
    mutable callback_t<void (socket_id, archive_type)> on_data_ready = [] (socket_id, archive_type) {};

public:
    simple_input_controller () = default;

public:
    void process_input (socket_id sid, archive_type && chunk)
    {
        if (chunk.empty())
            return;

        auto & acc = ensure_account(sid, std::move(chunk));

        while (true) {
            archive_type ar;

            if (!frame_type::parse(ar, acc.raw))
                break;

            on_data_ready(sid, std::move(ar));
        }
    }

    void remove (socket_id sid)
    {
        _accounts.erase(sid);
    }

private:
    account * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;
        return & acc;
    }

    account & ensure_account (socket_id sid, archive_type && chunk)
    {
        auto * pacc = locate_account(sid);

        if (pacc == nullptr) {
            auto pos = _accounts.emplace(sid, account{});
            PFS__THROW_UNEXPECTED(pos.second, "account insert failure");
            pacc = & pos.first->second;
        }

        pacc->raw.append(std::move(chunk));
        return *pacc;
    }
};

NETTY__NAMESPACE_END
