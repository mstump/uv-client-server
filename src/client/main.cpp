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

#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <uv.h>
#include <vector>
#include <iomanip>

#include "common.hpp"
#include "request.hpp"
#include "ssl_session.hpp"
#include "ssl_context.hpp"

#define MESSAGE_SLOTS 128

struct context_t {
   uv_loop_t* loop;

   explicit
   context_t(
      uv_loop_t* loop) :
      loop(loop)
   {}
};

#define STREAM_ID_MIN 1
#define STREAM_ID_MAX 127
#define STREAM_ID_COUNT STREAM_ID_MAX - STREAM_ID_MIN + 1
#define ADDRESS_MAX_LENGTH 46

#define CONNECTION_STATE_NEW 0
#define CONNECTION_STATE_RESOLVING 1
#define CONNECTION_STATE_RESOLVED 2
#define CONNECTION_STATE_CONNECTING 3
#define CONNECTION_STATE_CONNECTIED 4
#define CONNECTION_STATE_SSL_NEGOCIATING 5

struct client_connection_t {
   typedef request_t<message_t, message_t> caller_request_t;

   context_t*        context;
   uint64_t          state;
   message_t*        incomming;
   int8_t            available_streams[STREAM_ID_COUNT];
   size_t            available_streams_index;
   caller_request_t* callers[STREAM_ID_COUNT];

   // DNS and hostname stuff
   char*            address[46];
   int              address_family;
   std::string      hostname;
   std::string      port;
   uv_getaddrinfo_t resolver;
   struct addrinfo  resolver_hints;

   // the actual connection
   uv_connect_t connect_request;
   uv_tcp_t*    socket;

   explicit
   client_connection_t(
      context_t* context) :
      context(context),
      incomming(new message_t()),
      available_streams_index(STREAM_ID_MAX),
      address_family(PF_INET), // use ipv4 by default
      hostname("localhost"),
      port("9042"),
      socket(NULL)
   {
      for (int i = STREAM_ID_MIN; i < STREAM_ID_MAX + 1; ++i) {
         available_streams[i - STREAM_ID_MIN] = i;
      }

      resolver.data = this;
      connect_request.data = this;

      resolver_hints.ai_family = address_family;
      resolver_hints.ai_socktype = SOCK_STREAM;
      resolver_hints.ai_protocol = IPPROTO_TCP;
      resolver_hints.ai_flags = 0;
      memset(address, 0, ADDRESS_MAX_LENGTH);
   }

   void
   consume_buffer(
      uv_buf_t buf)
   {
      if (incomming->consume_buffer(buf)) {
         delete incomming;
         incomming = new message_t();
      }
   }

   void
   send_message(
      caller_request_t* request)
   {
      if (available_streams_index != 0) {
         int8_t stream_id = available_streams[available_streams_index--];
         callers[stream_id] = request;
         // TODO request_send(request);
      }
   }

   static void
   on_connect(
      uv_connect_t *request,
      int           status)
   {
      client_connection_t* connection = (client_connection_t*) request->data;
      if (status == -1) {
         // TODO
         fprintf(stderr, "connect failed error %s\n", uv_err_name(uv_last_error(connection->context->loop)));
         return;
      }
      // TODO do something uv_read_start((uv_stream_t*) req->data, alloc_buffer, on_read);
   }

   static void
   on_resolve(
      uv_getaddrinfo_t *resolver,
      int               status,
      struct addrinfo  *res)
   {
      client_connection_t* connection = (client_connection_t*) resolver->data;
      if (status == -1) {
         // TODO
         fprintf(stderr, "getaddrinfo callback error %s\n", uv_err_name(uv_last_error(connection->context->loop)));
         return;
      }

      // store the human readable address
      if (res->ai_family == AF_INET) {
         uv_ip4_name((struct sockaddr_in*) res->ai_addr, (char*) connection->address, ADDRESS_MAX_LENGTH);
      }
      else if (res->ai_family == AF_INET6) {
         uv_ip6_name((struct sockaddr_in6*) res->ai_addr, (char*) connection->address, ADDRESS_MAX_LENGTH);
      }

      // connect to the resolved host
      connection->socket = new uv_tcp_t;
      uv_tcp_init(connection->context->loop, connection->socket);
      uv_tcp_connect(&connection->connect_request, connection->socket, *(struct sockaddr_in*) res->ai_addr, client_connection_t::on_connect);
      uv_freeaddrinfo(res);
   }

