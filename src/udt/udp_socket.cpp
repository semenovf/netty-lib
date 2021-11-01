////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/net/p2p/udt/udp_socket.hpp"
#include "lib/udt.h"
#include <netinet/in.h>
#include <cassert>

namespace pfs {
namespace net {
namespace p2p {
namespace udt {

static UDTSOCKET create (inet4_addr const & addr
    , std::uint16_t port
    , sockaddr_in & addr_in4)
{
    int ai_family   = AF_INET;    // AF_INET | AF_INET6
    int ai_socktype = SOCK_DGRAM; // SOCK_DGRAM | SOCK_STREAM
    int ai_protocol = 0;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = ai_family;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(addr));

    UDTSOCKET socket = UDT::socket(ai_family, ai_socktype, ai_protocol);

    assert(socket != UDT::INVALID_SOCK);

    // UDT Options
    UDT::setsockopt(socket, 0, UDT_REUSEADDR, new bool(true), sizeof(bool));
    UDT::setsockopt(socket, 0, UDT_SNDSYN, new bool(false), sizeof(bool)); // sending is non-blocking
    UDT::setsockopt(socket, 0, UDT_RCVSYN, new bool(false), sizeof(bool)); // receiving is non-blocking
    //UDT::setsockopt(_socket, 0, UDT_MSS, new int(9000), sizeof(int));
    //UDT::setsockopt(_socket, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
    //UDT::setsockopt(_socket, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(_socket, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

    return socket;
}

udp_socket::~udp_socket ()
{
    close();
}

udp_socket::status_enum udp_socket::state () const
{
    assert(_socket >= 0);
    auto status = UDT::getsockstate(_socket);
    return static_cast<status_enum>(status);
}

bool udp_socket::bind (inet4_addr const & addr, std::uint16_t port)
{
    sockaddr_in addr_in4;
    _socket = create(addr, port, addr_in4);

    auto rc = UDT::bind(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (UDT::ERROR == rc) {
        failure(fmt::format("bind {}:{} to socket failure: {}"
            , std::to_string(addr)
            , port
            , error_string()));
        return false;
    }

    return true;
}

bool udp_socket::listen (int backlog)
{
    assert(_socket >= 0);

    auto rc = UDT::listen(_socket, backlog);

    if (rc < 0) {
        failure(fmt::format("`listen` failure: {}"
            , error_string()));
        return false;
    }

    return true;
}

udp_socket udp_socket::accept (inet4_addr * addr_ptr, std::uint16_t * port_ptr)
{
    udp_socket result;

    sockaddr saddr;
    int addrlen;

    result._socket = UDT::accept(_socket, & saddr, & addrlen);

    if (saddr.sa_family == AF_INET) {
        auto addr_in4_ptr = reinterpret_cast<sockaddr_in *>(& saddr);

        if (addr_ptr) {
            *addr_ptr = pfs::to_native_order(
                static_cast<std::uint32_t>(addr_in4_ptr->sin_addr.s_addr));
        }

        if (port_ptr) {
            *port_ptr = pfs::to_native_order(
                static_cast<std::uint16_t>(addr_in4_ptr->sin_port));
        }
    } else {
        failure("`accept` failure: unsupported sockaddr family"
            " (AF_INET supported only)");
    }

    return result;
}

bool udp_socket::connect (inet4_addr const & addr, std::uint16_t port)
{
    sockaddr_in addr_in4;
    _socket = create(addr, port, addr_in4);

    auto rc = UDT::connect(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (rc < 0) {
        failure(fmt::format("connection to {}:{} failure: {}"
            , std::to_string(addr)
            , port
            , error_string()));
        return false;
    }

    return true;
}

void udp_socket::close ()
{
    if (_socket >= 0) {
        UDT::close(_socket);
        _socket = -1;
    }
}

//     template <typename Callback>
//     void process_incoming_data (Callback && callback)
//     {
//         while (_socket.hasPendingDatagrams()) {
//
// #if QT_VERSION < QT_VERSION_CHECK(5,8,0)
//             // using QUdpSocket::readDatagram (API since Qt 4)
//             QByteArray packet_data;
//             QHostAddress sender_hostaddr;
//             std::uint16_t sender_port;
//             packet_data.resize(static_cast<int>(_socket.pendingDatagramSize()));
//             _socket.readDatagram(datagram.data(), datagram.size()
//                 , & sender_hostaddr, & sender_port);
// #else
//             // using QUdpSocket::receiveDatagram (API since Qt 5.8)
//             QNetworkDatagram datagram = _socket.receiveDatagram();
//             QByteArray packet_data = datagram.data();
//             auto sender_hostaddr = datagram.senderAddress();
//
//             if (datagram.senderPort() < 0) {
//                 failure("no sender port associated with datagram");
//                 continue;
//             }
//
//             auto sender_port = static_cast<std::uint16_t>(datagram.senderPort());
// #endif
//
//             bool ok {true};
//             inet4_addr sender_inet4_addr = sender_hostaddr.toIPv4Address(& ok);
//
//             if (!ok) {
//                 failure("bad remote address (expected IPv4)");
//                 continue;
//             }
//
//             callback(sender_inet4_addr, sender_port, packet_data.data(), packet_data.size());
//         }
//     }

std::streamsize udp_socket::send (char const * data, std::streamsize len)
{
    int ttl_millis = -1;
    bool inorder = true;
    return UDT::sendmsg(_socket, data, len, ttl_millis, inorder);
}

std::string udp_socket::error_string () const
{
    return UDT::getlasterror().getErrorMessage();
}

}}}} // namespace pfs::net::p2p::udt
