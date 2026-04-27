////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"

// #if _MSC_VER
// #   include <pfs/windows.hpp>
// #   include <wincrypt.h>
// #endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

tls_socket::tls_socket () = default;

tls_socket::tls_socket (std::unique_ptr<tls_socket::impl> d)
    : _d(std::move(d))
{}

tls_socket::tls_socket (tls_options opts, error * perr)
    : _d(new impl)
{
    _d->opts = std::move(opts);
}

tls_socket::tls_socket (tls_socket && other) noexcept = default;
tls_socket & tls_socket::operator = (tls_socket && other) noexcept = default;

tls_socket::~tls_socket ()
{
    if (_d) {
        if (_d->ssl != nullptr) {
            SSL_free(_d->ssl);
            _d->ssl = nullptr;
        }

        // FIXME
        // if (_d->ctx != nullptr) {
        //     SSL_CTX_free(_d->ctx);
        //     _d->ctx = nullptr;
        // }
    }
}

tls_socket::operator bool () const noexcept
{
    return (_d && *_d);
}

tls_socket::socket_id tls_socket::id () const noexcept
{
    return _d->id();
}

socket4_addr tls_socket::saddr () const noexcept
{
    return _d->saddr();
}

#if 0

// The cipher list consists of one or more cipher strings separated by colons.
// Commas or spaces are also acceptable separators but colons are normally used.

constexpr char const * DEFAULT_CIPHERS =
    "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA"
    ":ECDHE-ECDSA-AES256-SHA:ECDHE-ECDSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA384"
    ":ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA"
    ":ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384"
    ":DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-SHA"
    ":DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256:AES128-SHA";

#ifdef _MSC_VER
namespace {
bool load_windows_system_certificates (SSL_CTX * ssl, error * perr)
{
    DWORD flags = CERT_STORE_READONLY_FLAG
        | CERT_STORE_OPEN_EXISTING_FLAG
        | CERT_SYSTEM_STORE_CURRENT_USER;

    HCERTSTORE system_store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, flags, L"Root");

    if (!system_store) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::f_("CertOpenStore failure: {}"
            , pfs::utf8_error(GetLastError()));
        return false;
    }

    PCCERT_CONTEXT certificate_iterator = NULL;
    X509_STORE* opensslStore = SSL_CTX_get_cert_store(ssl);

    int certificate_count = 0;

    while ((certificate_iterator = CertEnumCertificatesInStore(system_store, certificate_iterator))) {
        X509 * x509 = d2i_X509(NULL, (const unsigned char**) & certificate_iterator->pbCertEncoded
            , certificate_iterator->cbCertEncoded);

        if (x509) {
            if (X509_STORE_add_cert(opensslStore, x509) == 1) {
                ++certificate_count;
            }

            X509_free(x509);
        }
    }

    CertFreeCertificateContext(certificate_iterator);
    CertCloseStore(system_store, 0);

    if (certificate_count == 0) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("no certificates found"));
        return false;
    }

    return true;
}

} // namespace
#endif

struct socket::impl: public posix::tcp_socket
{
    SSL * conn {nullptr};
    SSL_CTX * ctx {nullptr};
    SSL_METHOD const * method {nullptr};
    tls_options opts;

    // FIXME REMOVE
    // static std::once_flag ssl_init_flag;
    // static std::atomic<bool> ssl_initialized;

    impl (): posix::tcp_socket() {}

    // FIXME REMOVE
    // bool init_openssl (error * perr)
    // {
    //     OPENSSL_INIT_SETTINGS const * ssl_settings = nullptr;
    //     auto rc = OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, ssl_settings);
    //
    //     if (rc == 0) {
    //         pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("OpenSSL initialization failure"));
    //         return false;
    //     }
    //
    //     OpenSSL_add_ssl_algorithms();
    //     SSL_load_error_strings();
    //
    //     ssl_initialized = true;
    //     return true;
    // }

