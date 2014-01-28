// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

#ifndef __SSL_CONTEXT_HPP_INCLUDED__
#define __SSL_CONTEXT_HPP_INCLUDED__

#include <deque>

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

class ssl_context_t {

public:
    typedef int (*pem_callback_t)(char *, int, int, void *);
    typedef int (*verify_callback_t)(int, X509_STORE_CTX*);

    ssl_context_t() :
        _pem_callback(NULL),
        _verify_callback(&ssl_context_t::default_verify_callback),
        _ssl_ctx(NULL) // maybe substitute SSLv3_method here
    {}

    int
    init(bool client)
    {
        CRYPTO_malloc_debug_init();
        CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

        SSL_load_error_strings();
        SSL_library_init();
        if (client) {
            _ssl_ctx = SSL_CTX_new(SSLv23_client_method());
        }
        else {
            _ssl_ctx = SSL_CTX_new(SSLv23_server_method());
        }

        // SSL_CTX_set_cipher_list(_ssl_ctx, "TLS_RSA_WITH_AES_256_CBC_SHA");
        SSL_CTX_set_cipher_list(_ssl_ctx, "AES256-SHA");
        // TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH");
        SSL_CTX_set_verify(_ssl_ctx, SSL_VERIFY_PEER, _verify_callback);
        return CQL_ERROR_NO_ERROR;
    }

    ssl_session_t*
    session_new()
    {
        return new ssl_session_t(_ssl_ctx);
    }

    int
    add_ca(
        const char* input,
        size_t      size)
    {
        if (!_ca_store) {
            _ca_store = X509_STORE_new();
            SSL_CTX_set_cert_store(_ssl_ctx, _ca_store);
        }

        X509* x509 = load_pem_cert(input, size, _pem_callback);
        if (!x509) {
            return CQL_ERROR_SSL_CA_CERT;
        }

        X509_STORE_add_cert(_ca_store, x509);
        SSL_CTX_add_client_CA(_ssl_ctx, x509);
        X509_free(x509);
        return CQL_ERROR_NO_ERROR;
    }

    int
    add_crl(
        const char* input,
        size_t      size)
    {
        BIO* bio = load_bio(input, size);
        if (!bio) {
            return CQL_ERROR_SSL_CRL;
        }

        X509_CRL* x509 = PEM_read_bio_X509_CRL(bio, NULL, _pem_callback, NULL);
        if (!x509) {
            BIO_free_all(bio);
            return CQL_ERROR_SSL_CRL;
        }

        X509_STORE_add_crl(_ca_store, x509);
        X509_STORE_set_flags(_ca_store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
        BIO_free_all(bio);
        X509_CRL_free(x509);
        return CQL_ERROR_NO_ERROR;
    }

    int
    use_key(
        const char* input,
        size_t      input_size,
        const char* passphrase)
    {
        BIO* bio = load_bio(input, input_size);
        if (!bio) {
            return CQL_ERROR_SSL_PRIVATE_KEY;
        }

        EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, NULL, _pem_callback, (void*)passphrase);
        if (!key) {
            BIO_free_all(bio);
            return CQL_ERROR_SSL_PRIVATE_KEY;
        }

        SSL_CTX_use_PrivateKey(_ssl_ctx, key);
        EVP_PKEY_free(key);
        BIO_free_all(bio);
        return CQL_ERROR_NO_ERROR;
    }

    int
    use_cert(
        const char* input,
        size_t      size)
    {
        X509* x509 = load_pem_cert(input, size, _pem_callback);
        if (!x509) {
            return CQL_ERROR_SSL_CA_CERT;
        }

        int r = SSL_CTX_use_certificate(_ssl_ctx, x509);
        X509_free(x509);

        if (r <= 0) {
            return CQL_ERROR_SSL_CERT;
        }
        return CQL_ERROR_NO_ERROR;
    }

    int
    use_key(
        RSA* rsa)
    {
        if (SSL_CTX_use_RSAPrivateKey(_ssl_ctx, rsa) <= 0) {
            return CQL_ERROR_SSL_PRIVATE_KEY;
        }
        return CQL_ERROR_NO_ERROR;
    }

    int
    use_cert(
        X509* cert)
    {
        if (SSL_CTX_use_certificate(_ssl_ctx, cert) <= 0) {
            return CQL_ERROR_SSL_CERT;
        }
        return CQL_ERROR_NO_ERROR;
    }

    void
    ciphers(
        const char* ciphers)
    {
        SSL_CTX_set_cipher_list(_ssl_ctx, ciphers);
    }

    void
    pem_callback(
        pem_callback_t callback)
    {
        _pem_callback = callback;
    }

    void
    verify_callback(
        verify_callback_t callback)
    {
        _verify_callback = callback;
        SSL_CTX_set_verify(_ssl_ctx, SSL_VERIFY_PEER, callback);
    }

