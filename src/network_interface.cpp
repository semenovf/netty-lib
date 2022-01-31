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
#include "pfs/netty/network_interface.hpp"

namespace netty {

std::string to_string (network_interface_type type)
{
    switch (type) {
        case network_interface_type::ethernet:
            return std::string{"Ethernet"};
        case network_interface_type::tokenring:
            return std::string{"Token Ring"};
        case network_interface_type::ppp:
            return std::string{"PPP"};
        case network_interface_type::loopback:
            return std::string{"loopback"};
        case network_interface_type::atm:
            return std::string{"ATM"};
        case network_interface_type::ieee80211:
            return std::string{"IEEE 802.11"};
        case network_interface_type::tunnel:
            return std::string{"tunnel"};
        case network_interface_type::ieee1394:
            return std::string{"IEEE 1394"};
        case network_interface_type::fddi:
            return std::string{"FDDI"};
        case network_interface_type::slip:
            return std::string{"SLIP"};
        case network_interface_type::ieee80216:
            return std::string{"IEEE 80216"};
        case network_interface_type::ieee802154:
            return std::string{"IEEE 802.15.4"};

        case network_interface_type::other:
        default:
            break;
    }

    return std::string{"other"};
}

std::string to_string (network_interface_status status)
{
    switch (status) {
        case network_interface_status::up:
            return std::string{"up"};
        case network_interface_status::down:
            return std::string{"down"};
        case network_interface_status::testing:
            return std::string{"testing"};
        case network_interface_status::unknown:
        default:
            break;
    }

    return std::string{"unknown"};
}

} // namespace netty
