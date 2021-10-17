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
#include "pfs/memory.hpp"
#include <QUdpSocket>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

class udp_writer : public basic_writer<udp_writer>
{
    using base_class = basic_writer<udp_writer>;

    friend class basic_writer<udp_writer>;

private:
    std::unique_ptr<QUdpSocket> _writer;

protected:
    std::streamsize write_impl (inet4_addr const & addr, std::uint32_t port
        , char const * data, std::streamsize size)
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

        return _writer->writeDatagram(data, size
            , QHostAddress{static_cast<std::uint32_t>(addr)}, port);
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

