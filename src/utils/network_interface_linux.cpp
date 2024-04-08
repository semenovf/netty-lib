////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//      1. man netdevice
//      2. [Getting interface MTU under Linux with PCAP](https://serverfault.com/questions/361503/getting-interface-mtu-under-linux-with-pcap)
//      3. [using C code to get same info as ifconfig](https://stackoverflow.com/questions/4951257/using-c-code-to-get-same-info-as-ifconfig)
//      4. [Net-Tools](https://sourceforge.net/projects/net-tools/files/)
//
// Changelog:
//      2021.06.24 Initial version.
//      2024.04.08 Moved to `utils` namespace.
////////////////////////////////////////////////////////////////////////////////
#include "network_interface.hpp"
#include <map>
#include <cstring>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#if defined(ANDROID) && defined(__ANDROID_API__) && __ANDROID_API__ < 24
#   include "ifaddrs_android.h"
#   include "ifaddrs_android.c"
#else
#   include <ifaddrs.h>
#endif

#include "pfs/log.hpp"

namespace netty {
namespace utils {

static bool ioctl_helper (int fd, unsigned long request, struct ifreq * ifr, std::error_code & ec)
{
    if (ioctl(fd, request, ifr) < 0) {
        switch (errno) {
            case EPERM:
                ec = make_error_code(std::errc::permission_denied);
                break;
            case ENODEV:
                ec = make_error_code(std::errc::no_such_device);
                break;
            default:
                ec = make_error_code(errc::system_error);
                break;
        }

        return false;
    }

    return true;
}

//
// See `man 7 netdevice`.
// Man page implicitly says that get the MTU (Maximum Transfer Unit) of a device
// is not a privileged operation.
//

void foreach_interface (std::function<void(network_interface const &)> visitor, error * perr)
{
    std::vector<network_interface> cache;

    struct ifaddrs * ifaddr = nullptr;

    if (getifaddrs(& ifaddr) != 0) {
        pfs::throw_or(perr, error {
              errc::system_error
            , pfs::system_error_text()
        });

        return;
    }

    bool success = true;

    // Linux supports some standard ioctls to configure network devices.
    // They can be used on any socket's file descriptor regardless of
    // the family or type.
    auto sock = socket(PF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        pfs::throw_or(perr, error {
              errc::system_error
            , pfs::system_error_text()
        });

        return;
    }

    if (success) {
        for (struct ifaddrs * ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_name == nullptr)
                continue;

            std::string name {ifa->ifa_name};

            //LOGD("", "NAME:{}; ifa_addr={}; sa_family={}", name, (void*)ifa->ifa_addr, ifa->ifa_addr->sa_family);

            auto pos = std::find_if(cache.begin(), cache.end(), [& name] (network_interface const & iface) {
                return iface._data.adapter_name == name;
            });

            network_interface * iface = nullptr;
            bool cached = false;

            if (pos == cache.end()) {
                cache.emplace_back();
                iface = & cache.back();
            } else {
                iface = & *pos;
                cached = true;
            }

            std::error_code ec;

            if (!cached) {
                iface->_data.adapter_name = name;
                iface->_data.readable_name = name;

                struct ifreq ifr;
                std::memcpy(ifr.ifr_name, name.c_str(), name.size());
                ifr.ifr_name[name.size()] = '\0';

                if (!ec && ioctl_helper(sock, SIOCGIFHWADDR, & ifr, ec)) {
                    iface->_data.hardware_address = fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}"
                        , ifr.ifr_hwaddr.sa_data[0]
                        , ifr.ifr_hwaddr.sa_data[1]
                        , ifr.ifr_hwaddr.sa_data[2]
                        , ifr.ifr_hwaddr.sa_data[3]
                        , ifr.ifr_hwaddr.sa_data[4]
                        , ifr.ifr_hwaddr.sa_data[5]);
                }

                if (!ec && ioctl_helper(sock, SIOCGIFMTU, & ifr, ec))
                    iface->_data.mtu = ifr.ifr_mtu;

                if (!ec && ioctl_helper(sock, SIOCGIFFLAGS, & ifr, ec)) {
                    // Interface is a loopback interface
                    if (ifr.ifr_flags & IFF_LOOPBACK)
                        iface->_data.type = network_interface_type::loopback;

                    // Interface is a point-to-point link.
                    if (ifr.ifr_flags & IFF_POINTOPOINT)
                        iface->_data.type = network_interface_type::ppp;

                    iface->_data.status = network_interface_status::unknown;

                    // IFF_UP - Interface is running.
                    if (ifr.ifr_flags & IFF_UP)
                        iface->_data.status = network_interface_status::up;

                    // IFF_MULTICAST - Supports multicast
                    if (ifr.ifr_flags & IFF_MULTICAST)
                        iface->_data.flags |= static_cast<decltype(iface->_data.flags)>(network_interface_flag::multicast);

                    // TODO Other flags can be important
                    // IFF_BROADCAST     Valid broadcast address set.
                    // IFF_DEBUG         Internal debugging flag.
                    // IFF_RUNNING       Resources allocated.
                    // IFF_NOARP         No arp protocol, L2 destination address not set.
                    // IFF_PROMISC       Interface is in promiscuous mode.
                    // IFF_NOTRAILERS    Avoid use of trailers.
                    // IFF_ALLMULTI      Receive all multicast packets.
                    // IFF_MASTER        Master of a load balancing bundle.
                    // IFF_SLAVE         Slave of a load balancing bundle.
                    // IFF_PORTSEL       Is able to select media type via ifmap.
                    // IFF_AUTOMEDIA     Auto media selection active.
                    // IFF_DYNAMIC       The addresses are lost when the interface goes down.
                    // IFF_LOWER_UP      Driver signals L1 up (since Linux 2.6.17)
                    // IFF_DORMANT       Driver signals dormant (since Linux 2.6.17)
                    // IFF_ECHO          Echo sent packets (since Linux 2.6.25)
                }
            }


            // if (!ec && ioctl_helper(sock, SIOCGIFADDR, & ifr, ec)) {
            //     char buf[INET_ADDRSTRLEN];
            //     auto p = reinterpret_cast<struct sockaddr_in *>(& ifr.ifr_addr);
            //     iface->_data.ip4 = htonl(p->sin_addr.s_addr);
            //     iface->_data.ip4_name = inet_ntop(AF_INET, & p->sin_addr, buf, sizeof(buf));
            // }

            if (ifa->ifa_addr != nullptr) {
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    char buf[INET_ADDRSTRLEN];
                    auto p = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
                    iface->_data.ip4 = htonl(p->sin_addr.s_addr);
                    iface->_data.ip4_name = inet_ntop(AF_INET, & p->sin_addr, buf, sizeof(buf));
                    iface->_data.flags |= static_cast<decltype(iface->_data.flags)>(network_interface_flag::ip4_enabled);
                } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                    char buf[INET6_ADDRSTRLEN];
                    auto p = reinterpret_cast<sockaddr_in6 *>(ifa->ifa_addr);
                    iface->_data.ip6_name = inet_ntop(AF_INET6, & p->sin6_addr, buf, sizeof(buf));
                    iface->_data.flags |= static_cast<decltype(iface->_data.flags)>(network_interface_flag::ip6_enabled);
                } else {
                    // Nor IPv4 nor IPv6, ignore;
                }
            }

            if (ec) {
                pfs::throw_or(perr, error {
                      errc::system_error
                    , ec.message()
                });

                return;
            }
        }
    }

    freeifaddrs(ifaddr);

    for (auto const & iface: cache)
        visitor(iface);
}

}} // namespace netty::utils
