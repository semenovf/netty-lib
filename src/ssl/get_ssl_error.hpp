////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "namespace.hpp"
#include "error.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

NETTY__NAMESPACE_BEGIN

namespace ssl {

inline error get_ssl_error (int ssl_errn, std::string const & desc)
{
    if (ssl_errn == SSL_ERROR_SYSCALL)
        return error {pfs::get_last_system_error(), desc};

    return error {
          make_error_code(errc::ssl_error)
        , fmt::format("{}: {}", desc, ERR_error_string(ssl_errn, nullptr))
    };
}

} // namespace ssl

NETTY__NAMESPACE_END
