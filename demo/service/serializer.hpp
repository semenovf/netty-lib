////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "message.hpp"
#include "pfs/binary_istream_nt.hpp"
#include "pfs/binary_ostream.hpp"
#include "pfs/endian.hpp"
#include <string>

template <pfs::endian Endianess = pfs::endian::network>
struct message_serializer_impl
{
    using ostream_type = pfs::binary_ostream<Endianess>;
    using istream_type = pfs::binary_istream_nt<Endianess>;
    using archive_type = typename ostream_type::archive_type;

    ////////////////////////////////////////////////////////////////////////////////
    // echo serializer/deserializer
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, echo const & payload)
    {
        out << payload.text;
    }

    static void unpack (istream_type & in, echo & target)
    {
        in >> target.text;
    }
};

template <typename Packet, pfs::endian Endianess>
inline void pack (pfs::binary_ostream<Endianess> & out, Packet const & pkt)
{
    message_serializer_impl<Endianess>::pack(out, pkt);
}

template <typename Packet, pfs::endian Endianess>
inline void unpack (pfs::binary_istream_nt<Endianess> & in, Packet & pkt)
{
    message_serializer_impl<Endianess>::unpack(in, pkt);
}
