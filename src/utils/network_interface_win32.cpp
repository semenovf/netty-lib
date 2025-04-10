////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2021.06.24 Initial version.
//      2024.04.08 Moved to `utils` namespace.
////////////////////////////////////////////////////////////////////////////////
#include "network_interface.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/numeric_cast.hpp"
#include <vector>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

// Must be included after winsock2.h to avoid error C2375
#include "pfs/windows.hpp"

namespace netty {
namespace utils {

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

void foreach_interface (std::function<void(network_interface const &)> visitor, error * perr)
{
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
            pfs::throw_or(perr, error {
                  make_error_code(pfs::errc::system_error)
                , tr::_("not enough memory")
            });

            return;
        }

        rc = GetAdaptersAddresses(family, flags, nullptr, addresses_ptr, & buffer_size);

        if (rc != NO_ERROR) {
            error err;

            if (rc == ERROR_BUFFER_OVERFLOW) {
                err = error {
                      make_error_code(std::errc::invalid_argument)
                    , pfs::system_error_text()
                };
            } else {
                err = error {
                      make_error_code(pfs::errc::system_error)
                    , pfs::system_error_text()
                };
            }

            FREE(addresses_ptr);
            addresses_ptr = nullptr;

            throw_or (perr, std::move(err));
            return;
        }
    } else if (rc != NO_ERROR) {
        throw_or (perr, error {
              make_error_code(pfs::errc::system_error)
            , pfs::system_error_text()
        });

        return;
    }

    for (PIP_ADAPTER_ADDRESSES ptr = addresses_ptr; ptr; ptr = ptr->Next) {
        network_interface iface;

        {
            // use ConvertInterfaceLuidToNameW because that returns a friendlier name, though not
            // as "friendly" as FriendlyName below
            WCHAR adapter_name[IF_MAX_STRING_SIZE + 1];

            auto rc = ConvertInterfaceLuidToNameW(& ptr->Luid
                , adapter_name
                , sizeof(adapter_name)/sizeof(adapter_name[0]));

            if (rc == NO_ERROR) {
                auto nwchars = static_cast<int>(wcslen(adapter_name));
                iface._data.adapter_name = pfs::windows::utf8_encode(adapter_name, nwchars);
            }

            if (iface._data.adapter_name.empty()) {
                iface._data.adapter_name = ptr->AdapterName;
            }
        }

        {
            auto nwchars = static_cast<int>(wcslen(ptr->FriendlyName));
            iface._data.readable_name = pfs::windows::utf8_encode(ptr->FriendlyName, nwchars);
        }

        {
            auto nwchars = static_cast<int>(wcslen(ptr->Description));
            iface._data.description = pfs::windows::utf8_encode(ptr->Description, nwchars);
        }

        switch (ptr->IfType) {
        case IF_TYPE_ETHERNET_CSMACD:
            iface._data.type = network_interface_type::ethernet;
            break;

        case IF_TYPE_ISO88025_TOKENRING:
            iface._data.type = network_interface_type::tokenring;
            break;

        case IF_TYPE_FDDI:
            iface._data.type = network_interface_type::fddi;
            break;

        case IF_TYPE_PPP:
            iface._data.type = network_interface_type::ppp;
            break;

        case IF_TYPE_SLIP:
            iface._data.type = network_interface_type::slip;
            break;

        case IF_TYPE_SOFTWARE_LOOPBACK:
            iface._data.type = network_interface_type::loopback;
            break;

        case IF_TYPE_ATM:
            iface._data.type = network_interface_type::atm;
            break;

        case IF_TYPE_IEEE80211:
            iface._data.type = network_interface_type::ieee80211;
            break;

        case IF_TYPE_TUNNEL:
            iface._data.type = network_interface_type::tunnel;
            break;

        case IF_TYPE_IEEE1394:
            iface._data.type = network_interface_type::ieee1394;
            break;

        case IF_TYPE_IEEE80216_WMAN:
            iface._data.type = network_interface_type::ieee80216;
            break;

        case IF_TYPE_IEEE802154:
            iface._data.type = network_interface_type::ieee802154;
            break;

        case IF_TYPE_OTHER:
        default:
            iface._data.type = network_interface_type::other;
            break;
        }

        switch (ptr->OperStatus) {
        case IfOperStatusUp:
            iface._data.status = network_interface_status::up;
            break;

        case IfOperStatusDown:
        case IfOperStatusNotPresent:     // A refinement on the IfOperStatusDown state
        case IfOperStatusLowerLayerDown: // A refinement on the IfOperStatusDown state
            iface._data.status = network_interface_status::down;
            break;

        case IfOperStatusTesting:
            iface._data.status = network_interface_status::testing;
            break;

        case IfOperStatusDormant:
            iface._data.status = network_interface_status::pending;
            break;

        case IfOperStatusUnknown:
        default:
            iface._data.status = network_interface_status::unknown;
            break;
        }

        {
            if (ptr->Flags & IP_ADAPTER_DDNS_ENABLED)
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::ddns_enabled);

            if (ptr->Flags & IP_ADAPTER_DHCP_ENABLED)
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::dhcp_enabled);

            if (ptr->Flags & IP_ADAPTER_RECEIVE_ONLY)
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::receive_only);

            if (!(ptr->Flags & IP_ADAPTER_NO_MULTICAST))
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::multicast);

            if (ptr->Flags & IP_ADAPTER_IPV4_ENABLED)
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::ip4_enabled);

            if (ptr->Flags & IP_ADAPTER_IPV6_ENABLED)
                iface._data.flags |= static_cast<decltype(iface._data.flags)>(network_interface_flag::ip6_enabled);
        }

        {
            if (ptr->PhysicalAddressLength > 0) {
                char buf[4] = {0, 0, 0, 0};
                std::snprintf(buf, sizeof(buf), "%02X", static_cast<int>(ptr->PhysicalAddress[0]));
                iface._data.hardware_address += buf;

                for (int i = 1; i < static_cast<int>(ptr->PhysicalAddressLength); i++) {
                    std::snprintf(buf, sizeof(buf), "%02X", static_cast<int>(ptr->PhysicalAddress[i]));
                    iface._data.hardware_address += ':';
                    iface._data.hardware_address += buf;
                }
            }
        }

        if (ptr->FirstUnicastAddress) {
            auto p = ptr->FirstUnicastAddress;

            while (p) {
                SOCKADDR const * psock_addr = p->Address.lpSockaddr;

                switch (psock_addr->sa_family) {
                    case AF_INET: {
                        if (iface._data.ip4 == inet4_addr{}) {
                            auto addr = pfs::numeric_cast<std::uint32_t>(reinterpret_cast<sockaddr_in const *>(psock_addr)->sin_addr.S_un.S_addr);
                            addr = pfs::to_native_order(addr);
                            iface._data.ip4 = inet4_addr(addr);
                        }

                        break;
                    }

                    case AF_INET6: {
                        auto pin6_addr = reinterpret_cast<sockaddr_in6 const *>(psock_addr);
                        char buf[INET6_ADDRSTRLEN];
                        iface._data.ip6 = inet_ntop(AF_INET6, & pin6_addr->sin6_addr, buf, INET6_ADDRSTRLEN);
                        break;
                    }
                }

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

        iface._data.mtu = ptr->Mtu;
        // iface._data.ip4_index = ptr->IfIndex;
        // iface._data.ip6_index = ptr->Ipv6IfIndex;

        visitor(iface);
    }
}

}} // namespace netty::utils
