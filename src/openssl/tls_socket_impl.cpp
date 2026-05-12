////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"
#include <pfs/i18n.hpp>

#if _MSC_VER
#   include <pfs/windows.hpp>
#   include <wincrypt.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

#if _MSC_VER
namespace {

bool load_windows_system_certificates (SSL_CTX * ctx, error * perr)
{
    DWORD flags = CERT_STORE_READONLY_FLAG
        | CERT_STORE_OPEN_EXISTING_FLAG
        | CERT_SYSTEM_STORE_CURRENT_USER;

    HCERTSTORE hstore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, flags, L"Root");

    if (!hstore) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::f_("CertOpenStore() call failure: {}"
            , pfs::system_error_text()));
        return false;
    }

    PCCERT_CONTEXT cert_iter = NULL;
    X509_STORE * openssl_store = SSL_CTX_get_cert_store(ctx);

    int cert_count = 0;

    while ((cert_iter = CertEnumCertificatesInStore(hstore, cert_iter))) {
        unsigned char const * encoded = cert_iter->pbCertEncoded;

        X509 * cert = d2i_X509(nullptr, & encoded, cert_iter->cbCertEncoded);

        if (cert != nullptr) {
            auto success = X509_STORE_add_cert(openssl_store, cert) > 0;

            if (success)
                ++cert_count;

            X509_free(cert);
        }
    }

    CertFreeCertificateContext(hstore);
    CertCloseStore(hstore, 0);

    if (cert_count == 0) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("no system certificates found"));
        return false;
    }

    return true;
}

} // namespace
#endif

bool load_certificates (SSL_CTX * ctx, tls_options const & opts, error * perr)
{
    // TODO Provide loading system certificates

    int encoding = opts.format == encoding_format::pem
        ? SSL_FILETYPE_PEM
        : opts.format == encoding_format::asn1
            ? SSL_FILETYPE_ASN1
            : SSL_FILETYPE_PEM;

    if (opts.cert_file) {
        auto rc = SSL_CTX_use_certificate_file(ctx, opts.cert_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("loading certificate from file (SSL_CTX_use_certificate_file) failure: {}"
                , *opts.cert_file)));
            return false;
        }
    }

    if (opts.key_file) {
        auto rc = SSL_CTX_use_PrivateKey_file(ctx, opts.key_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("adding private key (SSL_CTX_use_PrivateKey_file) failure: {}"
                , *opts.key_file)));
            return false;
        }

        // Check the consistency of a private key with the corresponding certificate loaded
        // into context.
        rc = SSL_CTX_check_private_key(ctx);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("checking the consistency of a private key (SSL_CTX_check_private_key)"
                    " failure: {}", *opts.key_file)));
            return false;
        }
    }

    return ctx;
}

} // namespace ssl

NETTY__NAMESPACE_END