    bool create_context (error * perr)
    {
        SSL_METHOD const * method = SSLv23_client_method();

        if (method == nullptr) {
            pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSLv23_client_method() failure"));
            return false;
        }

        SSL_CTX * ctx = SSL_CTX_new(method);

        if (ctx != nullptr) {
            SSL_CTX_set_mode(ctx
                , SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
            int options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_CIPHER_SERVER_PREFERENCE;

            SSL_CTX_set_options(ctx, options);
        } else {
            pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSL_CTX_new() failure"));
            return false;
        }

        this->method = method;
        this->ctx = ctx;
        return true;
    }

    bool add_ca_roots_from_string (std::string const roots)
    {
        X509_STORE * certificate_store = SSL_CTX_get_cert_store(this->ctx);

        if (certificate_store == nullptr)
            return false;

        X509_STORE_set_flags(certificate_store
            , X509_V_FLAG_TRUSTED_FIRST | X509_V_FLAG_PARTIAL_CHAIN);

        BIO * buffer = BIO_new_mem_buf((void*) roots.c_str(), static_cast<int>(roots.length()));

        if (buffer == nullptr)
            return false;

        bool success = true;
        size_t number_of_roots = 0;

        while (true) {
            X509 * root = PEM_read_bio_X509_AUX(buffer, nullptr, nullptr, (void*) "");

            if (root == nullptr) {
                ERR_clear_error();
                break;
            }

            ERR_clear_error();

            if (!X509_STORE_add_cert(certificate_store, root)) {
                unsigned long error = ERR_get_error();

                if (ERR_GET_LIB(error) != ERR_LIB_X509
                        || ERR_GET_REASON(error) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                    success = false;
                    X509_free(root);
                    break;
                }
            }

            X509_free(root);
            number_of_roots++;
        }

        BIO_free(buffer);

        if (number_of_roots == 0)
            success = false;

        return success;
    }

    bool handle_tls_options (error * perr)
    {
        ERR_clear_error();

        auto files_specified = opts.cert_file && opts.key_file;

        if (files_specified) {
            // The certificates must be in PEM format and must be sorted starting with the
            // subject's certificate (actual client or server certificate), followed by
            // intermediate CA certificates if applicable, and ending at the highest level (root) CA
            auto rc = SSL_CTX_use_certificate_chain_file(this->ctx, opts.cert_file->c_str());

            if (rc != 1) {
                auto errn = ERR_get_error();
                pfs::throw_or(perr, make_error_code(errc::ssl_error)
                    , tr::f_("loading certificate chain from file failure: {}: {}"
                    , *opts.cert_file, ERR_error_string(errn, nullptr)));
                return false;
            } else {
                // Add private key to context.
                rc = SSL_CTX_use_PrivateKey_file(this->ctx, opts.key_file->c_str(), SSL_FILETYPE_PEM);

                if (rc != 1) {
                    auto errn = ERR_get_error();
                    pfs::throw_or(perr, make_error_code(errc::ssl_error)
                        , tr::f_("adding private key failure: {}: {}"
                        , *opts.key_file, ERR_error_string(errn, nullptr)));
                    return false;
                } else {
                    // Check the consistency of a private key with the corresponding certificate
                    // loaded into context. If more than one key/certificate pair (RSA/DSA) is
                    // installed, the last item installed will be checked.
                    rc = SSL_CTX_check_private_key(this->ctx);

                    if (rc != 1) {
                        auto errn = ERR_get_error();
                        pfs::throw_or(perr, make_error_code(errc::ssl_error)
                            , tr::f_("checking the consistency of a private key: {}"
                            , ERR_error_string(errn, nullptr)));
                        return false;
                    }
                }
            }
        }

        ERR_clear_error();

        bool peer_verification_disabled = !opts.ca_file.has_value();

        if (!peer_verification_disabled) {
            if (*opts.ca_file == "SYSTEM") {
#ifdef _MSC_VER
                if (!load_windows_system_certificates(this->ctx, perr))
                    return false;
#else
                // Specify that the default locations from which CA certificates are loaded should
                // be used. There is one default directory, one default file and one default store.
                // The default CA certificates directory is called certs in the default OpenSSL
                // directory, and this is also the default store.
                auto rc = SSL_CTX_set_default_verify_paths(this->ctx);

                if (rc == 0) {
                    auto errn = ERR_get_error();
                    pfs::throw_or(perr, make_error_code(errc::ssl_error)
                        , tr::f_("specifying the default locations"
                        " of CA certificates failure: {}", ERR_error_string(errn, nullptr)));
                    return false;
                }
#endif
            } else {
                std::string marker = "-----BEGIN CERTIFICATE-----";
                auto is_using_in_memory = opts.ca_file->find(marker) != std::string::npos;

                if (is_using_in_memory) {
                    add_ca_roots_from_string(*opts.ca_file);
                } else {
                    // Specifies the locations for context, at which CA certificates for
                    // verification purposes are located. The certificates available via CAfile,
                    // CApath and CAstore are trusted.
                    auto rc = SSL_CTX_load_verify_locations(this->ctx, opts.ca_file->c_str(), nullptr);

                    if (rc != 1) {
                        auto errn = ERR_get_error();
                        pfs::throw_or(perr, make_error_code(errc::ssl_error)
                            , tr::f_("specifying the locations"
                            " of CA certificates failure: {}", ERR_error_string(errn, nullptr)));
                        return false;
                    }
                }
            }

            SSL_CTX_set_verify(this->ctx, SSL_VERIFY_PEER, [](int preverify, X509_STORE_CTX *) -> int
            {
                return preverify;
            });

            SSL_CTX_set_verify_depth(this->ctx, 4);
        } else {
            SSL_CTX_set_verify(this->ctx, SSL_VERIFY_NONE, nullptr);
        }

        bool is_using_default_ciphers = !opts.ciphers.has_value() || *opts.ciphers == "DEFAULT";
        char const * cipher_list = is_using_default_ciphers
            ? DEFAULT_CIPHERS
            : opts.ciphers->c_str();

        // Set the list of default ciphers (TLSv1.2 and below) for context using
        // the control string.
        auto rc = SSL_CTX_set_cipher_list(this->ctx, cipher_list);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, make_error_code(errc::ssl_error)
                , tr::f_("setting cipher list failure: {}: list=\"{}\""
                    , ERR_error_string(errn, nullptr)
                    , cipher_list));
            return false;
        }