   void
   hostname_resolve()
   {
      uv_getaddrinfo(context->loop,
                     &resolver,
                     client_connection_t::on_resolve,
                     hostname.c_str(),
                     port.c_str(),
                     &resolver_hints);
   }

private:
   client_connection_t(const client_connection_t&) {}
   void operator=(const client_connection_t&) {}

};

void
handle_read(
   uv_stream_t* client,
   ssize_t      nread,
   uv_buf_t     buf)
{
   client_connection_t* connection = reinterpret_cast<client_connection_t*>(client->data);
   context_t*           context    = connection->context;

   if (nread == -1) {
      if (uv_last_error(context->loop).code != UV_EOF) {
         fprintf(stderr, "Read error %s\n", uv_err_name(uv_last_error(context->loop)));
      }
      uv_close(reinterpret_cast<uv_handle_t*>(client), NULL);
      return;
   }

   connection->consume_buffer(buf);
}

void
on_new_connection(
   uv_stream_t* server,
   int          status)
{
   context_t* context = reinterpret_cast<context_t*>(server->data);
   if (status == -1) {
      return;
   }

   uv_tcp_t *client = new uv_tcp_t();
   uv_tcp_init(context->loop, client);
   client->data = new client_connection_t(context);

   if (uv_accept(server, reinterpret_cast<uv_stream_t*>(client)) == 0) {
      uv_read_start(reinterpret_cast<uv_stream_t*>(client), alloc_buffer, handle_read);
   }
   else {
      uv_close(reinterpret_cast<uv_handle_t*>(client), NULL);
   }
}

int
main() {
   ssl_context_t ssl_client_context;
   ssl_client_context.init(true);

   ssl_context_t ssl_server_context;
   ssl_server_context.init(false);

   RSA* rsa = ssl_context_t::create_key(2048);
   if (!rsa) {
      std::cerr << "create_key" << std::endl;
      return 1;
   }

   const char* pszCommonName = "test name";
   X509* cert = ssl_context_t::create_cert(rsa, rsa, pszCommonName, pszCommonName, "DICE", 3 * 365 * 24 * 60 * 60);
   if (!cert) {
      std::cerr << "Couldn't create a certificate" << std::endl;
      return 1;
   }
   ssl_server_context.use_key(rsa);
   ssl_server_context.use_cert(cert);

   std::auto_ptr<ssl_session_t> client_session(ssl_client_context.session_new());
   std::auto_ptr<ssl_session_t> server_session(ssl_server_context.session_new());

   client_session->handshake(true);
   server_session->handshake(false);

   std::deque<uv_buf_t> encrypt_input;
   std::deque<uv_buf_t> encrypt_output;
   std::deque<uv_buf_t> decrypt_output;

   const char* input_data   = "foobar";
   uv_buf_t    input_buffer = uv_buf_init((char*)input_data, strlen(input_data));

   encrypt_input.push_back(input_buffer);
   std::cout << "client_session->write_ssl " << client_session->write_ssl(encrypt_input, encrypt_output) << std::endl;
   if (encrypt_output.empty()) {
      return 1;
   }

   for (std::deque<uv_buf_t>::const_iterator it = encrypt_output.begin();
        it != encrypt_output.end();
        ++it)
   {
      std::cout << "buf.len " << it->len << std::endl;
   }

   int rv = server_session->read_ssl(encrypt_output, decrypt_output);
   std::cout << "client_session->read_ssl " << rv << std::endl;
   if (decrypt_output.empty()) {
      return 2;
   }

   return 0;

   context_t context(uv_default_loop());
   client_connection_t connection(&context);
   connection.hostname_resolve();
   return uv_run(context.loop, UV_RUN_DEFAULT);


   uv_tcp_t server;
   uv_tcp_init(context.loop, &server);
   server.data = &context;

   struct sockaddr_in bind_addr = uv_ip4_addr("127.0.0.1", 7777);
   uv_tcp_bind(&server, bind_addr);

   if (uv_listen(reinterpret_cast<uv_stream_t*>(&server), 128, on_new_connection)) {
      fprintf(stderr, "Listen error %s\n", uv_err_name(uv_last_error(context.loop)));
      return 1;
   }
   return uv_run(context.loop, UV_RUN_DEFAULT);
}
