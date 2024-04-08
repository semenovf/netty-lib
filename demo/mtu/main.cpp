////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.21 Initial version (netty-lib).
//      2024.04.08 Refactored.
////////////////////////////////////////////////////////////////////////////////
#include "netty/utils/network_interface.hpp"
#include "pfs/argvapi.hpp"
#include "pfs/fmt.hpp"
#include "pfs/log.hpp"
#include <string>

void printUsage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    std::FILE * out = stdout;

    if (!errorString.empty()) {
        out = stderr;
        fmt::println(out, "Error: {}", errorString);
    }

    fmt::println(out, "Usage:\n\n"
        "{0} --help | -h\n"
        "\tPrint this help and exit\n\n"
        "{0} [INTERFACE]"
        "\tnPrint MTU for specified/all interface\n"
#if (defined(__linux) || defined(__linux__))
        "\tnAvailable interfaces can be listed by command `ip a`"
#elif _MSC_VER
        "\tAvailable interfaces can be listed by command `netsh interface ipv4 show subinterfaces`"
#endif
        , programName);
}


int main (int argc, char * argv[])
{
    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    std::string interfaceName;

    while (commandLineIterator.has_more()) {
        auto x = commandLineIterator.next();

        if (x.is_option("help") || x.is_option("h")) {
            printUsage(programName);
            return EXIT_SUCCESS;
        } else if (x.has_arg()) {
            using pfs::to_string;
            interfaceName = to_string(x.arg());
            break;
        }
    }

    if (interfaceName.empty()) {
        auto ifaces = netty::utils::fetch_interfaces([] (netty::utils::network_interface const & iface) -> bool {
            fmt::println("MTU value for interface [{}]: {}", iface.adapter_name(), iface.mtu());
            return true;
        });
    } else {
        using netty::utils::fetch_interfaces_by_name;
        auto ifaces = fetch_interfaces_by_name(netty::utils::usename::adapter, interfaceName);

        if (ifaces.empty()) {
            LOGE("", "interface [{}] not found", interfaceName);
            return EXIT_FAILURE;
        }

        for (auto const & iface: ifaces)
            fmt::println("MTU value for interface [{}]: {}", iface.adapter_name(), iface.mtu());
    }

    return EXIT_SUCCESS;
}
