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

#include <iostream>
#include <stdio.h>

#include "common.hpp"

char TEST_MESSAGE_ERROR[] = {
   0x81, 0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x06, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72
};

#define TEST(x) if (!x) { return -1; }
#define CHECK(x) if (!x) { std::cerr << "TEST FAILED AT " << __FILE__ << ":" << __LINE__ << std::endl; return false; }
#define CHECK_EQUAL(x,y) if (x != y) { std::cerr << "TEST FAILED AT " << __FILE__ << ":" << __LINE__  << " " << x << " != " << y << std::endl; return false; }

void
print_hex(
   char*  value,
   size_t size)
{
   for (size_t i = 0; i < size; ++i) {
      printf("%02.2x ", (unsigned)(unsigned char) value[i]);
   }
}

bool
test_header_consume()
{
   char* buffer = new char[sizeof(TEST_MESSAGE_ERROR)];
   memcpy(buffer, TEST_MESSAGE_ERROR, sizeof(TEST_MESSAGE_ERROR));
   uv_buf_t buf = uv_buf_init(buffer, sizeof(TEST_MESSAGE_ERROR));

   message_t message;
   CHECK(message.consume_buffer(buf));
   CHECK_EQUAL(message.version, 0x81);
   CHECK_EQUAL(message.flags, 0x01);
   CHECK_EQUAL(message.stream, 0x7F);
   CHECK_EQUAL(message.opcode, 0x00);
   CHECK_EQUAL(message.length, 0x00);
   return true;
}

bool
test_header_prepare()
{
   message_t message;
   message.version = 0x81;
   message.flags = 0x01;
   message.stream = 0x7F;
   message.opcode = 0x00;
   message.body = new body_error_t(0xFFFFFFFF, (const char*)"foobar", 6);

   CHECK(message.prepare_buffer());
   CHECK_EQUAL(sizeof(TEST_MESSAGE_ERROR), message.buffers.front().len);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_ERROR, message.buffers.front().base, sizeof(TEST_MESSAGE_ERROR)), 0);
   return true;
}

int
main() {
   TEST(test_header_consume());
   TEST(test_header_prepare());
   return 0;
}
