////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/mtu.hpp"
#include <iostream>
#include <cerrno>

int main (int argc, char * argv[])
{
    if (argc < 2) {
        std::cerr << "Too few arguments"
            << "\nRun `" << argv[0] << " <interface>`"
            << "\n\nAvailable interfaces can be listed by command `ip a`"
            << std::endl;
        return EXIT_FAILURE;
    }

    std::error_code ec;
    auto interface = std::string(argv[1]);
    auto mtu = pfs::net::mtu(interface, ec);

    if (ec) {
        std::cerr << "ERROR: failed to get MTU value for interface ["
            << interface << "]: "
            << ec.message() << std::endl;

        if (ec == pfs::net::make_error_code(pfs::net::errc::check_errno)) {
            std::cerr << "ERROR: errno: " << errno << std::endl;
        }
        return EXIT_FAILURE;
    }

    std::cout << "MTU value for interface ["
        << interface << "]: "
        << mtu << std::endl;

//     using exit_status = modulus2::exit_status;
//     pfs::iostream_logger logger;
//     modulus2::dispatcher d{logger};
//     timer_quit_plugin quit_plugin {2};
//
//     d.attach_plugin(quit_plugin);
//
//     if (d.exec() == exit_status::success) {
//         std::cout << "Exit successfully\n";
//     }
//
    return EXIT_SUCCESS;
}
