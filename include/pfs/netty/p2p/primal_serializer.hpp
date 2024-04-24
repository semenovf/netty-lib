////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "hello_packet.hpp"
#include "packet.hpp"
#include "universal_id.hpp"
#include "pfs/endian.hpp"
#include "pfs/binary_istream.hpp"
#include "pfs/binary_ostream.hpp"
#include "pfs/string_view.hpp"
#include "pfs/universal_id_pack.hpp"

namespace netty {
namespace p2p {

template <pfs::endian Endianess = pfs::endian::network>
struct primal_serializer
{
    using output_archive_type = std::vector<char>;
    using input_archive_type  = pfs::string_view;
    using ostream_type = pfs::binary_ostream<Endianess>;
    using istream_type = pfs::binary_istream<Endianess>;

    ////////////////////////////////////////////////////////////////////////////////
    // packet
    ////////////////////////////////////////////////////////////////////////////////

    static void pack (ostream_type & out, packet const & pkt)
    {
        out << pkt.packettype
            << pkt.packetsize
            << pkt.addresser
            << pkt.payloadsize
            << pkt.partcount
            << pkt.partindex
            << pfs::exclude_size{}
            << std::make_pair(pkt.payload, pkt.packetsize - packet::PACKET_HEADER_SIZE);
    }

    static void unpack (istream_type & in, packet & pkt)
    {
        in >> pkt.packettype
            >> pkt.packetsize
            >> pkt.addresser
            >> pkt.payloadsize
            >> pkt.partcount
            >> pkt.partindex;

        auto n = static_cast<typename istream_type::size_type>(pkt.packetsize) - packet::PACKET_HEADER_SIZE;

        in  >> pfs::expected_size{n}
            >> std::make_pair(pkt.payload, n);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // hello_packet
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, hello_packet const & pkt)
    {
        out << pkt.greeting[0]
            << pkt.greeting[1]
            << pkt.greeting[2]
            << pkt.greeting[3]
            << pkt.uuid
            << pkt.port
            << pkt.expiration_interval
            << pkt.counter
            << pkt.timestamp
            << crc16_of(pkt);
    }

    static void unpack (istream_type & in, hello_packet & pkt)
    {
        in  >> pkt.greeting[0]
            >> pkt.greeting[1]
            >> pkt.greeting[2]
            >> pkt.greeting[3]
            >> pkt.uuid
            >> pkt.port
            >> pkt.expiration_interval
            >> pkt.counter
            >> pkt.timestamp
            >> pkt.crc16;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // hello
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & /*out*/, hello const & /*h*/)
    {}

    static void unpack (istream_type & /*in*/, hello & /*h*/)
    {}

    ////////////////////////////////////////////////////////////////////////////////
    // file_credentials
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_credentials const & fc)
    {
        out << fc.fileid << fc.filename << fc.filesize << fc.offset;
    }

    static void unpack (istream_type & in, file_credentials & fc)
    {
        in >> fc.fileid >> fc.filename >> fc.filesize >> fc.offset;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_request
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_request const & fr)
    {
        out << fr.fileid << fr.offset;
    }

    static void unpack (istream_type & in, file_request & fr)
    {
        in >> fr.fileid >> fr.offset;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_stop
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_stop const & fs)
    {
        out << fs.fileid;
    }

    static void unpack (istream_type & in, file_stop & fs)
    {
        in >> fs.fileid;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_chunk_header
    ////////////////////////////////////////////////////////////////////////////////
    static void unpack (istream_type & in, file_chunk_header & fch)
    {
        in >> fch.fileid >> fch.offset >> fch.chunksize;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_chunk
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_chunk const & fc)
    {
        out << fc.fileid << fc.offset << fc.chunksize << pfs::exclude_size{} << fc.chunk;
    }

    static void unpack (istream_type & in, file_chunk & fc)
    {
        in >> fc.fileid >> fc.offset >> fc.chunksize
            >> pfs::expected_size{static_cast<typename istream_type::size_type>(fc.chunksize)}
            >> fc.chunk;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_begin
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_begin const & fb)
    {
        out << fb.fileid << fb.offset;
    }

    static void unpack (istream_type & in, file_begin & fb)
    {
        in >> fb.fileid >> fb.offset;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_end
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_end const & fe)
    {
        out << fe.fileid;
    }

    static void unpack (istream_type & in, file_end & fe)
    {
        in >> fe.fileid;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // file_state
    ////////////////////////////////////////////////////////////////////////////////
    static void pack (ostream_type & out, file_state const & fs)
    {
        out << fs.fileid << fs.status;
    }

    static void unpack (istream_type & in, file_state & fs)
    {
        in >> fs.fileid >> fs.status;
    }
};

template <typename Packet, pfs::endian Endianess>
inline void pack (pfs::binary_ostream<Endianess> & out, Packet const & pkt)
{
    primal_serializer<Endianess>::pack(out, pkt);
}

template <typename Packet, pfs::endian Endianess>
inline void unpack (pfs::binary_istream<Endianess> & in, Packet & pkt)
{
    primal_serializer<Endianess>::unpack(in, pkt);
}

}} // namespace netty::p2p
