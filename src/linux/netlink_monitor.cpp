////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/i18n.hpp"
#include "pfs/endian.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/linux/netlink_monitor.hpp"
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <libmnl/libmnl.h>

namespace netty {
namespace linux_os {

static int parse_attributes (nlattr const * attr, void * data)
{
    auto tb = static_cast<nlattr const **>(data);
    auto type = mnl_attr_get_type(attr);

    // Skip unsupported attribute in user-space
    if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
        return MNL_CB_OK;

    switch(type) {
        case IFLA_MTU:
            if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0)
                return MNL_CB_ERROR;
            break;

        case IFLA_IFNAME:
            if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0)
                return MNL_CB_ERROR;
            break;

        case IFA_ADDRESS:
            if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0)
                return MNL_CB_ERROR;
            break;
    }

    tb[type] = attr;
    return MNL_CB_OK;
}

static int data_cb (nlmsghdr const * nlh, void * pmonitor)
{
    auto this_monitor = static_cast<netlink_monitor *>(pmonitor);
    nlattr * tb[IFLA_MAX + 1] = {};
    std::memset(& tb[0], 0, sizeof(nlattr *) * (IFLA_MAX + 1));

    switch (nlh->nlmsg_type) {
        case RTM_NEWADDR:
        case RTM_DELADDR: {
            auto ifa = static_cast<ifaddrmsg *>(mnl_nlmsg_get_payload(nlh));
            mnl_attr_parse(nlh, sizeof(*ifa), parse_attributes, tb);

            if (tb[IFA_ADDRESS]) {
                void * addr = mnl_attr_get_payload(tb[IFA_ADDRESS]);

                if (ifa->ifa_family == AF_INET) {
                    auto ip = inet4_addr{pfs::to_native_order(*static_cast<std::uint32_t*>(addr))};

                    if (nlh->nlmsg_type == RTM_NEWADDR) {
                        if (this_monitor->inet4_addr_added)
                            this_monitor->inet4_addr_added(ip, ifa->ifa_index);
                    } else {
                        if (this_monitor->inet4_addr_removed)
                            this_monitor->inet4_addr_removed(ip, ifa->ifa_index);
                    }
                } else if (ifa->ifa_family == AF_INET6) {
                    // TODO Not implemented yet (cause: inet6_addr not implemented too)
                }
            }
            break;
        }

        case RTM_NEWLINK:
        case RTM_DELLINK: {
            auto ifm = static_cast<ifinfomsg const *>(mnl_nlmsg_get_payload(nlh));
            netlink_attributes attrs;

            if (ifm->ifi_flags & IFF_RUNNING)
                attrs.running = true;
            else
                attrs.running = false;

            if (ifm->ifi_flags & IFF_UP)
                attrs.up = true;
            else
                attrs.up = false;

            mnl_attr_parse(nlh, sizeof(*ifm), & parse_attributes, tb);

            if (tb[IFLA_MTU])
                attrs.mtu = mnl_attr_get_u32(tb[IFLA_MTU]);

            if (tb[IFLA_IFNAME])
                attrs.iface_name = mnl_attr_get_str(tb[IFLA_IFNAME]);

            if (this_monitor->attrs_ready)
                this_monitor->attrs_ready(attrs);

            break;
        }

        default:
            break;
    }

    return MNL_CB_OK;
}

netlink_monitor::netlink_monitor ()
    : _nls(netlink_socket::type_enum::route)
{
    _poller.on_error = [this] (netlink_socket::native_type, std::string const & s) {
        this->on_error(s);
    };

    _poller.ready_read = [this] (netlink_socket::native_type) {
        char buf[MNL_SOCKET_BUFFER_SIZE];
        auto n = _nls.recv(buf, sizeof(buf));

        if (n > 0) {
            unsigned int seq = 0;
            unsigned int portid = 0;
            auto rc = mnl_cb_run(buf, n, seq, portid, & data_cb, this);

            if (rc < 0) {
                throw error {
                      errc::socket_error
                    , tr::_("Netlink parse data failure")
                };
            }
        }
    };

    _poller.add(_nls.native());
}

int netlink_monitor::poll (std::chrono::milliseconds millis, error * perr)
{
    return _poller.poll(millis, perr);
}

}} // namespace netty::linux_os


