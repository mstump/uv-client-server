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

#ifndef __SSL_SESSION_HPP_INCLUDED__
#define __SSL_SESSION_HPP_INCLUDED__

#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

#include <deque>

#define BUFFER_SIZE 66560

class SSLSession {
  SSL* ssl;

  BIO* ssl_bio;
  BIO* network_bio;
  BIO* internal_bio;

 public:
  SSLSession(
      SSL_CTX* ctx) :
      ssl(SSL_new(ctx))
  {}

  bool
  init() {
    if (!ssl) {
      return false;
    }

    if (!BIO_new_bio_pair(
            &internal_bio,
            BUFFER_SIZE,
            &network_bio,
            BUFFER_SIZE)) {
      return false;
    }

    ssl_bio = BIO_new(BIO_f_ssl());
    if (!ssl_bio) {
      return false;
    }

    SSL_set_bio(ssl, internal_bio, internal_bio);
    BIO_set_ssl(ssl_bio, ssl, BIO_NOCLOSE);
    return true;
  }

  void
  shutdown() {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }

  void
  handshake(
      bool client) {
    if (client) {
      SSL_set_connect_state(ssl);
    } else {
      SSL_set_accept_state(ssl);
    }
    SSL_do_handshake(ssl);
  }

  bool
  handshake_done() {
    return SSL_is_init_finished(ssl);
  }

  char*
  ciphers(
      char* output,
      size_t size) {
    SSL_CIPHER* sc = SSL_get_current_cipher(ssl);
    return SSL_CIPHER_description(sc, output, size);
  }

  int
  read_write(
      char*   read_input,
      size_t  read_input_size,
      size_t& read_size,
      char**  read_output,
      size_t& read_output_size,
      char*   write_input,
      size_t  write_input_size,
      char**  write_output,
      size_t& write_output_size) {
    if (write_input_size) {
      int write_status = BIO_write(ssl_bio, write_input, write_input_size);
      if (!check_error(write_status)) {
        ERR_print_errors_fp(stdout);
        return CQL_ERROR_SSL_WRITE;
      }
    }

    int pending = BIO_ctrl_pending(ssl_bio);

    if (pending) {
      *read_output = new char[pending];
      int read     = BIO_read(ssl_bio, *read_output, pending);

      if (!check_error(read)) {
        return CQL_ERROR_SSL_READ;
      }
      read_output_size = read;
    }

    if (read_input_size > 0) {
      if ((read_size = BIO_get_write_guarantee(network_bio))) {
        if (read_size > read_input_size) {
          read_size = read_input_size;
        }

        if (!check_error(BIO_write(network_bio, read_input, read_size))) {
          return CQL_ERROR_SSL_READ;
        }
      }
    } else {
      read_size = 0;
    }

    write_output_size = BIO_ctrl_pending(network_bio);
    if (write_output_size) {
      *write_output = new char[write_output_size];
      if (!check_error(
              BIO_read(
                  network_bio,
                  *write_output,
                  write_output_size))) {
        return CQL_ERROR_SSL_WRITE;
      }
    }

    return CQL_ERROR_NO_ERROR;
  }

  bool
  check_error(
      int input) {
    int err = SSL_get_error(ssl, input);

    if (err == SSL_ERROR_NONE || err == SSL_ERROR_WANT_READ) {
      return true;
    }

    if (err == SSL_ERROR_SYSCALL) {
      return false;
    }

    if (err == SSL_ERROR_SSL) {
      return false;
    }
    return true;
  }
};

#endif
