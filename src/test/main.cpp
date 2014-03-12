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
#include "message.hpp"

char TEST_MESSAGE_ERROR[] = {
    0x81, 0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x0C, // header
    0xFF, 0xFF, 0xFF, 0xFF,                         // error code
    0x00, 0x06, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72  // message
};

char TEST_MESSAGE_OPTIONS[] = {
    0x02, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00  // header
};

char TEST_MESSAGE_STARTUP[] = {
    0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x16, // header
    0x00, 0x01,                                     // 1 entry
    0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f, 0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, // CQL_VERSION
    0x00, 0x05, 0x33, 0x2e, 0x30, 0x2e, 0x30        // 3.0.0
};

char TEST_MESSAGE_QUERY[] = {
    0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x22, // header
    0x00, 0x00, 0x00, 0x1b,                         // string length (27)
    0x53, 0x45, 0x4c, 0x45, 0x43, 0x54,             // SELECT
    0x20, 0x2a, 0x20,                               //  *
    0x46, 0x52, 0x4f, 0x4d, 0x20,                   // FROM
    0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e,       // peers;
    0x70, 0x65, 0x65, 0x72, 0x73, 0x3b,             // system.peers;
    0x00, 0x01,                                     // consistency
    0x00                                            // flags
};

char TEST_MESSAGE_QUERY_PAGING[] = {
    0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x2a, // header
    0x00, 0x00, 0x00, 0x1b,                         // string length (27)
    0x53, 0x45, 0x4c, 0x45, 0x43, 0x54,             // SELECT
    0x20, 0x2a, 0x20,                               //  *
    0x46, 0x52, 0x4f, 0x4d, 0x20,                   // FROM
    0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e,       // peers;
    0x70, 0x65, 0x65, 0x72, 0x73, 0x3b,             // system.peers;
    0x00, 0x01,                                     // consistency
    0x08,                                           // flags
    0x00, 0x06,                                     // length 6
    0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72              // foobar
};


#define TEST(x)          if (!x) { return -1; }
#define CHECK(x)         if (!x) { std::cerr << "TEST FAILED AT " << __FILE__ << ":" << __LINE__ << std::endl; return false; }
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
test_error_consume()
{
   message_t message;
   CHECK_EQUAL(message.consume(TEST_MESSAGE_ERROR, sizeof(TEST_MESSAGE_ERROR)), sizeof(TEST_MESSAGE_ERROR));
   CHECK_EQUAL((int) message.version, 0x81);
   CHECK_EQUAL((int) message.flags, 0x01);
   CHECK_EQUAL((int) message.stream, 0x7F);
   CHECK_EQUAL((int) message.opcode, 0x00);
   CHECK_EQUAL((int) message.length, 0x0C);
   return true;
}

bool
test_error_prepare()
{
   message_t message;
   message.version = 0x81;
   message.flags   = 0x01;
   message.stream  = 0x7F;
   message.opcode  = 0x00;
   message.body.reset(new body_error_t(0xFFFFFFFF, (const char*)"foobar", 6));

   std::unique_ptr<char> buffer;
   char*                 buffer_ptr;
   size_t                size;
   CHECK(message.prepare(&buffer_ptr, size));
   buffer.reset(buffer_ptr);

   CHECK_EQUAL(sizeof(TEST_MESSAGE_ERROR), size);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_ERROR, buffer.get(), sizeof(TEST_MESSAGE_ERROR)), 0);
   return true;
}

bool
test_options_prepare()
{
   message_t             message(CQL_OPCODE_OPTIONS);
   std::unique_ptr<char> buffer;
   char*                 buffer_ptr;
   size_t                size;

   CHECK(message.body);
   CHECK(message.prepare(&buffer_ptr, size));
   buffer.reset(buffer_ptr);

   CHECK_EQUAL(sizeof(TEST_MESSAGE_OPTIONS), size);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_OPTIONS, buffer.get(), sizeof(TEST_MESSAGE_OPTIONS)), 0);
   return true;
}

bool
test_startup_prepare()
{
   message_t             message(CQL_OPCODE_STARTUP);
   std::unique_ptr<char> buffer;
   char*                 buffer_ptr;
   size_t                size;

   CHECK(message.body);
   CHECK(message.prepare(&buffer_ptr, size));
   buffer.reset(buffer_ptr);

   CHECK_EQUAL(sizeof(TEST_MESSAGE_STARTUP), size);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_STARTUP, buffer.get(), sizeof(TEST_MESSAGE_STARTUP)), 0);
   return true;
}

bool
test_query_query()
{
   message_t             message(CQL_OPCODE_QUERY);
   std::unique_ptr<char> buffer;
   char*                 buffer_ptr;
   size_t                size;

   CHECK(message.body);
   body_query_t* query = static_cast<body_query_t*>(message.body.get());
   query->query_string("SELECT * FROM system.peers;");
   query->consistency(CQL_CONSISTENCY_ONE);

   CHECK(message.prepare(&buffer_ptr, size));
   buffer.reset(buffer_ptr);
   CHECK_EQUAL(sizeof(TEST_MESSAGE_QUERY), size);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_QUERY, buffer.get(), sizeof(TEST_MESSAGE_QUERY)), 0);
   return true;
}

