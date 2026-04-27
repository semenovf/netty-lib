////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
// #include "../error.hpp"
// #include "../exports.hpp"
// #include "../conn_status.hpp"
#include "../namespace.hpp"
// // #include "../send_result.hpp"
// #include "../socket4_addr.hpp"
#include <pfs/optional.hpp>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace ssl {

enum class encoding_format { pem, asn1 };

struct tls_options
{
    encoding_format format = encoding_format::pem;
    pfs::optional<std::string> cert_file;
    pfs::optional<std::string> key_file;

    // The ca certificate (or certificate bundle) file containing
    // certificates to be trusted by peers; use 'SYSTEM' to
    // leverage the system defaults.
    // If caFile is not set peer, verification is disabled.
    pfs::optional<std::string> ca_file;

     // List of ciphers (rsa, etc...)
    pfs::optional<std::string> ciphers {"DEFAULT"};

    // Whether to skip validating the peer's hostname against the certificate presented
    bool disable_hostname_validation = false;
};

} // namespace ssl

NETTY__NAMESPACE_END
