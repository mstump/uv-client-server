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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <thread>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

#include "client_connection.hpp"

void
ready(
    cql::ClientConnection* connection,
    cql::Error*            err) {

  if (err) {
    std::cout << err->message << std::endl;
  } else {
    std::cout << "ready" << std::endl;
    connection->set_keyspace("system");
  }
}

void
keyspace_set(
    cql::ClientConnection* connection,
    char*                  keyspace,
    size_t                 size) {
  (void) connection;
  (void) size;
  std::cout << "keyspace_set: " << keyspace << std::endl;
}

int
main() {
  cql::ClientContext    context(uv_default_loop());
  cql::ClientConnection connection(&context);
  connection.init(ready);
  return uv_run(context.loop, UV_RUN_DEFAULT);
}
