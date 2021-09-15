////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/network_interface.hpp"
#include <iostream>
#include <string>
#include <cerrno>

int main (int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "Too few arguments"
            << "\nRun `" << argv[0] << " <interface>`"
#if (defined(__linux) || defined(__linux__))
            << "\n\nAvailable interfaces can be listed by command `ip a`"
#elif _MSC_VER
            << "\n\nAvailable interfaces can be listed by command `netsh interface ipv4 show subinterfaces`"
#endif
            << std::endl;
        return EXIT_FAILURE;
    }

    std::error_code ec;
    auto interface_name = argv[1];

    auto ifaces = pfs::net::fetch_interfaces(ec, [& interface_name] (pfs::net::network_interface const & iface) -> bool {
        std::cout << "Adapter name: "  << iface.adapter_name() << "\n";
        std::cout << "\tReadable name: " << iface.readable_name() << "\n";
        std::cout << "\tDescription  : " << iface.description() << "\n";
        std::cout << "\tHW address   : " << iface.hardware_address() << "\n";
        std::cout << "\tType         : " << std::to_string(iface.type()) << "\n";
        std::cout << "\tStatus       : " << std::to_string(iface.status()) << "\n";
        std::cout << "\tMTU          : " << iface.mtu() << "\n";
        std::cout << "\tIPv4 enabled : " << std::boolalpha << iface.is_flag(pfs::net::network_interface_flag::ip4_enabled) << "\n";
        std::cout << "\tIPv6 enabled : " << std::boolalpha << iface.is_flag(pfs::net::network_interface_flag::ip6_enabled) << "\n";
        std::cout << "\tIPv4 interface index: " << iface.ip4_index() << "\n";
        std::cout << "\tIPv6 interface index: " << iface.ip6_index() << "\n";
        std::cout << "\tIPv4         : " << std::to_string(iface.ip4_addr()) << "\n";
        std::cout << "\n\n";
        return interface_name == iface.readable_name();
    });

    if (ec) {
        std::cerr << "ERROR: failed to get MTU value for interface ["
            << interface_name << "]: "
            << ec.message() << std::endl;

        if (ec == pfs::net::make_error_code(pfs::net::errc::system_error)) {
            std::cerr << "ERROR: errno: " << errno << std::endl;
        }
        return EXIT_FAILURE;
    }

    if (ifaces.empty()) {
        std::cerr << "ERROR: interface ["
            << interface_name << "]: not found" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "MTU value for interface ["
        << interface_name << "]: "
        << pfs::net::mtu(interface_name, ec) << std::endl;

    return EXIT_SUCCESS;
}
