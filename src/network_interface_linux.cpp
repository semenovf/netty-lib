////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// References:
//      1. man netdevice
//      2. [Getting interface MTU under Linux with PCAP](https://serverfault.com/questions/361503/getting-interface-mtu-under-linux-with-pcap)
//      3. [using C code to get same info as ifconfig](https://stackoverflow.com/questions/4951257/using-c-code-to-get-same-info-as-ifconfig)
//      4. [Net-Tools](https://sourceforge.net/projects/net-tools/files/)
//
// Changelog:
//      2021.06.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/network_interface.hpp"
#include <map>
#include <cstring>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

namespace pfs {
namespace net {

static bool ioctl_helper (int fd
    , unsigned long request
    , struct ifreq * ifr
    , std::error_code & ec)
{
    if (ioctl(fd, request, ifr) < 0) {
        switch (errno) {
            case EPERM:
                ec = make_error_code(errc::permissions_denied);
                break;
            case ENODEV:
                ec = make_error_code(errc::device_not_found);
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

std::vector<network_interface> fetch_interfaces (std::error_code & ec)
{
    std::vector<network_interface> result;

    struct ifaddrs * ifaddr;

    if (getifaddrs(& ifaddr) != 0) {
        ec = make_error_code(errc::system_error);
        return result;
    }

    bool success = true;

    // Linux supports some standard ioctls to configure network devices.
    // They can be used on any socket's file descriptor regardless of
    // the family or type.
    auto sock = socket(PF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        ec = make_error_code(errc::system_error);
        success = false;
    }

    if (success) {
        std::map<std::string, size_t> ifaces_map;

        for (struct ifaddrs * ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_name == nullptr)
                continue;

            //if (ifa->ifa_addr == nullptr)
            //    continue;

            std::string name {ifa->ifa_name};

            auto it = ifaces_map.find(name);
            network_interface * iface = nullptr;

            if (it == ifaces_map.end()) {
                result.emplace_back();
                iface = & result.back();
                ifaces_map.insert({name, result.size() - 1});
                iface->_data.adapter_name = name;
                iface->_data.readable_name = name;

                struct ifreq ifr;
                std::memcpy(ifr.ifr_name, name.c_str(), name.size());
                ifr.ifr_name[name.size()] = '\0';

                if (ioctl_helper(sock, SIOCGIFMTU, & ifr, ec))
                    iface->_data.mtu = ifr.ifr_mtu;

                if (ioctl_helper(sock, SIOCGIFFLAGS, & ifr, ec)) {
                    // Interface is a loopback interface
                    if (ifr.ifr_flags & IFF_LOOPBACK)
                        iface->_data.type = network_interface_type::loopback;

                    // Interface is a point-to-point link.
                    if (ifr.ifr_flags & IFF_POINTOPOINT)
                        iface->_data.type = network_interface_type::ppp;

                    // TODO Other flags can be important
                    // IFF_UP            Interface is running.
                    // IFF_BROADCAST     Valid broadcast address set.
                    // IFF_DEBUG         Internal debugging flag.
                    // IFF_RUNNING       Resources allocated.
                    // IFF_NOARP         No arp protocol, L2 destination address not set.
                    // IFF_PROMISC       Interface is in promiscuous mode.
                    // IFF_NOTRAILERS    Avoid use of trailers.
                    // IFF_ALLMULTI      Receive all multicast packets.
                    // IFF_MASTER        Master of a load balancing bundle.
                    // IFF_SLAVE         Slave of a load balancing bundle.
                    // IFF_MULTICAST     Supports multicast
                    // IFF_PORTSEL       Is able to select media type via ifmap.
                    // IFF_AUTOMEDIA     Auto media selection active.
                    // IFF_DYNAMIC       The addresses are lost when the interface goes down.
                    // IFF_LOWER_UP      Driver signals L1 up (since Linux 2.6.17)
                    // IFF_DORMANT       Driver signals dormant (since Linux 2.6.17)
                    // IFF_ECHO          Echo sent packets (since Linux 2.6.25)

                }
            } else {
                iface = & result[it->second];
            }
        }
    }

    freeifaddrs(ifaddr);

    return result;
}

}} // pfs