    /**
     * Used for self signed certs, or if an error occurs during validation
     *
     * @param preverify_ok
     * @param ctx
     *
     * @return 1 if ok 0 otherwise
     */
    static int
    default_verify_callback(
        int             preverify_ok,
        X509_STORE_CTX* ctx)
    {
        (void) preverify_ok;
        (void) ctx;
        return 1;
    }

    static BIO*
    load_bio(
        const char* input,
        size_t      size)
    {
        BIO* bio = BIO_new(BIO_s_mem());
        int r = BIO_write(bio, input, size);
        return r <= 0 ? NULL : bio;
    }

    static X509*
    load_pem_cert(
        const char*    input,
        size_t         size,
        pem_callback_t callback)
    {
        BIO *bio = load_bio(input, size);
        if (!bio) {
            return NULL;
        }

        X509* x509 = PEM_read_bio_X509(bio, NULL, callback, NULL);
        BIO_free_all(bio);
        return x509;
    }

    static RSA*
    create_key(
        size_t size)
    {
        RSA*    pair = RSA_new();
        BIGNUM* e    = BN_new();
        BN_set_word(e, 65537);

        if (!RSA_generate_key_ex(pair, size, e, NULL)) {
            pair = NULL;
        }
        BN_free(e);
        return pair;
    }

    static EVP_PKEY*
    get_evp_pkey(
        RSA* rsa,
        int  priv)
    {
#define CHECK_KEY(x) if (x) { if (pkey) EVP_PKEY_free(pkey); if (key) RSA_free(key); return NULL; }
        RSA* key = (priv ? RSAPrivateKey_dup(rsa): RSAPublicKey_dup(rsa));
        EVP_PKEY* pkey = EVP_PKEY_new();

        CHECK_KEY(!key);
        CHECK_KEY(!pkey);
        CHECK_KEY(!(EVP_PKEY_assign_RSA(pkey, key)));
        return pkey;
#undef CHECK_KEY
    }

    static X509*
    create_cert(
        RSA*         rsa,
        RSA*         rsaSign,
        const char*  cname,
        const char*  cnameSign,
        const char*  pszOrgName,
        unsigned int certLifetime)
    {
        time_t start_time = time(NULL);
        time_t end_time = start_time + certLifetime;

#define CHECK_CERT(x) if (x) { X509_free(x509); return NULL; }
        X509* x509 = X509_new();
        CHECK_CERT(!x509);

        EVP_PKEY* sign_pkey = get_evp_pkey(rsaSign, 1);
        CHECK_CERT(!sign_pkey);

        EVP_PKEY* pkey = get_evp_pkey(rsa, 0);
        CHECK_CERT(!pkey);

        CHECK_CERT(!X509_set_version(x509, 2));
        CHECK_CERT(!ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)start_time));

        X509_NAME* name = X509_NAME_new();
        CHECK_CERT(!name);

        int nid = OBJ_txt2nid("organizationName");
        CHECK_CERT(nid == NID_undef);
        CHECK_CERT(!X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC, (unsigned char*)pszOrgName, -1, -1, 0));
        CHECK_CERT((nid = OBJ_txt2nid("commonName")) == NID_undef);
        CHECK_CERT(!X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC, (unsigned char*)cname, -1, -1, 0));
        CHECK_CERT(!X509_set_subject_name(x509, name))

        X509_NAME* name_issuer = X509_NAME_new();
        CHECK_CERT(!name_issuer);
        CHECK_CERT((nid = OBJ_txt2nid("organizationName")) == NID_undef);

        CHECK_CERT(!(X509_NAME_add_entry_by_NID(name_issuer, nid, MBSTRING_ASC, (unsigned char*)pszOrgName, -1, -1, 0)));
        CHECK_CERT((nid = OBJ_txt2nid("commonName")) == NID_undef);
        CHECK_CERT(!(X509_NAME_add_entry_by_NID(name_issuer, nid, MBSTRING_ASC, (unsigned char*)cnameSign, -1, -1, 0)));
        CHECK_CERT(!(X509_set_issuer_name(x509, name_issuer)));
        CHECK_CERT(!X509_time_adj(X509_get_notBefore(x509), 0, &start_time));
        CHECK_CERT(!X509_time_adj(X509_get_notAfter(x509), 0, &end_time));
        CHECK_CERT(!X509_set_pubkey(x509, pkey));
        CHECK_CERT(!X509_sign(x509, sign_pkey, EVP_sha1()));
#undef CHECK_CERT

        if (sign_pkey) {
            EVP_PKEY_free(sign_pkey);
        }
        if (pkey) {
            EVP_PKEY_free(pkey);
        }
        if (name) {
            X509_NAME_free(name);
        }
        if (name_issuer) {
            X509_NAME_free(name_issuer);
        }

        return x509;
    }


private:

    ssl_context_t(const ssl_context_t&) {}
    void operator=(const ssl_context_t&) {}

    pem_callback_t    _pem_callback;
    verify_callback_t _verify_callback;
    SSL_CTX*          _ssl_ctx;
    X509_STORE*       _ca_store;
};

#endif
