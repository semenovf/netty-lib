////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../basic_reader.hpp"
#include "packet_serializer.hpp"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/crc32.hpp"
#include "pfs/fmt.hpp"
#include "pfs/memory.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <memory>
#include <cassert>
#include <unordered_map>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

template <std::size_t PacketSize>
class udp_reader : public basic_reader<udp_reader<PacketSize>, PacketSize>
{
    using base_class = basic_reader<udp_reader<PacketSize>, PacketSize>;

    friend class basic_reader<udp_reader<PacketSize>, PacketSize>;

public:
    using packet_type = typename base_class::packet_type;

private:
    struct internal_options {
        QHostAddress listener_addr4;
        quint16 listener_port;
        QNetworkInterface listener_interface;
    };

    bool _started {false};
    internal_options _opts;
    std::unique_ptr<QUdpSocket> _reader;

private:
    static bool is_remote_addr (QHostAddress const & addr)
    {
        assert(!addr.isNull());

        if (addr.isLoopback() || QNetworkInterface::allAddresses().contains(addr))
            return false;

        return true;
    }

protected:
    bool set_options_impl (typename base_class::options && opts)
    {
        if (_started) {
            base_class::failure("unable to set options during operation");
            return false;
        }

        if (opts.listener_addr4 == inet4_addr{})
            _opts.listener_addr4 = QHostAddress::AnyIPv4;
        else
            _opts.listener_addr4 = QHostAddress{static_cast<quint32>(opts.listener_addr4)};

        if (_opts.listener_addr4.isNull()) {
            base_class::failure("bad listener address");
            return false;
        }

        _opts.listener_port = opts.listener_port;

        if (!opts.listener_interface.empty() && opts.listener_interface != "*") {
            _opts.listener_interface = QNetworkInterface::interfaceFromName(
                QString::fromStdString(opts.listener_interface));

            if (!_opts.listener_interface.isValid()) {
                base_class::failure("bad listener interface specified");
                return false;
            }
        }

        return true;
    }

    void process_incoming_datagrams ()
    {
        while (_reader->hasPendingDatagrams()) {

#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
            QByteArray packet_data;
            QHostAddress sender_hostaddr;
            packet_data.resize(static_cast<int>(_reader->pendingDatagramSize()));
            _reader->readDatagram(packet_data.data(), packet_data.size(), & sender_hostaddr);
#else
            QNetworkDatagram datagram = _reader->receiveDatagram();
            QByteArray packet_data = datagram.data();
            auto sender_hostaddr = datagram.senderAddress();
#endif

            if (packet_data.size() != packet_type::PACKET_SIZE) {
                base_class::failure(fmt::format("bad packet size: {}, expected {}"
                    , packet_data.size()
                    , packet_type::PACKET_SIZE));
                return;
            }

            bool ok {true};

            // IPv6 address (TODO implement support of IPv6)
            inet4_addr sender_inet4_addr = sender_hostaddr.toIPv4Address(& ok);

            if (!ok) {
                base_class::failure("bad sender address (expected IPv4)");
                return;
            }

            // Ignore self packets
            if (is_remote_addr(sender_hostaddr)) {
                auto parse_result = qt5::deserialize_packet<packet_type::PACKET_SIZE>(packet_data);

                if (!parse_result.first.empty()) {
                    base_class::failure(parse_result.first);
                    return;
                }

                base_class::packet_received(parse_result.second);
            }
        }
    }

    bool start_impl ()
    {
        auto rc = false;

        if (!_started) {
            do {
                assert(_reader.get() == nullptr);
                _reader = pfs::make_unique<QUdpSocket>();

                QUdpSocket::BindMode bind_mode = QUdpSocket::ShareAddress
                    | QUdpSocket::ReuseAddressHint;

                if (!_reader->bind(_opts.listener_addr4, _opts.listener_port, bind_mode)) {
                    base_class::failure("listener socket binding failure");
                    break;
                }

                QObject::connect(& *_reader, & QUdpSocket::readyRead
                    , [this] {
                        process_incoming_datagrams();
                });

                QObject::connect(& *_reader
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
                    , & QUdpSocket::errorOccurred
#else
                    , static_cast<void (QUdpSocket::*)(QUdpSocket::SocketError)>(& QUdpSocket::error)
#endif
                    , [this] (QUdpSocket::SocketError error_code) {
                        // RemoteHostClosedError will not consider an error
                        if (error_code != QUdpSocket::RemoteHostClosedError) {
                            this->failure(_reader->errorString().toStdString());
                        }
                });

                _started = true;
                rc = true;
            } while (false);
        } else {
            rc = true;
        }

        if (!rc) {
            _reader.reset();
        }

        return rc;
    }

    void stop_impl ()
    {
        if (_started) {
            _reader.reset();
            _started = false;
        }
    }

    bool started_impl () const noexcept
    {
        return _started;
    }

    inet4_addr address_impl () const noexcept
    {
        return inet4_addr{_opts.listener_addr4.toIPv4Address()};
    }

    std::uint16_t port_impl () const noexcept
    {
        return _opts.listener_port;
    }

public:
    udp_reader () = default;

    ~udp_reader ()
    {
        if (_started)
            stop_impl();
    }

    udp_reader (udp_reader const &) = delete;
    udp_reader & operator = (udp_reader const &) = delete;

    udp_reader (udp_reader &&) = default;
    udp_reader & operator = (udp_reader &&) = default;
};

}}}} // namespace pfs::net::p2p::qt5
