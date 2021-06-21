////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/mtu.hpp"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <cstring>
#include <unistd.h>

namespace pfs {
namespace net {

//
// See `man 7 netdevice`.
// Man page implicitly says that get the MTU (Maximum Transfer Unit) of a device
// is not a privileged operation.
//
/**
 * Returns MTU (Maximum Transfer Unit) value of a device specified
 * by @a interface, or @c -1 if error occured. In last case @c ec will be set to
 * appropriate error code:
 *     * @c errc::permissions_denied if underlying system call(s) need specific
 *       privileges;
 *     * @c errc::name_too_long if @a interface name is too long than allowed by
 *       underlying system call(s);
 *     * @c errc::device_not_found if @a interface specifies bad device;
 *     * @c errc::system_error if system specific call returns error,
 *       check @c errno value.
 *
 */
DLL_API int mtu (std::string const & interface, std::error_code & ec)
{
    struct ifreq ifr;

    if (interface.size() >= sizeof(ifr.ifr_name)) {
        ec = make_error_code(errc::name_too_long);
        return -1;
    }

    std::memcpy(ifr.ifr_name, interface.c_str(), interface.size());
    ifr.ifr_name[interface.size()] = '\0';

    auto s = socket(PF_INET, SOCK_DGRAM, 0); //IPPROTO_IP;

    if (s < 0) {
        ec = make_error_code(errc::check_errno);
        return -1;
    }

    if (ioctl(s, SIOCGIFMTU, & ifr) < 0) {
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

        close(s);
        return -1;
    }

    close(s);
    return ifr.ifr_mtu;
}

}} // namespace pfs::net
