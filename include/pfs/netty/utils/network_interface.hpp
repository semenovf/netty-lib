////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.24 Initial version.
//      2024.04.08 Moved to `utils` namespace.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace netty {
namespace utils {

using string_type = std::string;

enum class network_interface_type {
      other      // Some other type of network interface
    , ethernet   // An Ethernet network interface
    , tokenring  // A token ring network interface
    , ppp        // A PPP network interface
    , loopback   // A software loopback network interface
    , atm        // An ATM network interface
    , ieee80211  // An IEEE 802.11 wireless network interface
    , tunnel     // A tunnel type encapsulation network interface
    , ieee1394   // An IEEE 1394 (Firewire) high performance serial bus network interface
    , fddi       // FDDI
    , slip       // Generic SLIP (Serial Line Internet Protocol)
    , ieee80216  // An IEEE 80216
    , ieee802154 // IEEE 802.15.4 WPAN interface
};

enum class network_interface_status
{
      unknown  // The operational status of the interface is unknown.
    , up       // The interface is up and able to pass packets.
    , down     // The interface is down and not in a condition to pass packets.
    , testing  // The interface is in testing mode.
    , pending  // For Windows: the interface is not actually in a condition
               // to pass packets (it is not up), but is in a pending state,
               // waiting for some external event. For on-demand interfaces,
               // this new state identifies the situation where the interface
               // is waiting for events to place it in the `up` state.
};

enum class network_interface_flag {
      ddns_enabled = 0x0001
    , dhcp_enabled = 0x0004
    , receive_only = 0x0008
    , multicast    = 0x0010
    , ip4_enabled  = 0x0080
    , ip6_enabled  = 0x0100
};

struct network_interface_data
{
    // The index of the IPv4 interface with which these addresses
    // are associated. On Windows Server 2003 and Windows XP,
    // this member is zero if IPv4 is not available on the interface.
    // int ip4_index {0};

    // The interface index for the IPv6 IP address. This member is zero
    // if IPv6 is not available on the interface.
    // NOTE: This structure member is only available on Windows XP with SP1 and later.
    // int ip6_index {0};

    // The maximum transmission unit (MTU) size, in bytes.
    std::uint32_t mtu {0};

    // IPv4 address associated with interface.
    // For Windows contains first unicast address if any.
    inet4_addr ip4;

    // IPv6 address associated with interface
    string_type ip6;

    string_type adapter_name;

    // A user-friendly name for the adapter.
    string_type readable_name;

    // A description for the adapter.
    string_type description;

    // Hardware address.
    // On Ethernet interfaces, this will be a MAC address in string representation,
    // separated by colons.
    string_type hardware_address;

    // The interface type as defined by the Internet Assigned Names Authority (IANA).
    network_interface_type type {network_interface_type::other};

    network_interface_status status {network_interface_status::unknown};

    std::uint32_t flags {0};
};

class network_interface
{
private:
    network_interface_data _data;

public:
    network_interface () {}
    network_interface (network_interface const & ) = default;
    network_interface (network_interface && ) = default;
    network_interface & operator = (network_interface const & ) = default;
    network_interface & operator = (network_interface && ) = default;

    inet4_addr ip4_addr () const noexcept
    {
        return _data.ip4;
    }

    string_type ip6_addr () const noexcept
    {
        return _data.ip6;
    }

    // int ip4_index () const noexcept
    // {
    //     return _data.ip4_index;
    // }

    // int ip6_index () const noexcept
    // {
    //     return _data.ip6_index;
    // }

    std::uint32_t mtu () const noexcept
    {
        return _data.mtu;
    }

    string_type adapter_name () const noexcept
    {
        return _data.adapter_name;
    }

    string_type readable_name () const noexcept
    {
        return _data.readable_name;
    }

    string_type hardware_address () const noexcept
    {
        return _data.hardware_address;
    }

    string_type description () const noexcept
    {
        return _data.description;
    }

    network_interface_type type () const noexcept
    {
        return _data.type;
    }

    network_interface_status status () const noexcept
    {
        return _data.status;
    }

    /**
     * Checks the interface is up and able to pass packets.
     */
    bool is_up () const noexcept
    {
        return _data.status == network_interface_status::up;
    }

    /**
     * Checks the interface is down and not in a condition to pass packets.
     */
    bool is_down () const noexcept
    {
        return _data.status == network_interface_status::down;
    }

    bool is_flag_on (network_interface_flag flag) const noexcept
    {
        return _data.flags & static_cast<decltype(_data.flags)>(flag);
    }

    bool is_loopback () const noexcept
    {
        return _data.type == network_interface_type::loopback;
    }

    friend NETTY__EXPORT
    void foreach_interface (std::function<void(network_interface const &)> visitor, error * perr);
};

void foreach_interface (std::function<void(network_interface const &)> visitor
    , error * perr = nullptr);

/**
 * @brief Fetch network interfaces.
 *
 * @param [out] ec error code (if any errors occured):
 *         - std::errc::not_enough_memory
 *         - std::errc::value_too_large
 *         - pfs::errc::system_error
 * @return Network interfaces
 */

template <typename Visitor>
std::vector<network_interface> fetch_interfaces (Visitor && visitor, error * perr = nullptr)
{
    error err;
    std::vector<network_interface> result;

    foreach_interface([& result, & visitor] (network_interface const & iface) {
        if (visitor(iface))
            result.push_back(iface);
    }, & err);

    if (err) {
        pfs::throw_or(perr, std::move(err));
        return std::vector<network_interface>{};
    }

    return result;
}

inline std::vector<network_interface> fetch_all_interfaces (error * perr = nullptr)
{
    return fetch_interfaces([] (network_interface const &) { return true; }, perr);
}

enum class usename {
      adapter
    , readable
};

inline std::vector<network_interface> fetch_interfaces_by_name (usename un
    , std::string const & interface_name
    , error * perr = nullptr)
{
    auto ifaces = fetch_interfaces([un, & interface_name] (
            network_interface const & iface) -> bool {
        return un == usename::readable
            ? interface_name == iface.readable_name()
            : interface_name == iface.adapter_name();
    }, perr);

    return ifaces;
}

NETTY__EXPORT std::string to_string (network_interface_type type);
NETTY__EXPORT std::string to_string (network_interface_status status);

}} // namespace netty::utils
