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
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

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
template <pfs::endian Endianess, typename SizeT>
class envelope final
{
public:
    static constexpr std::size_t MIN_SIZE = 2 + sizeof(SizeT);

private:
    static constexpr char BEGIN_FLAG = static_cast<char>(0xBE);
    static constexpr char END_FLAG = static_cast<char>(0xED);

public:
    using size_type = SizeT;

    class parser
    {
        pfs::binary_istream<Endianess> _in;

        // True wnen source contains inappropriate data
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
        pfs::optional<std::vector<char>> next ()
        {
            if (_bad)
                return pfs::nullopt;

            if (!_in.is_good()) {
                _bad = true;
                return pfs::nullopt;
            }

            if (_in.available() < MIN_SIZE)
                return pfs::nullopt;

            char begin_flag = 0;
            size_type payload_len = 0;

            _in.start_transaction();
            _in >> begin_flag >> payload_len;

            // Bad or corrupted envelope
            if (begin_flag != BEGIN_FLAG) {
                _bad = true;
                return pfs::nullopt;
            }

            // Not enough data
            if (_in.available() < payload_len + 1) { // +1 is end flag
                _in.rollback_transaction();
                return pfs::nullopt;
            }

            std::vector<char> result;
            result.reserve(payload_len);

            char end_flag = 0;
            _in >> std::make_pair(& result, static_cast<std::size_t>(payload_len)) >> end_flag;

            if (!_in.commit_transaction()) {
                _bad = true;
                return pfs::nullopt;
            }

            if (end_flag != END_FLAG) {
                _bad = true;
                return pfs::nullopt;
            }

            return result;
        }
    };

public:
    void pack (std::vector<char> & buf, char const * payload, size_type payload_len)
    {
        pfs::binary_ostream<Endianess> out {buf};
        out << BEGIN_FLAG << payload_len << pfs::string_view(payload, payload_len) << END_FLAG;
    }
};

template <pfs::endian Endianess, typename SizeT>
constexpr std::size_t envelope<Endianess, SizeT>::MIN_SIZE;

template <pfs::endian Endianess, typename SizeT>
constexpr char envelope<Endianess, SizeT>::BEGIN_FLAG;

template <pfs::endian Endianess, typename SizeT>
constexpr char envelope<Endianess, SizeT>::END_FLAG;

using envelope8_t    = envelope<pfs::endian::network, std::uint8_t>; // Endianess no matter here
using envelope16le_t = envelope<pfs::endian::little, std::uint16_t>;
using envelope16be_t = envelope<pfs::endian::big, std::uint16_t>;
using envelope32le_t = envelope<pfs::endian::little, std::uint32_t>;
using envelope32be_t = envelope<pfs::endian::big, std::uint32_t>;
using envelope64le_t = envelope<pfs::endian::little, std::uint64_t>;
using envelope64be_t = envelope<pfs::endian::big, std::uint64_t>;

NETTY__NAMESPACE_END
