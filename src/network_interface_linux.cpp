////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// References:
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
            } else {
                iface = & result[it->second];
            }
        }
    }

    freeifaddrs(ifaddr);

    return result;
}

}} // pfs
