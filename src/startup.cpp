////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "startup.hpp"
#include <pfs/assert.hpp>
#include <atomic>

#if _MSC_VER
#   define WIN32_LEAN_AND_MEAN
#   include <winsock2.h>
#endif

#if NETTY__UDT_ENABLED
#   include "udt/newlib/udt.hpp"
#endif

#if NETTY__OPENSSL3_ENABLED
#   include <openssl/ssl.h>
#   include <openssl/err.h>
#endif

namespace netty {

//
// NOTE [Static Initialization Order Fiasco](https://en.cppreference.com/w/cpp/language/siof)
//

// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup

static std::atomic_int startup_counter {0};

void startup ()
{
    if (startup_counter.fetch_add(1) == 0) {
#if _MSC_VER
        WSADATA wsa_data;
        auto version_requested = MAKEWORD(2, 2);

        auto rc = WSAStartup(version_requested, & wsa_data);
        PFS__TERMINATE(rc == 0, "WSAStartup failed: the Winsock 2.2 or newer dll was not found");
#endif // _MSC_VER

#if NETTY__UDT_ENABLED
        try {
            UDT::startup_context ctx;

            ctx.state_changed_callback = [] (UDTSOCKET) {
                //_socket_state_changed_buffer.push(sid);
            };

            UDT::startup(std::move(ctx));
        } catch (CUDTException ex) {
            // Here may be exception CUDTException(1, 0, WSAGetLastError());
            // related to WSAStartup call.
            if (CUDTException{1, 0, 0}.getErrorCode() == ex.getErrorCode()) {
                PFS__TERMINATE(false, ex.getErrorMessage());
            } else {
                // Unexpected error
                PFS__TERMINATE(false, ex.getErrorMessage());
            }
        }
#endif

#if NETTY__OPENSSL3_ENABLED
        OPENSSL_INIT_SETTINGS const * ssl_settings = nullptr;
        auto rc = OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, ssl_settings);

        if (rc == 0)
            PFS__TERMINATE(false, "OpenSSL initialization failure");

        // OpenSSL 3.x: SSL_library_init not required; still safe to call ERR strings
        ERR_load_crypto_strings();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
#endif
    }
}

void cleanup ()
{
    if (startup_counter.fetch_sub(1) == 1) {
#if _MSC_VER
        WSACleanup();
#endif // _MSC_VER

#if NETTY__UDT_ENABLED
        UDT::cleanup();
#endif

#if NETTY__OPENSSL3_ENABLED
        // In versions prior to OpenSSL 1.1.0, ERR_free_strings() releases any resources created
        // by the above functions.
        // ERR_free_strings();

        // In versions prior to 1.1.0 EVP_cleanup() removed all ciphers and digests from the table.
        // It no longer has any effect in OpenSSL 1.1.0.
        // EVP_cleanup();
#endif
    }
}

} //namespace netty
