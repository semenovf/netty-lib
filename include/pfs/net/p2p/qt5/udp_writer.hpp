////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../basic_writer.hpp"
#include "packet_serializer.hpp"
// #include "pfs/net/inet4_addr"
// #include "pfs/crc32.hpp"
// #include "pfs/fmt.hpp"
#include "pfs/memory.hpp"
// #include "pfs/optional.hpp"
// #include <QDataStream>
// #include <QHostAddress>
// #include <QNetworkInterface>
#include <QUdpSocket>
// #include <memory>
// #include <cassert>
// #include <unordered_map>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

template <std::size_t PacketSize>
class udp_writer : public basic_writer<udp_writer<PacketSize>, PacketSize>
{
    using base_class = basic_writer<udp_writer<PacketSize>, PacketSize>;
    using packet_type = typename base_class::packet_type;

    friend class basic_writer<udp_writer<PacketSize>, PacketSize>;

private:
    std::unique_ptr<QUdpSocket> _writer;

protected:
    void write_impl (inet4_addr const & addr, std::uint32_t port, packet_type const & packet)
    {
        if (!_writer) {
            _writer = pfs::make_unique<QUdpSocket>();

            QObject::connect(& *_writer
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
                , & QUdpSocket::errorOccurred
#else
                , static_cast<void (QUdpSocket::*)(QUdpSocket::SocketError)>(& QUdpSocket::error)
#endif
                , [this] (QUdpSocket::SocketError error_code) {
                    // RemoteHostClosedError will not consider an error
                    if (error_code != QUdpSocket::RemoteHostClosedError) {
                        this->failure(_writer->errorString().toStdString());
                    }
            });
        }

        auto bytes = qt5::serialize(packet);
        _writer->writeDatagram(bytes, addr, port);
    }

public:
    udp_writer () = default;
    ~udp_writer () = default;

    udp_writer (udp_writer const &) = delete;
    udp_writer & operator = (udp_writer const &) = delete;

    udp_writer (udp_writer &&) = default;
    udp_writer & operator = (udp_writer &&) = default;
};

}}}} // namespace pfs::net::p2p::qt5

