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

#ifndef __COMMON_HPP_INCLUDED__
#define __COMMON_HPP_INCLUDED__

#include <assert.h>
#include <deque>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <uv.h>

#define CQL_HEADER_SIZE 8

#define CQL_ERROR_NO_ERROR        0
#define CQL_ERROR_SSL_CERT        1000000
#define CQL_ERROR_SSL_PRIVATE_KEY 1000001
#define CQL_ERROR_SSL_CA_CERT     1000002
#define CQL_ERROR_SSL_CRL         1000003
#define CQL_ERROR_SSL_READ        1000004
#define CQL_ERROR_SSL_WRITE       1000005

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


uv_buf_t
alloc_buffer(
   uv_handle_t *handle,
   size_t suggested_size)
{
   (void) handle;
   return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void
free_buffer(
   uv_buf_t& buf)
{
   delete buf.base;
}

char*
decode_short(
   char*    input,
   int16_t& output)
{
   output = ntohs(*(reinterpret_cast<int16_t*>(input)));
   return input + sizeof(int16_t);
}

char*
encode_short(
   char*   output,
   int16_t value)
{
   int16_t net_value = htons(value);
   return (char*) memcpy(output, &net_value, sizeof(net_value)) + sizeof(int16_t);
}

char*
decode_int(
   char*    input,
   int32_t& output)
{
   output = ntohl(*(reinterpret_cast<const int32_t*>(input)));
   return input + sizeof(int32_t);
}

char*
encode_int(
   char*   output,
   int32_t value)
{
   int32_t net_value = htonl(value);
   return (char*) memcpy(output, &net_value, sizeof(net_value)) + sizeof(int32_t);
}

char*
decode_string(
   char*   input,
   char**  output,
   size_t& size)
{
   *output = decode_short(input, ((int16_t&) size));
   return *output + size;
}

char*
encode_string(
   char*       output,
   const char* input,
   size_t      size)
{
   char* buffer = encode_short(output, size);
   return (char*) memcpy(buffer, input, size) + size;
}


struct body_t {

   virtual bool
   consume(
      size_t                reserved,
      std::deque<uv_buf_t>& buffers) = 0;

   virtual bool
   prepare(
      size_t                reserved,
      std::deque<uv_buf_t>& buffers) = 0;

   virtual
   ~body_t()
   {}

};

struct body_error_t
   : public body_t
{
   std::auto_ptr<char> guard;
   int32_t             code;
   char*               message;
   size_t              message_size;

   body_error_t() :
      guard(NULL),
      code(0xFFFFFFFF),
      message(NULL),
      message_size(0)
   {}

   body_error_t(
      int32_t     code,
      const char* input,
      size_t      input_size) :
      guard((char*) malloc(input_size)),
      code(code),
      message(guard.get()),
      message_size(input_size)
   {
      memcpy(message, input, input_size);
   }

   bool
   consume(
      size_t                reserved,
      std::deque<uv_buf_t>& buffers)
   {
      if (buffers.empty()) {
         return false;
      }

      char* buffer = buffers.front().base + reserved;
      buffer       = decode_int(buffer, code);
      decode_string(buffer, &message, message_size);
      return true;
   }

   bool
   prepare(
      size_t                reserved,
      std::deque<uv_buf_t>& buffers)
   {
      size_t buffer_size = reserved + sizeof(int32_t) + sizeof(int16_t) + message_size;
      buffers.push_back(uv_buf_init((char*) malloc(buffer_size), buffer_size));

      char* buffer = buffers.back().base + reserved;
      buffer       = encode_int(buffer, code);
      encode_string(buffer, message, message_size);
      return true;
   }

private:
   body_error_t(const body_error_t&) {}
   void operator=(const body_error_t&) {}
};

struct message_t {
   typedef std::deque<uv_buf_t> buffer_list_t;

   uint8_t       version;
   int8_t        flags;
   int8_t        stream;
   uint8_t       opcode;
   int32_t       length;
   int32_t       received;
   buffer_list_t buffers;
   body_t*       body;

   message_t() :
      version(0),
      flags(0),
      stream(0),
      opcode(0),
      length(0),
      received(0),
      body(0)
   {}

   ~message_t()
   {
      clear_buffers();
   }

   inline void
   clear_buffers()
   {
      length = 0;
      while (!buffers.empty()) {
         delete buffers.front().base;
         buffers.pop_front();
      }
      delete body;
   }

   inline static bool
   allocate_body(
      uint8_t  opcode,
      body_t** body)
   {
      switch (opcode) {
         case CQL_OPCODE_ERROR:
            *body = static_cast<body_t*>(new body_error_t);
            return true;
      }
      return false;
   }

   bool
   prepare_buffer()
   {
      if (body) {
         if (body->prepare(CQL_HEADER_SIZE, buffers)) {

            if (buffers.empty()) {
               buffers.push_front(uv_buf_init((char*) malloc(CQL_HEADER_SIZE), CQL_HEADER_SIZE));
            }
            else {
               for (buffer_list_t::iterator it = buffers.begin();
                    it != buffers.end();
                    ++it)
               {
                  length += it->len;
               }
               length = length - CQL_HEADER_SIZE;
            }

            uint8_t* buffer = (uint8_t*) buffers.front().base;
            buffer[0]       = version;
            buffer[1]       = flags;
            buffer[2]       = stream;
            buffer[3]       = opcode;
            encode_int((char*)(buffer + 4), length);
            return true;
         }
      }
      return false;
   }

   bool
   consume_buffer(
      uv_buf_t buf)
   {
      buffers.push_back(buf);

      if (received < CQL_HEADER_SIZE && received + buf.len >= CQL_HEADER_SIZE) {
         char* buffer = buffers.front().base;
         version      = *(buffer++);
         flags        = *(buffer++);
         stream       = *(buffer++);
         opcode       = *(buffer++);
         length       = ntohl(*buffer);
         received     = buffers.front().len;
      }
      else {
         received += buf.len;
      }

      if (received >= CQL_HEADER_SIZE) {
         if ((received - CQL_HEADER_SIZE) >= length) {
            if (allocate_body(opcode, &body)) {
               body->consume(CQL_HEADER_SIZE, buffers);
               return true;
            }
         }
      }
      return false;
   }
};

#endif
