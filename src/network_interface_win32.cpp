////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2021.06.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/i18n.hpp"
#include "pfs/netty/network_interface.hpp"
#include <vector>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>

// Must be included after winsock2.h to avoid error C2375
#include "pfs/windows.hpp"

namespace netty {

struct wsa_session
{
    bool success = true;

    wsa_session ()
    {
        WSADATA unused;
        auto rc = WSAStartup(MAKEWORD(2, 2), & unused);

        if (rc != 0) {
            //std::cerr << "WSAStartup failed: " << rc << std::endl;
            success = false;
        }
    }

    ~wsa_session ()
    {
        WSACleanup();
    }
};

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

std::vector<network_interface> fetch_interfaces (error * perr)
{
    std::vector<network_interface> result;

    IP_ADAPTER_ADDRESSES static_buffer[4];
    ULONG buffer_size = sizeof(static_buffer);
    PIP_ADAPTER_ADDRESSES addresses_ptr = & static_buffer[0];

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX
        | GAA_FLAG_SKIP_DNS_SERVER
        | GAA_FLAG_SKIP_MULTICAST;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    ULONG rc = GetAdaptersAddresses(family, flags, nullptr, addresses_ptr, & buffer_size);

    if (rc == ERROR_BUFFER_OVERFLOW) {
        addresses_ptr = static_cast<IP_ADAPTER_ADDRESSES *>(MALLOC(buffer_size));

        if (!addresses_ptr) {
            error err {
                  errc::system_error
                , tr::_("not enough memory")
                , pfs::system_error_text()
            };

            if (perr) {
                *perr = err;
                return std::vector<network_interface>{};
            } else {
                throw err;
            }
        }

        rc = GetAdaptersAddresses(family, flags, nullptr, addresses_ptr, & buffer_size);

        if (rc != NO_ERROR) {
            error err;

            if (rc == ERROR_BUFFER_OVERFLOW) {
                err = error {
                      errc::invalid_argument
                    , pfs::system_error_text()
                };
            } else {
                err = error {
                      errc::system_error
                    , pfs::system_error_text()
                };
            }

            FREE(addresses_ptr);
            addresses_ptr = nullptr;

            if (perr) {
                *perr = err;
                return std::vector<network_interface>{};
            } else {
                throw err;
            }
        }
    } else if (rc != NO_ERROR) {
        error err {
              errc::socket_error
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = err;
            return std::vector<network_interface>{};
        } else {
            throw err;
        }
    }

    result.reserve(buffer_size / sizeof(IP_ADAPTER_ADDRESSES));

    for (PIP_ADAPTER_ADDRESSES ptr = addresses_ptr; ptr; ptr = ptr->Next) {
        result.emplace_back();
        auto & iface = result.back()._data;

        {
            // use ConvertInterfaceLuidToNameW because that returns a friendlier name, though not
            // as "friendly" as FriendlyName below
            WCHAR adapter_name[IF_MAX_STRING_SIZE + 1];

            auto rc = ConvertInterfaceLuidToNameW(& ptr->Luid
                , adapter_name
                , sizeof(adapter_name)/sizeof(adapter_name[0]));

            if (rc == NO_ERROR) {
                auto nwchars = static_cast<int>(wcslen(adapter_name));
                iface.adapter_name = pfs::windows::utf8_encode(adapter_name, nwchars);
            }

            if (iface.adapter_name.empty()) {
                iface.adapter_name = ptr->AdapterName;
            }
        }

        {
            auto nwchars = static_cast<int>(wcslen(ptr->FriendlyName));
            iface.readable_name = pfs::windows::utf8_encode(ptr->FriendlyName, nwchars);
        }

        {
            auto nwchars = static_cast<int>(wcslen(ptr->Description));
            iface.description = pfs::windows::utf8_encode(ptr->Description, nwchars);
        }

        switch (ptr->IfType) {
            case IF_TYPE_ETHERNET_CSMACD:
                iface.type = network_interface_type::ethernet;
                break;

            case IF_TYPE_ISO88025_TOKENRING:
                iface.type = network_interface_type::tokenring;
                break;

            case IF_TYPE_FDDI:
                iface.type = network_interface_type::fddi;
                break;

            case IF_TYPE_PPP:
                iface.type = network_interface_type::ppp;
                break;

            case IF_TYPE_SLIP:
                iface.type = network_interface_type::slip;
                break;

            case IF_TYPE_SOFTWARE_LOOPBACK:
                iface.type = network_interface_type::loopback;
                break;

            case IF_TYPE_ATM:
                iface.type = network_interface_type::atm;
                break;

            case IF_TYPE_IEEE80211:
                iface.type = network_interface_type::ieee80211;
                break;

            case IF_TYPE_TUNNEL:
                iface.type = network_interface_type::tunnel;
                break;

            case IF_TYPE_IEEE1394:
                iface.type = network_interface_type::ieee1394;
                break;

            case IF_TYPE_IEEE80216_WMAN:
                iface.type = network_interface_type::ieee80216;
                break;

            case IF_TYPE_IEEE802154:
                iface.type = network_interface_type::ieee802154;
                break;

            case IF_TYPE_OTHER:
            default:
                iface.type = network_interface_type::other;
                break;
        }

        switch (ptr->OperStatus) {
            case IfOperStatusUp:
                iface.status = network_interface_status::up;
                break;

            case IfOperStatusDown:
            case IfOperStatusNotPresent:     // A refinement on the IfOperStatusDown state
            case IfOperStatusLowerLayerDown: // A refinement on the IfOperStatusDown state
                iface.status = network_interface_status::down;
                break;

            case IfOperStatusTesting:
                iface.status = network_interface_status::testing;
                break;

            case IfOperStatusDormant:
                iface.status = network_interface_status::pending;
                break;

            case IfOperStatusUnknown:
            default:
                iface.status = network_interface_status::unknown;
                break;
        }

        {
            if (ptr->Flags & IP_ADAPTER_DDNS_ENABLED)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::ddns_enabled);

            if (ptr->Flags & IP_ADAPTER_DHCP_ENABLED)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::dhcp_enabled);

            if (ptr->Flags & IP_ADAPTER_RECEIVE_ONLY)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::receive_only);

