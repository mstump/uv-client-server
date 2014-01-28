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

#include "common.hpp"

struct context_t {
   uv_loop_t* loop;

   explicit
   context_t(
      uv_loop_t* loop) :
      loop(loop)
   {}
};


struct connection_t {
   context_t* context;
   size_t     state;
   message_t* incomming;

   explicit
   connection_t(
      context_t* context) :
      context(context),
      incomming(new message_t())
   {}

   void
   consume_buffer(
      uv_buf_t buf)
   {
      if (incomming->consume_buffer(buf)) {
         delete incomming;
         incomming = new message_t();
      }
   }
};

void
handle_read(
   uv_stream_t* client,
   ssize_t      nread,
   uv_buf_t     buf)
{
   connection_t* connection = reinterpret_cast<connection_t*>(client->data);
   context_t*    context    = connection->context;

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
   client->data = new connection_t(context);

   if (uv_accept(server, reinterpret_cast<uv_stream_t*>(client)) == 0) {
      uv_read_start(reinterpret_cast<uv_stream_t*>(client), alloc_buffer, handle_read);
   }
   else {
      uv_close(reinterpret_cast<uv_handle_t*>(client), NULL);
   }
}

int main() {
   context_t context(uv_default_loop());

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
