////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/utils/network_interface.hpp"
#include "pfs/fmt.hpp"

int main (int , char *[])
{
    auto ifaces = netty::utils::fetch_interfaces([] (netty::utils::network_interface const & iface) -> bool {
        fmt::println("Adapter name: {}\n"
            "\tReadable name: {}\n"
            "\tDescription  : {}\n"
            "\tHW address   : {}\n"
            "\tType         : {}\n"
            "\tStatus       : {}\n"
            "\tMTU          : {}\n"
            "\tIPv4 enabled : {}\n"// << std::boolalpha <<  << "\n";
            "\tIPv6 enabled : {}\n"// << std::boolalpha <<  << "\n";
            "\tIPv4         : {}\n"
            "\tIPv6         : {}\n"
            "\tMulticast    : {}\n\n"
            , iface.adapter_name()
            , iface.readable_name()
            , iface.description()
            , iface.hardware_address()
            , to_string(iface.type())
            , to_string(iface.status())
            , iface.mtu()
            , iface.is_flag_on(netty::utils::network_interface_flag::ip4_enabled)
            , iface.is_flag_on(netty::utils::network_interface_flag::ip6_enabled)
            , to_string(iface.ip4_addr())
            , iface.ip6_addr()
            , iface.is_flag_on(netty::utils::network_interface_flag::multicast));
        return true;
    });

    return EXIT_SUCCESS;
}
