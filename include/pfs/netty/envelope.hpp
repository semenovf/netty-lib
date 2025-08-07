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
#include <pfs/endian.hpp>
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
template <typename SizeT>
class envelope final
{
    static constexpr int MIN_SIZE = 2 + sizeof(SizeT);
    static constexpr char BEGIN_FLAG = static_cast<char>(0xBE);
    static constexpr char END_FLAG = static_cast<char>(0xED);

public:
    using size_type = SizeT;

    class parser
    {
        char const * _data {nullptr};
        std::size_t _size {0};

        // True wnen source contains inappropriate data
        bool _bad {false};

    public:
        parser (char const * data, std::size_t size)
            : _data(data)
            , _size(size)
        {}

    public:
        bool bad () const noexcept
        {
            return _bad;
        }

        std::size_t remain_size () const noexcept
        {
            return _size;
        }

        /**
         * Attempts to parse next envelope from raw bytes.
         */
        pfs::optional<envelope> next ()
        {
            if (_size < MIN_SIZE)
                return pfs::nullopt;

            if (_data[0] != BEGIN_FLAG) {
                _bad = true;
                return pfs::nullopt;
            }

            size_type payload_len = 0;
            pfs::unpack_unsafe<pfs::endian::network>(_data + 1, payload_len);

            if (_size < MIN_SIZE + payload_len)
                return pfs::nullopt;

            if (_data[payload_len + MIN_SIZE - 1] != END_FLAG) {
                _bad = true;
                return pfs::nullopt;
            }

            envelope env {payload_len};
            env.copy(_data + 1 + sizeof(size_type), payload_len);
            _data += payload_len + MIN_SIZE;
            _size -= payload_len + MIN_SIZE;

            return env;
        }
    };

private:
    std::vector<char> _data;

public:
    explicit envelope (size_type payload_len = 0)
        : _data(payload_len + MIN_SIZE)
    {
        stamp(payload_len);
    }

    envelope (envelope const &) = default;
    envelope (envelope &&) = default;
    envelope & operator = (envelope const &) = default;
    envelope & operator = (envelope &&) = default;

public:
    char const * data () const noexcept
    {
        return _data.empty() ? nullptr : _data.data();
    }

    std::size_t size () const noexcept
    {
        return _data.size();
    }

    char const * payload () const noexcept
    {
        auto n = _data.size();
        return _data.size() == MIN_SIZE ? nullptr : _data.data() + MIN_SIZE - 1;
    }

    std::size_t payload_size () const noexcept
    {
        return _data.size() - MIN_SIZE;
    }

    bool empty () const noexcept
    {
        return payload_size() == 0;
    }

    void copy (char const * payload, size_type payload_len)
    {
        if (payload == nullptr || payload_len == 0) {
            _data.resize(MIN_SIZE);
            stamp(0);
        } else {
            if (payload_len != payload_size()) {
                _data.resize(MIN_SIZE + payload_len);
                stamp(payload_len);
            }

            std::copy(payload, payload + payload_len, _data.data() + MIN_SIZE - 1);
        }
    }

private:
    // "Stick a stamp"
    void stamp (size_type payload_len)
    {
        _data[0] = BEGIN_FLAG;
        pfs::pack_unsafe<pfs::endian::network>(_data.data() + 1, payload_len);
        _data[payload_len + MIN_SIZE - 1] = END_FLAG;
    }
};

using envelope8_t  = envelope<std::uint8_t>;
using envelope16_t = envelope<std::uint16_t>;
using envelope32_t = envelope<std::uint32_t>;
using envelope64_t = envelope<std::uint64_t>;

NETTY__NAMESPACE_END
