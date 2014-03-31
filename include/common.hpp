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

#define CQL_LOG_CRITICAL 0x00
#define CQL_LOG_ERROR    0x01
#define CQL_LOG_INFO     0x02
#define CQL_LOG_DEBUG    0x03

#define CQL_ERROR_SOURCE_OS          1
#define CQL_ERROR_SOURCE_NETWORK     2
#define CQL_ERROR_SOURCE_SSL         3
#define CQL_ERROR_SOURCE_COMPRESSION 4
#define CQL_ERROR_SOURCE_SERVER      5
#define CQL_ERROR_SOURCE_LIBRARY     6

#define CQL_ERROR_NO_ERROR          0
#define CQL_ERROR_SSL_CERT          1000000
#define CQL_ERROR_SSL_PRIVATE_KEY   1000001
#define CQL_ERROR_SSL_CA_CERT       1000002
#define CQL_ERROR_SSL_CRL           1000003
#define CQL_ERROR_SSL_READ          1000004
#define CQL_ERROR_SSL_WRITE         1000005
#define CQL_ERROR_SSL_READ_WAITING  1000006
#define CQL_ERROR_SSL_WRITE_WAITING 1000007
#define CQL_ERROR_NO_STREAMS        1000008

#define CQL_OPCODE_ERROR        0x00
#define CQL_OPCODE_STARTUP      0x01
#define CQL_OPCODE_READY        0x02
#define CQL_OPCODE_AUTHENTICATE 0x03
#define CQL_OPCODE_CREDENTIALS  0x04
#define CQL_OPCODE_OPTIONS      0x05
#define CQL_OPCODE_SUPPORTED    0x06
#define CQL_OPCODE_QUERY        0x07
#define CQL_OPCODE_RESULT       0x08
#define CQL_OPCODE_PREPARE      0x09
#define CQL_OPCODE_EXECUTE      0x0A
#define CQL_OPCODE_REGISTER     0x0B
#define CQL_OPCODE_EVENT        0x0C

#define CQL_CONSISTENCY_ANY          0x0000
#define CQL_CONSISTENCY_ONE          0x0001
#define CQL_CONSISTENCY_TWO          0x0002
#define CQL_CONSISTENCY_THREE        0x0003
#define CQL_CONSISTENCY_QUORUM       0x0004
#define CQL_CONSISTENCY_ALL          0x0005
#define CQL_CONSISTENCY_LOCAL_QUORUM 0x0006
#define CQL_CONSISTENCY_EACH_QUORUM  0x0007
#define CQL_CONSISTENCY_SERIAL       0x0008
#define CQL_CONSISTENCY_LOCAL_SERIAL 0x0009
#define CQL_CONSISTENCY_LOCAL_ONE    0x000A

#define CQL_COLUMN_TYPE_UNKNOWN   0xFFFF
#define CQL_COLUMN_TYPE_CUSTOM    0x0000
#define CQL_COLUMN_TYPE_ASCII     0x0001
#define CQL_COLUMN_TYPE_BIGINT    0x0002
#define CQL_COLUMN_TYPE_BLOB      0x0003
#define CQL_COLUMN_TYPE_BOOLEAN   0x0004
#define CQL_COLUMN_TYPE_COUNTER   0x0005
#define CQL_COLUMN_TYPE_DECIMAL   0x0006
#define CQL_COLUMN_TYPE_DOUBLE    0x0007
#define CQL_COLUMN_TYPE_FLOAT     0x0008
#define CQL_COLUMN_TYPE_INT       0x0009
#define CQL_COLUMN_TYPE_TEXT      0x000A
#define CQL_COLUMN_TYPE_TIMESTAMP 0x000B
#define CQL_COLUMN_TYPE_UUID      0x000C
#define CQL_COLUMN_TYPE_VARCHAR   0x000D
#define CQL_COLUMN_TYPE_VARINT    0x000E
#define CQL_COLUMN_TYPE_TIMEUUID  0x000F
#define CQL_COLUMN_TYPE_INET      0x0010
#define CQL_COLUMN_TYPE_LIST      0x0020
#define CQL_COLUMN_TYPE_MAP       0x0021
#define CQL_COLUMN_TYPE_SET       0x0022

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

#endif
