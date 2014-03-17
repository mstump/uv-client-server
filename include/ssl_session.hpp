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

#define BUFFER_SIZE 4096

class SSLSession {
  SSL* ssl;
  BIO* read_bio;
  BIO* write_bio;

 public:
  SSLSession(
      SSL_CTX* ctx) :
      ssl(SSL_new(ctx)),
      read_bio(BIO_new(BIO_s_mem())),
      write_bio(BIO_new(BIO_s_mem()))
  {}

  ~SSLSession() {
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
    SSL_set_bio(ssl, read_bio, write_bio);
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
      const std::deque<uv_buf_t>& read_input,
      std::deque<uv_buf_t>&       read_output,
      const std::deque<uv_buf_t>& write_input,
      std::deque<uv_buf_t>&       write_output) {
    for (std::deque<uv_buf_t>::const_iterator it = read_input.begin();
         it != read_input.end();
         ++it) {
      int written = BIO_write(read_bio, it->base, it->len);
      if (!check_error(written)) {
        return CQL_ERROR_SSL_READ;
      }
    }

    for (std::deque<uv_buf_t>::const_iterator it = write_input.begin();
         it != write_input.end();
         ++it) {
      int written = SSL_write(ssl, it->base, it->len);
      if (!check_error(written)) {
        return CQL_ERROR_SSL_WRITE;
      }
    }

    for (;;) {
      uv_buf_t buf = alloc_buffer(BUFFER_SIZE);
      int read = SSL_read(ssl, buf.base, buf.len);

      if (!check_error(read)) {
        free_buffer(buf);
        return CQL_ERROR_SSL_READ;
      }

      if (read > 0) {
        buf.len = read;
        read_output.push_back(buf);
      }
      free_buffer(buf);

      if (read != BUFFER_SIZE || read == 0) {
        break;
      }
    }

    for (;;) {
      uv_buf_t buf = alloc_buffer(BUFFER_SIZE);
      int read = BIO_read(write_bio, buf.base, buf.len);

      if (read > 0) {
        buf.len = read;
        write_output.push_back(buf);
      }
      free_buffer(buf);

      if (read != BUFFER_SIZE || read == 0) {
        break;
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

  // c->ssl = SSL_new(c->ssl_ctx);
  // c->read_bio = BIO_new(BIO_s_mem());
  // c->write_bio = BIO_new(BIO_s_mem());
  // SSL_set_bio(c->ssl, c->read_bio, c->write_bio);
  // SSL_set_connect_state(c->ssl);

  // r = SSL_do_handshake(c->ssl);

  // if(!SSL_is_init_finished(c->ssl)) {
  //    int r = SSL_connect(c->ssl);
};

#endif