bool
test_query_query_paging()
{
   message_t             message(CQL_OPCODE_QUERY);
   std::unique_ptr<char> buffer;
   char*                 buffer_ptr;
   size_t                size;
   const char*           paging_state = "foobar";

   CHECK(message.body);
   body_query_t* query = static_cast<body_query_t*>(message.body.get());
   query->query_string("SELECT * FROM system.peers;");
   query->consistency(CQL_CONSISTENCY_ONE);
   query->paging_state(paging_state, strlen(paging_state));

   CHECK(message.prepare(&buffer_ptr, size));
   buffer.reset(buffer_ptr);

   print_hex(buffer_ptr, size);
   std::cout << std::endl;

   print_hex(TEST_MESSAGE_QUERY_PAGING, sizeof(TEST_MESSAGE_QUERY_PAGING));
   std::cout << std::endl;

   CHECK_EQUAL(sizeof(TEST_MESSAGE_QUERY_PAGING), size);
   CHECK_EQUAL(memcmp(TEST_MESSAGE_QUERY_PAGING, buffer.get(), sizeof(TEST_MESSAGE_QUERY_PAGING)), 0);
   return true;
}

bool
ssl_test()
{
    // ssl_context_t ssl_client_context;
    // ssl_client_context.init(true, true);

    // ssl_context_t ssl_server_context;
    // ssl_server_context.init(true, false);

    // RSA* rsa = ssl_context_t::create_key(2048);
    // if (!rsa) {
    //     std::cerr << "create_key" << std::endl;
    //     return 1;
    // }

    // const char* pszCommonName = "test name";
    // X509* cert = ssl_context_t::create_cert(rsa, rsa, pszCommonName, pszCommonName, "DICE", 3 * 365 * 24 * 60 * 60);
    // if (!cert) {
    //     std::cerr << "Couldn't create a certificate" << std::endl;
    //     return 1;
    // }
    // ssl_server_context.use_key(rsa);
    // ssl_server_context.use_cert(cert);

    // std::auto_ptr<ssl_session_t> client_session(ssl_client_context.session_new());
    // std::auto_ptr<ssl_session_t> server_session(ssl_server_context.session_new());

    // client_session->handshake(true);
    // server_session->handshake(false);

    // std::deque<uv_buf_t> client_write_input;
    // std::deque<uv_buf_t> client_write_output;
    // std::deque<uv_buf_t> client_read_output;

    // std::deque<uv_buf_t> server_write_input;
    // std::deque<uv_buf_t> server_write_output;
    // std::deque<uv_buf_t> server_read_output;


    // for (;;) {

    //     std::cout << "client_session->read_write " << client_session->read_write(server_write_output, client_read_output, client_write_input, client_write_output) << std::endl;
    //     for (std::deque<uv_buf_t>::const_iterator it = client_read_output.begin();
    //          it != client_read_output.end();
    //          ++it)
    //     {
    //         std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
    //     }
    //     clear_buffer_deque(server_write_output);
    //     clear_buffer_deque(client_read_output);

    //     std::cout << "server_session->read_write " << server_session->read_write(client_write_output, server_read_output, server_write_input, server_write_output) << std::endl;
    //     for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
    //          it != server_read_output.end();
    //          ++it)
    //     {
    //         std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
    //     }
    //     clear_buffer_deque(client_write_output);
    //     clear_buffer_deque(server_read_output);

    //     if (server_session->handshake_done() && client_session->handshake_done()) {
    //         char ciphers[1024];
    //         std::cout << client_session->ciphers(ciphers, sizeof(ciphers)) << std::endl;
    //         std::cout << server_session->ciphers(ciphers, sizeof(ciphers)) << std::endl;
    //         break;
    //     }
    // }

    // const char* client_input_data   = "foobar                  ";
    // uv_buf_t    client_input_buffer = uv_buf_init((char*)client_input_data, strlen(client_input_data));
    // client_write_input.push_back(client_input_buffer);

    // const char* server_input_data   = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\nConnection: close\r\n\r\nthis is a test";
    // uv_buf_t    server_input_buffer = uv_buf_init((char*)server_input_data, strlen(server_input_data));
    // server_write_input.push_back(server_input_buffer);


    // for (;;) {
    //     std::cout << "client_session->read_write " << client_session->read_write(server_write_output, client_read_output, client_write_input, client_write_output) << std::endl;
    //     client_write_input.clear();
    //     for (std::deque<uv_buf_t>::const_iterator it = client_read_output.begin();
    //          it != client_read_output.end();
    //          ++it)
    //     {
    //         std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << " buf.base " << std::string(it->base, it->len) << std::endl;
    //     }
    //     clear_buffer_deque(server_write_output);
    //     clear_buffer_deque(client_read_output);

    //     std::cout << "server_session->read_write " << server_session->read_write(client_write_output, server_read_output, server_write_input, server_write_output) << std::endl;
    //     server_write_input.clear();
    //     for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
    //          it != server_read_output.end();
    //          ++it)
    //     {
    //         std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << " buf.base " << std::string(it->base, it->len) << std::endl;
    //     }
    //     clear_buffer_deque(client_write_output);
    //     clear_buffer_deque(server_read_output);

    //     if (client_read_output.empty() && server_read_output.empty() && client_write_output.empty() && server_write_output.empty()) {
    //         break;
    //     }
    // }

    //client_write_input.push_back(input_buffer);
    // std::cout << "client_session->write_ssl " << client_session->write_ssl(client_write_input, client_write_output) << std::endl;
    // std::cout << "server_session->read_ssl " << server_session->read_ssl(client_write_output, server_read_output) << std::endl;
    // for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
    //      it != server_read_output.end();
    //      ++it)
    // {
    //     std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
    // }
    return true;
}

int
main() {
   TEST(test_error_consume());
   TEST(test_error_prepare());
   TEST(test_options_prepare());
   TEST(test_startup_prepare());
   TEST(test_query_query());
   TEST(test_query_query_paging());
   return 0;
}
