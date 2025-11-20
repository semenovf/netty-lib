////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "traits/archive_traits.hpp"
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

//
// Envelope format
//
// +--+--+--+------------+--+
// |BE| len | payload... |ED|
// +--+--+--+------------+--+
//
// Byte 0         - 0xBE, begin flag
// Bytes 1..Size  - Payload length (Size bytes)
// Bytes 3..len+2 - Payload
// Byte len+3     - 0xED, end flag
//
// SizeT determines how large the envelope can be.
//
template <typename SizeT, typename SerializerTraits>
class envelope final
{
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using archive_traits_type = archive_traits<archive_type>;

public:
    static constexpr std::size_t min_size() { return 2 + sizeof(SizeT); }

private:
    static constexpr char begin_flag () { return static_cast<char>(0xBE); }
    static constexpr char end_flag () { return static_cast<char>(0xED); }

public:
    using size_type = SizeT;

    class parser
    {
        deserializer_type _in;

        // True when source contains inappropriate data
        bool _bad {false};

    public:
        parser (char const * data, std::size_t size)
            : _in(data, size)
        {}

    public:
        bool bad () const noexcept
        {
            return _bad;
        }

        std::size_t remain_size () const noexcept
        {
            return _bad ? 0 : _in.available();
        }

        /**
         * Attempts to parse next envelope from raw bytes.
         */
        pfs::optional<archive_type> next ()
        {
            if (_bad)
                return pfs::nullopt;

            if (!_in.is_good()) {
                _bad = true;
                return pfs::nullopt;
            }

            if (_in.available() < min_size())
                return pfs::nullopt;

            char bflag = 0;
            size_type payload_len = 0;

            _in.start_transaction();
            _in >> bflag >> payload_len;

            // Bad or corrupted envelope
            if (bflag != begin_flag()) {
                _bad = true;
                return pfs::nullopt;
            }

            // Not enough data
            if (_in.available() < payload_len + 1) { // +1 is end flag
                _in.rollback_transaction();
                return pfs::nullopt;
            }

            char eflag = 0;

            // FIXME REMOVE
            //_in >> std::make_pair(archive_traits_type::data(result), static_cast<std::size_t>(payload_len)) >> end_flag;
            // auto result = archive_traits_type::make(_in.begin(), payload_len);

            archive_type ar;
            _in.read(ar, payload_len);
            _in >> eflag;

            if (!_in.commit_transaction()) {
                _bad = true;
                return pfs::nullopt;
            }

            if (eflag != end_flag()) {
                _bad = true;
                return pfs::nullopt;
            }

            return ar;
        }
    };

public:
    void pack (archive_type & ar, char const * payload, size_type payload_len)
    {
        serializer_type out {ar};
        out << begin_flag() << payload_len;
        out.write(payload, payload_len);
        out << end_flag();
    }
};

NETTY__NAMESPACE_END
