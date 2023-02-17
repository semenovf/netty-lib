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
#include <utility>

#if NETTY__LIBMNL_ENABLED
#   include <libmnl/libmnl.h>
#endif

namespace netty {
namespace linux_os {

enum class callback_result {
#if NETTY__LIBMNL_ENABLED
      error = MNL_CB_ERROR
    , stop  = MNL_CB_STOP
    , ok    = MNL_CB_OK
#else
      error = -1
    , stop  =  0
    , ok    =  1
#endif
};

//     EXPORT_SYMBOL(mnl_attr_get_type);
// uint16_t mnl_attr_get_type(const struct nlattr *attr)
// {
// 	return attr->nla_type & NLA_TYPE_MASK;
// }
//
// EXPORT_SYMBOL(mnl_attr_type_valid);
// int mnl_attr_type_valid(const struct nlattr *attr, uint16_t max)
// {
// 	if (mnl_attr_get_type(attr) > max) {
// 		errno = EOPNOTSUPP;
// 		return -1;
// 	}
// 	return 1;
// }


template <typename IfMsgType>
IfMsgType const * cast_payload (nlmsghdr const * nlh)
{
#if NETTY__LIBMNL_ENABLED
    return static_cast<IfMsgType const *>(mnl_nlmsg_get_payload(nlh));
#else
    return static_cast<IfMsgType const *>(NLMSG_DATA(nlh));
#endif
}

template <typename AttrType>
std::pair<AttrType const *, int> get_attr_ptr (nlattr const * pattr)
{
#if NETTY__LIBMNL_ENABLED
    return std::make_pair(static_cast<AttrType const *>(mnl_attr_get_payload(pattr))
        , static_cast<int>(mnl_attr_get_payload_len(pattr)));
#else
    return std::make_pair(
          reinterpret_cast<AttrType const *>(reinterpret_cast<char const *>(pattr) + NLA_HDRLEN)
        , static_cast<int>(pattr->nla_len - NLA_HDRLEN));
#endif
}

static int attr_type (nlattr const * pattr)
{
#if NETTY__LIBMNL_ENABLED
    return mnl_attr_get_type(pattr);
#else
    return (pattr->nla_type & NLA_TYPE_MASK);
#endif
}

inline nlattr const * begin_attr (nlmsghdr const * nlh, std::size_t offset)
{
#if NETTY__LIBMNL_ENABLED
    return static_cast<nlattr const *>(mnl_nlmsg_get_payload_offset(nlh, offset));
#else
    return reinterpret_cast<nlattr const *>(
        reinterpret_cast<char const *>(nlh) + NLMSG_HDRLEN + NLMSG_ALIGN(offset));
#endif
}

inline bool has_more_attrs (nlmsghdr const * nlh, nlattr const * curr_attr)
{
#if NETTY__LIBMNL_ENABLED
    return mnl_attr_ok(curr_attr, static_cast<char const *>(mnl_nlmsg_get_payload_tail(nlh))
        - reinterpret_cast<char const *>(curr_attr));
#else
    auto tail_ptr = reinterpret_cast<char const *>(nlh) + NLMSG_ALIGN(nlh->nlmsg_len);
    auto len = tail_ptr - reinterpret_cast<char const *>(curr_attr);
    bool success = len >= sizeof(nlattr)
        && curr_attr->nla_len >= sizeof(nlattr)
        && curr_attr->nla_len <= len;

    return success;
#endif
}

inline nlattr const * next_attr (nlattr const * curr_attr)
{
#if NETTY__LIBMNL_ENABLED
    return mnl_attr_next(curr_attr);
#else
    return reinterpret_cast<nlattr const *>(reinterpret_cast<char const *>(curr_attr)
        + NLA_ALIGN(curr_attr->nla_len));
#endif
}

static callback_result foreach_attr (nlmsghdr const * nlh, std::size_t offset
    , std::function<callback_result(nlattr const *, netlink_monitor *)> visitor
    , netlink_monitor * pmonitor)
{
    for (nlattr const * attr = begin_attr(nlh, offset); has_more_attrs(nlh, attr)
            ; attr = next_attr(attr)) {

        auto ret = visitor(attr, pmonitor);

        if (ret != callback_result::ok)
            return ret;
    }

    return callback_result::ok;
}

// TODO Use this code as an example to implement attributes validation
//
// static int parse_attributes (nlattr const * attr, void * data)
// {
//     auto tb = static_cast<nlattr const **>(data);
//
// #if NETTY__LIBMNL_ENABLED
//     // Skip unsupported attribute in user-space
//     if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
//         return MNL_CB_OK;
//
//     auto type = mnl_attr_get_type(attr);
// #else
//     auto type = (attr->nla_type & NLA_TYPE_MASK);
//
//     // Skip unsupported attribute in user-space
//     if (type > IFLA_MAX)
//         return MNL_CB_OK;
// #endif
//
//     switch(type) {
//         case IFLA_MTU:
//             if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0)
//                 return MNL_CB_ERROR;
//             break;
//
//         case IFLA_IFNAME:
//             if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0)
//                 return MNL_CB_ERROR;
//             break;
//
//         case IFA_ADDRESS:
//             if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0)
//                 return MNL_CB_ERROR;
//             break;
//     }
//
//     tb[type] = attr;
//     return MNL_CB_OK;
// }

static callback_result parser_callback (nlmsghdr const * nlh, netlink_monitor * pmonitor)
{
    auto this_monitor = static_cast<netlink_monitor *>(pmonitor);
    auto ret = callback_result::ok;

    switch (nlh->nlmsg_type) {
        case RTM_NEWADDR:
        case RTM_DELADDR: {
            auto ifa = cast_payload<ifaddrmsg>(nlh);

            auto nlmsg_type = nlh->nlmsg_type;

            ret = foreach_attr(nlh, sizeof(*ifa), [ifa, nlmsg_type] (
                    nlattr const * pattr, netlink_monitor * pmonitor) {

                auto attrtype = attr_type(pattr);

                switch (attrtype) {
                    case IFA_ADDRESS: {
                        auto value = get_attr_ptr<std::uint32_t>(pattr);

                        if (ifa->ifa_family == AF_INET) {
                            auto ip = inet4_addr{pfs::to_native_order(*value.first)};

                            if (nlmsg_type == RTM_NEWADDR) {
                                if (pmonitor->inet4_addr_added)
                                    pmonitor->inet4_addr_added(ip, ifa->ifa_index);
                            } else {
                                if (pmonitor->inet4_addr_removed)
                                    pmonitor->inet4_addr_removed(ip, ifa->ifa_index);
                            }
                        } else if (ifa->ifa_family == AF_INET6) {
                            // TODO Not implemented yet (cause: inet6_addr not implemented too)
                        }

                        break;
                    }

                    default:
                        break;
                }

                return callback_result::ok;
            }, this_monitor);

            break;
        }

        case RTM_NEWLINK:
        case RTM_DELLINK: {
            auto ifm = cast_payload<ifinfomsg>(nlh);

            netlink_attributes attrs;

            if (ifm->ifi_flags & IFF_RUNNING)
                attrs.running = true;
            else
                attrs.running = false;

            if (ifm->ifi_flags & IFF_UP)
                attrs.up = true;
            else
                attrs.up = false;

            ret = foreach_attr(nlh, sizeof(*ifm), [& attrs] (nlattr const * pattr, netlink_monitor *) {
                auto attrtype = attr_type(pattr);

                switch (attrtype) {
                    case IFLA_MTU: {
                        auto value = get_attr_ptr<std::uint32_t>(pattr);
                        attrs.mtu = *value.first;
                        break;
                    }

                    case IFLA_IFNAME: {
                        auto value = get_attr_ptr<char>(pattr);
                        attrs.iface_name = std::string(value.first, value.second);
                        break;
                    }

                    default:
                        break;
                }

                return callback_result::ok;
            }, this_monitor);

            if (this_monitor->attrs_ready)
                this_monitor->attrs_ready(attrs);

            break;
        }

        default:
            break;
    }

    return ret;
}

#if NETTY__LIBMNL_ENABLED
static int data_cb (nlmsghdr const * nlh, void * pmonitor)
{
    return static_cast<int>(parser_callback(nlh, static_cast<netlink_monitor *>(pmonitor)));
}
#endif

static callback_result parse (void const * buf, std::size_t len
    , callback_result (* callback) (nlmsghdr const *, netlink_monitor *), netlink_monitor * pmonitor)
{
#if NETTY__LIBMNL_ENABLED
    (void)callback;

    unsigned int seq = 0;
    unsigned int portid = 0;
    auto rc = mnl_cb_run(buf, len, seq, portid, & data_cb, pmonitor);
    return static_cast<callback_result>(rc);
#else
    auto ret = callback_result::ok;
    auto nlh = static_cast<nlmsghdr const *>(buf);

    while (NLMSG_OK(nlh, len)) {
        if (nlh->nlmsg_flags & NLM_F_DUMP_INTR) {
            errno = EINTR;
            return callback_result::error;
        }

        if (nlh->nlmsg_type >= NLMSG_MIN_TYPE) {
            if (callback) {
                ret = callback(nlh, pmonitor);

                if (ret != callback_result::ok)
                    return ret;
            }
        } else {
            switch (nlh->nlmsg_type) {
                case NLMSG_ERROR: {
                    auto err = cast_payload<nlmsgerr>(nlh);

                    if (nlh->nlmsg_len < (sizeof(nlmsgerr) + NLMSG_HDRLEN)) {
                        errno = EBADMSG;
                        return callback_result::error;
                    }

                    // Netlink subsystems returns the errno value with different signess
                    if (err->error < 0)
                        errno = -err->error;
                    else
                        errno = err->error;

                    ret = err->error == 0
                        ? callback_result::stop
                        : callback_result::error;
                    break;
                }

                case NLMSG_DONE:
                    ret = callback_result::stop;
                    break;

                case NLMSG_NOOP:
                case NLMSG_OVERRUN:
                default:
                    ret = callback_result::ok;
                    break;
            }

            if (ret != callback_result::ok)
                return ret;
        }

        nlh = NLMSG_NEXT(nlh, len);
    }

    return ret;
#endif
}

netlink_monitor::netlink_monitor ()
    : _nls(netlink_socket::type_enum::route)
{
    _poller.on_error = [this] (netlink_socket::native_type, std::string const & s) {
        this->on_error(s);
    };

    _poller.ready_read = [this] (netlink_socket::native_type) {
#if NETTY__LIBMNL_ENABLED
        char buf[MNL_SOCKET_BUFFER_SIZE];
#else
        char buf[8192]; // Max size for MNL_SOCKET_BUFFER_SIZE
#endif
        auto n = _nls.recv(buf, sizeof(buf));

        if (n > 0) {
#if NETTY__LIBMNL_ENABLED
            auto rc = parse(buf, n, nullptr, this);
#else
            auto rc = parse(buf, n, & parser_callback, this);
#endif
            if (rc == callback_result::error) {
                throw error {
                      errc::socket_error
                    , tr::_("Netlink parse data failure")
                    , pfs::system_error_text()
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