            if (ptr->Flags & IP_ADAPTER_NO_MULTICAST)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::no_multicast);

            if (ptr->Flags & IP_ADAPTER_IPV4_ENABLED)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::ip4_enabled);

            if (ptr->Flags & IP_ADAPTER_IPV6_ENABLED)
                iface.flags |= static_cast<decltype(iface.flags)>(network_interface_flag::ip6_enabled);
        }

        {
            if (ptr->PhysicalAddressLength > 0) {
                char buf[4] = {0, 0, 0, 0};
                std::snprintf(buf, sizeof(buf), "%02X", static_cast<int>(ptr->PhysicalAddress[0]));
                iface.hardware_address += buf;

                for (int i = 1; i < static_cast<int>(ptr->PhysicalAddressLength); i++) {
                    std::snprintf(buf, sizeof(buf), "%02X", static_cast<int>(ptr->PhysicalAddress[i]));
                    iface.hardware_address += ':';
                    iface.hardware_address += buf;
                }
            }
        }

        if (ptr->FirstUnicastAddress) {
            auto p = ptr->FirstUnicastAddress;

            while (p) {
                // TODO Implement adding unicast addresses
                p = p->Next;
            }
        }

        if (ptr->FirstAnycastAddress) {
            auto p = ptr->FirstAnycastAddress;

            while (p) {
                // TODO Implement adding anycast addresses
                p = p->Next;
            }
        }

        if (ptr->FirstMulticastAddress) {
            auto p = ptr->FirstMulticastAddress;

            while (p) {
                // TODO Implement adding multicast addresses
                p = p->Next;
            }
        }

        if (ptr->FirstDnsServerAddress) {
            auto p = ptr->FirstDnsServerAddress;

            while (p) {
                // TODO Implement adding DNS addresses
                p = p->Next;
            }
        }

        if (ptr->FirstDnsServerAddress) {
            auto p = ptr->FirstPrefix;

            while (p) {
                // TODO Implement Adapter Prefix entries
                p = p->Next;
            }
        }

        iface.mtu = ptr->Mtu;
        iface.ip4_index = ptr->IfIndex;
        iface.ip6_index = ptr->Ipv6IfIndex;
    }

    return result;
}

} // namespace netty
