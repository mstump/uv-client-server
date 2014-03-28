/*
  Copyright 2014 DataStax

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __CLIENT_CONTEXT_HPP_INCLUDED__
#define __CLIENT_CONTEXT_HPP_INCLUDED__

#include <uv.h>
#include "ssl_context.hpp"

namespace cql {

struct ClientContext {
  uv_loop_t*       loop;
  cql::SSLContext* ssl;

  explicit
  ClientContext(
      uv_loop_t* loop) :
      loop(loop)
  {}

  SSLSession*
  ssl_session_new() {
    if (ssl) {
      return ssl->session_new();
    }
    return NULL;
  }

  inline void
  log(
      int         level,
      const char* message) {
    (void) level;
    std::cout << message << std::endl;
  }

  inline void
  log(
      int                level,
      const std::string& message) {
    (void) level;
    std::cout << message << std::endl;
  }
};
}
#endif