        return true;
    }

    bool openSSLCheckServerCert(SSL* ssl,
#if OPENSSL_VERSION_NUMBER < 0x10100000L
                                               const std::string& hostname,
#else
                                               const std::string& /* hostname */,
#endif
                                               std::string& errMsg)
    {
        // FIXME
//         X509* server_cert = SSL_get_peer_certificate(ssl);
//         if (server_cert == nullptr)
//         {
//             errMsg = "OpenSSL failed - peer didn't present a X509 certificate.";
//             return false;
//         }
//
// #if OPENSSL_VERSION_NUMBER < 0x10100000L
//         // Check server name
//         bool hostname_verifies_ok = false;
//         STACK_OF(GENERAL_NAME)* san_names = (STACK_OF(GENERAL_NAME)*) X509_get_ext_d2i(
//             (X509*) server_cert, NID_subject_alt_name, NULL, NULL);
//         if (san_names)
//         {
//             for (int i = 0; i < sk_GENERAL_NAME_num(san_names); i++)
//             {
//                 const GENERAL_NAME* sk_name = sk_GENERAL_NAME_value(san_names, i);
//                 if (sk_name->type == GEN_DNS)
//                 {
//                     char* name = (char*) ASN1_STRING_data(sk_name->d.dNSName);
//                     if ((size_t) ASN1_STRING_length(sk_name->d.dNSName) == strlen(name) &&
//                         checkHost(hostname, name))
//                     {
//                         hostname_verifies_ok = true;
//                         break;
//                     }
//                 }
//             }
//         }
//         sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);
//
//         if (!hostname_verifies_ok)
//         {
//             int cn_pos = X509_NAME_get_index_by_NID(
//                 X509_get_subject_name((X509*) server_cert), NID_commonName, -1);
//             if (cn_pos >= 0)
//             {
//                 X509_NAME_ENTRY* cn_entry =
//                     X509_NAME_get_entry(X509_get_subject_name((X509*) server_cert), cn_pos);
//
//                 if (cn_entry != nullptr)
//                 {
//                     ASN1_STRING* cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
//                     char* cn = (char*) ASN1_STRING_data(cn_asn1);
//
//                     if ((size_t) ASN1_STRING_length(cn_asn1) == strlen(cn) &&
//                         checkHost(hostname, cn))
//                     {
//                         hostname_verifies_ok = true;
//                     }
//                 }
//             }
//         }
//
//         if (!hostname_verifies_ok)
//         {
//             errMsg = "OpenSSL failed - certificate was issued for a different domain.";
//             return false;
//         }
// #endif
//
//         X509_free(server_cert);
        return true;
    }

    bool openSSLClientHandshake (std::string const & host, error * perr
        /*, CancellationRequest const & isCancellationRequested*/)
    {
        while (true) {
            // if (_ssl_connection == nullptr || _ssl_context == nullptr)
            //     return false;

            // FIXME
            // if (isCancellationRequested()) {
            //     errMsg = "Cancellation requested";
            //     return false;
            // }

            ERR_clear_error();

            // Initiate the TLS/SSL handshake with a server.
            // If the underlying BIO is nonblocking, SSL_connect() will also return when
            // the underlying BIO could not satisfy the needs of SSL_connect() to continue
            // the handshake, indicating the problem by the return value -1. In this case a call
            // to SSL_get_error() with the return value of SSL_connect() will yield
            // SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE. The calling process then must repeat
            // the call after taking appropriate action to satisfy the needs of SSL_connect().
            // The action depends on the underlying BIO. When using a nonblocking socket,
            // nothing is to be done, but select() can be used to check for the required condition.
            // When using a buffering BIO, like a BIO pair, data must be written into or retrieved
            // out of the BIO before being able to continue.
            // int connect_result = SSL_connect(this->conn);

            // // The TLS/SSL handshake was successfully completed, a TLS/SSL connection has been
            // // established.
            // if (connect_result == 1) {
            //     if (opts.disable_hostname_validation)
            //         return true;
            //
            //     return openSSLCheckServerCert(_ssl_connection, host, errMsg);
            // }
            //
            // int reason = SSL_get_error(_ssl_connection, connect_result);
            // bool rc = false;
            //
            // if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
            //     rc = true;
            // } else {
            //     errMsg = getSSLError(connect_result);
            //     rc = false;
            // }
            //
            // if (!rc)
            //     return false;
        }

        return true;
    }

    bool do_handshake (socket4_addr const & remote_saddr, error * perr)
    {
        auto success = create_context(perr);

        if (!success)
            return false;

        success = handle_tls_options(perr);

        if (!success)
            return false;

        this->conn = SSL_new(this->ctx);

        if (this->conn == nullptr) {
            pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("connection failure"));
            return false;
        }

        SSL_set_fd(this->conn, this->id());

        // SNI support
        auto host = to_string(remote_saddr.addr);
        SSL_set_tlsext_host_name(this->conn, host.c_str());

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
        // Support for server name verification
        // (The docs say that this should work from 1.0.2, and is the default from
        // 1.1.0, but it does not. To be on the safe side, the manual test
        // below is enabled for all versions prior to 1.1.0.)
        if (!opts.disable_hostname_validation) {
            X509_VERIFY_PARAM * param = SSL_get0_param(this->conn);
            X509_VERIFY_PARAM_set1_host(param, host.c_str(), host.size());
        }
#endif

        // FIXME
        // success = openSSLClientHandshake(host, errMsg, isCancellationRequested);

        return success;
    }
};

// FIXME REMOVE
// std::once_flag socket::impl::ssl_init_flag;
// std::atomic<bool> socket::impl::ssl_initialized {false};


conn_status socket::connect (socket4_addr const & remote_saddr, error * perr)
// bool SocketOpenSSL::connect(const std::string& host,
//                                 int port,
//                                 std::string& errMsg,
//                                 const CancellationRequest& isCancellationRequested)
{
    auto status = _d->connect(remote_saddr, perr);

    if (status == conn_status::failure)
        return status;

    if (status != conn_status::connected)
        return status;

    // Do handshake
    auto success = _d->do_handshake(remote_saddr, perr);

    if (!success)
        return conn_status::failure;

    return status;
}


#endif

} // namespace ssl

NETTY__NAMESPACE_END

