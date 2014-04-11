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

#ifndef __COMMON_HPP_INCLUDED__
#define __COMMON_HPP_INCLUDED__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include <thread>
#include <deque>
#include <list>
#include <map>
#include <string>

#include "cql.h"
#include "cql_request.hpp"
#include "cql_error.hpp"
#include "cql_message.hpp"

namespace cql {

typedef std::function<void(int, const char*, size_t)> LogCallback;
typedef cql::Request<std::string, cql::Error*, cql::Message*> CallerRequest;

uv_buf_t
alloc_buffer(
    size_t suggested_size) {
  return uv_buf_init(new char[suggested_size], suggested_size);
}

uv_buf_t
alloc_buffer(
    uv_handle_t *handle,
    size_t       suggested_size) {
  (void) handle;
  return alloc_buffer(suggested_size);
}

void
free_buffer(
    uv_buf_t buf) {
  delete buf.base;
}

void
clear_buffer_deque(
    std::deque<uv_buf_t>& buffers) {
  for (std::deque<uv_buf_t>::iterator it = buffers.begin();
       it != buffers.end();
       ++it) {
    free_buffer(*it);
  }
  buffers.clear();
}

std::string
opcode_to_string(
    int opcode) {
  switch (opcode) {
    case CQL_OPCODE_ERROR:
      return "CQL_OPCODE_ERROR";
    case CQL_OPCODE_STARTUP:
      return "CQL_OPCODE_STARTUP";
    case CQL_OPCODE_READY:
      return "CQL_OPCODE_READY";
    case CQL_OPCODE_AUTHENTICATE:
      return "CQL_OPCODE_AUTHENTICATE";
    case CQL_OPCODE_CREDENTIALS:
      return "CQL_OPCODE_CREDENTIALS";
    case CQL_OPCODE_OPTIONS:
      return "CQL_OPCODE_OPTIONS";
    case CQL_OPCODE_SUPPORTED:
      return "CQL_OPCODE_SUPPORTED";
    case CQL_OPCODE_QUERY:
      return "CQL_OPCODE_QUERY";
    case CQL_OPCODE_RESULT:
      return "CQL_OPCODE_RESULT";
    case CQL_OPCODE_PREPARE:
      return "CQL_OPCODE_PREPARE";
    case CQL_OPCODE_EXECUTE:
      return "CQL_OPCODE_EXECUTE";
    case CQL_OPCODE_REGISTER:
      return "CQL_OPCODE_REGISTER";
    case CQL_OPCODE_EVENT:
      return "CQL_OPCODE_EVENT";
  };
  assert(false);
  return "";
}

enum HostDistance {
  LOCAL,
  REMOTE,
  IGNORED
};
}
#endif
