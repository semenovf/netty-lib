////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//      * [GetIpAddrTable function (iphlpapi.h)](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getipaddrtable)
//      * [Managing Interfaces](https://learn.microsoft.com/en-us/windows/win32/iphlp/managing-interfaces)
//      * [Notifications (System Event Notification Service)](https://learn.microsoft.com/en-us/windows/win32/sens/notifications)
//
// Changelog:
//      2024.04.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netlink_monitor.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/numeric_cast.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// Must be included after winsock2.h to avoid error C2375
#include "pfs/windows.hpp"

namespace netty {
namespace utils {

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

inline error notify_addr_change (HANDLE * h, OVERLAPPED * o)
{
    DWORD rc = NotifyAddrChange(h, o);

    if (rc != NO_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            return error {
                  make_error_code(pfs::errc::system_error)
                , tr::_("NotifyAddrChange failure")
            };
        }
    }

    return error{};
}

PMIB_IPADDRTABLE get_ip_addr_table (error * perr)
{
    DWORD dwSize = 0;
    PMIB_IPADDRTABLE ip_addr_table = static_cast<MIB_IPADDRTABLE *>(MALLOC(sizeof(MIB_IPADDRTABLE)));

    if (ip_addr_table != nullptr) {
        // Make an initial call to GetIpAddrTable to get the
        // necessary size into the dwSize variable
        if (GetIpAddrTable(ip_addr_table, & dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
            FREE(ip_addr_table);
            ip_addr_table = static_cast<MIB_IPADDRTABLE *>(MALLOC(dwSize));
        }
    }

    if (ip_addr_table == nullptr) {
        pfs::throw_or(perr, error {
              make_error_code(std::errc::not_enough_memory)
            , tr::_("GetIpAddrTable")
        });

        return nullptr;
    }

    // Make a second call to GetIpAddrTable to get the
    // actual data we want

    DWORD rc = GetIpAddrTable(ip_addr_table, & dwSize, 0);

    if (rc != NO_ERROR) { 
        pfs::throw_or(perr, error {
              make_error_code(pfs::errc::system_error)
            , tr::f_("GetIpAddrTable failure: {}", pfs::system_error_text(static_cast<int>(rc)))
        });

        FREE(ip_addr_table);
        return nullptr;
    }

    //auto nentries = pfs::numeric_cast<int>(ip_addr_table->dwNumEntries);

    //for (int i = 0; i < nentries; i++) {

    //    //fmt::println("Interface Index[{}]: {}", i, ip_addr_table->table[i].dwIndex);

    //    ULONG index = ip_addr_table->table[i].dwIndex;
       //IN_ADDR IPAddr;
    //    IN_ADDR IPAddrMask;
    //    IN_ADDR IPAddrBCast;
    //  IPAddr.S_un.S_addr = 0;// static_cast<ULONG>(ip_addr_table->table[i].dwAddr);
    //    IPAddrMask.S_un.S_addr = static_cast<ULONG>(ip_addr_table->table[i].dwMask);
    //    IPAddrBCast.S_un.S_addr = static_cast<ULONG>(ip_addr_table->table[i].dwBCastAddr);

    //    fmt::println("ip: {}(value={}, index={}); subnet={}; broadcast={}"
    //        , inet_ntoa(IPAddr), IPAddr.S_un.S_addr, index
    //        , inet_ntoa(IPAddrMask), inet_ntoa(IPAddrBCast));
    //}

    return ip_addr_table;
}

class netlink_monitor::impl
{
public:
    OVERLAPPED overlap;
    HANDLE handle {nullptr};
    MIB_IPADDRTABLE * ip_addr_table {nullptr};

public:
    impl ()
    {
        std::memset(& overlap, 0, sizeof(overlap));
    }
};

netlink_monitor::netlink_monitor (error * perr)
    : _d(new impl)
{
    _d->overlap.hEvent = WSACreateEvent();

    auto err = notify_addr_change(& _d->handle, & _d->overlap);

    if (err) {
        pfs::throw_or(perr, std::move(err));
        return;
    }

    _d->ip_addr_table = get_ip_addr_table(perr);
}

netlink_monitor::~netlink_monitor ()
{
    if (_d->ip_addr_table != nullptr) {
        FREE(_d->ip_addr_table);
        _d->ip_addr_table = nullptr;
    }

    WSACloseEvent(_d->overlap.hEvent);
}

int netlink_monitor::poll (std::chrono::milliseconds millis, error * perr)
{
    auto rc = WaitForSingleObject(_d->overlap.hEvent, millis.count());
    int n = 0;

    switch (rc) {
        case WAIT_OBJECT_0: { // Our event occurred
            if (inet4_addr_added)
                inet4_addr_added(0, 0);

            BOOL success = WSAResetEvent(_d->overlap.hEvent);

            if (success != TRUE) {
                on_failure(netty::error {
                      make_error_code(pfs::errc::system_error)
                    , tr::_("WSAResetEvent failure: {}", pfs::system_error_text())
                });

                n = -1;
                break;
            }

            auto err = notify_addr_change(& _d->handle, & _d->overlap);

            if (err) {
                on_failure(err);
                n = -1;
                break;
            }

            // Get new IP table
            auto ip_addr_table = get_ip_addr_table(& err);

            if (err) {
                on_failure(err);
                n = -1;
                break;
            }

            auto nentries1 = pfs::numeric_cast<int>(ip_addr_table->dwNumEntries);
            auto nentries2 = pfs::numeric_cast<int>(_d->ip_addr_table->dwNumEntries);

            // Find new (added) IP addresses
            for (int i = 0; i < nentries1; i++) {
                bool found = false;

                for (int j = 0; j < nentries2; j++) {
                    if (ip_addr_table->table[i].dwAddr == _d->ip_addr_table->table[j].dwAddr) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    if (inet4_addr_added) {
                        auto addr = pfs::numeric_cast<std::uint32_t>(ip_addr_table->table[i].dwAddr);
                        addr = pfs::to_native_order(addr);
                        auto index = pfs::numeric_cast<std::uint32_t>(ip_addr_table->table[i].dwIndex);
                        inet4_addr_added(inet4_addr{addr}, index);
                    }
                }
            }

            // Find old (removed) IP addresses
            for (int j = 0; j < nentries2; j++) {
                bool found = false;

                for (int i = 0; i < nentries1; i++) {
                    if (ip_addr_table->table[i].dwAddr == _d->ip_addr_table->table[j].dwAddr) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    if (inet4_addr_removed) {
                        auto addr = pfs::numeric_cast<std::uint32_t>(_d->ip_addr_table->table[j].dwAddr);
                        addr = pfs::to_native_order(addr);
                        auto index = pfs::numeric_cast<std::uint32_t>(_d->ip_addr_table->table[j].dwIndex);
                        inet4_addr_added(inet4_addr(addr), index);
                    }
                }
            }

            if (_d->ip_addr_table != nullptr)
                FREE(_d->ip_addr_table);

            _d->ip_addr_table = ip_addr_table;

            break;
        }

        case WAIT_TIMEOUT:
            n = 0;
            break;

        case WAIT_ABANDONED:
            pfs::throw_or(perr, error {
                  make_error_code(pfs::errc::system_error)
                , tr::f_("WaitForSingleObject abandoned: {}", pfs::system_error_text())
            });
            n = -1;
            break;

        case WAIT_FAILED:
            pfs::throw_or(perr, error {
                  make_error_code(pfs::errc::system_error)
                , tr::f_("WaitForSingleObject failed: {}", pfs::system_error_text())
            });

            n = -1;
            break;

        default:
            pfs::throw_or(perr, error {
                  pfs::make_error_code(pfs::errc::unexpected_error)
                , tr::f_("WaitForSingleObject: {}", tr::f_("returned unexpected value: {:x}", rc))
            });

            n = -1;
            break;
    }

    return n;
}

}} // namespace netty::utils
