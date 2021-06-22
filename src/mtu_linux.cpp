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
//      2021.06.21 Initial version
//      2021.06.21 Use 2-way to get MTU: mtu_alternative0() and mtu_alternative1()
////////////////////////////////////////////////////////////////////////////////
#if !(defined(__linux) || defined(__linux__))
#   error "Expected Linux"
#endif

#include "pfs/net/mtu.hpp"
#include <cstring>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>

namespace pfs {
namespace net {

//
// See `man 7 netdevice`.
// Man page implicitly says that get the MTU (Maximum Transfer Unit) of a device
// is not a privileged operation.
//

static int mtu_alternative0 (std::string const & interface, std::error_code & ec)
{
    struct ifreq ifr;

    if (interface.size() >= sizeof(ifr.ifr_name)) {
        ec = make_error_code(errc::name_too_long);
        return -1;
    }

    std::memcpy(ifr.ifr_name, interface.c_str(), interface.size());
    ifr.ifr_name[interface.size()] = '\0';

    auto fd = socket(PF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        ec = make_error_code(errc::check_errno);
        return -1;
    }

    if (ioctl(fd, SIOCGIFMTU, & ifr) < 0) {
        switch (errno) {
            case EPERM:
                ec = make_error_code(errc::permissions_denied);
                break;
            case ENODEV:
                ec = make_error_code(errc::device_not_found);
                break;
            default:
                ec = make_error_code(errc::check_errno);
                break;
        }

        close(fd);
        return -1;
    }

    close(fd);
    return ifr.ifr_mtu;
}

static int mtu_alternative1 (std::string const & interface, std::error_code & ec)
{
    std::string path {"/sys/class/net/"};

    path += interface;
    path += "/mtu";

    // Check if specified directory exists
    if (::access(path.c_str(), F_OK) != 0) {
        ec = make_error_code(errc::device_not_found);
        return -1;
    }

    // Unable read access for file
    if (::access(path.c_str(), R_OK) != 0) {
        ec = make_error_code(errc::permissions_denied);
        return -1;
    }

    std::ifstream ifs(path);

    if (!ifs.is_open()) {
        ec = make_error_code(errc::permissions_denied);
        return -1;
    }

    int result = -1;
    ifs >> result;

    return result;
}

DLL_API int mtu (std::string const & interface, std::error_code & ec) noexcept
{
    auto result = -1;//mtu_alternative0(interface, ec);

    if (result < 0)
        result = mtu_alternative1(interface, ec);

    return result;
}

}} // namespace pfs::net
