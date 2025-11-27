////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.09 Initial version.
//      2025.11.27 Include account type instead of external type.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../callback.hpp"
#include "frame.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/utility.hpp>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace pubsub {

template <typename SerializerTraits>
class input_controller
{
public:
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;

private:
    using deserializer_type = typename serializer_traits_type::deserializer_type;

    class account
    {
        using frame_type = frame<serializer_traits_type>;

    private:
        // Buffer to accumulate raw data
        archive_type _raw;

    public:
        // Buffer to accumulate payloads
        archive_type ar;

    public:
        void append_chunk (archive_type const & chunk)
        {
            _raw.append(chunk);

            while (frame_type::parse(ar, _raw)) {
                ; // empty body
            }
        }
    };

protected:
    account _acc;

public:
    input_controller () = default;

public: // Callbacks
    mutable callback_t<void (archive_type)> on_data_ready = [] (archive_type) {};

public:
    void process_input (archive_type && chunk)
    {
        if (chunk.empty())
            return;

        _acc.append_chunk(std::move(chunk));

        auto & ar = _acc.ar;

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
                case packet_enum::data: {
                    archive_type bytes_in;
                    data_packet pkt {h, in, bytes_in};

                    if (in.commit_transaction())
                        on_data_ready(std::move(bytes_in));
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
};

} // namespace pubsub

NETTY__NAMESPACE_END
