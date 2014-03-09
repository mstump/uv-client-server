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
#include "message.hpp"
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

    enum client_connection_state_t {
        CLIENT_STATE_NEW,
        CLIENT_STATE_RESOLVED,
        CLIENT_STATE_CONNECTED,
        CLIENT_STATE_SUPPORTED,
        CLIENT_STATE_READY,
        CLIENT_STATE_DISCONNECTING,
        CLIENT_STATE_DISCONNECTED
    };

    typedef request_t<message_t, message_t> caller_request_t;

    client_connection_state_t  state;
    context_t*                 context;
    std::unique_ptr<message_t> incomming;
    int8_t                     available_streams[STREAM_ID_COUNT];
    size_t                     available_streams_index;
    caller_request_t*          callers[STREAM_ID_COUNT];
    // DNS and hostname stuff
    struct sockaddr_in         address;
    char*                      address_string[46]; // max length for ipv6
    int                        address_family;
    std::string                hostname;
    std::string                port;
    uv_getaddrinfo_t           resolver;
    struct addrinfo            resolver_hints;
    // the actual connection
    uv_connect_t               connect_request;
    uv_tcp_t*                  socket;
    // ssl stuff
    ssl_session_t*             ssl;

    explicit
    client_connection_t(
        context_t* context) :
        state(CLIENT_STATE_NEW),
        context(context),
        incomming(new message_t()),
        available_streams_index(STREAM_ID_MAX),
        address_family(PF_INET), // use ipv4 by default
        hostname("localhost"),
        port("9042"),
        socket(NULL),
        ssl(NULL)
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
        memset(address_string, 0, ADDRESS_MAX_LENGTH);
    }

    void
    event_received()
    {
        switch(state) {
            case CLIENT_STATE_NEW:
                hostname_resolve();
                break;
            case CLIENT_STATE_RESOLVED:
                connect();
                break;
            case CLIENT_STATE_CONNECTED:
                if (ssl) {
                    ssl_handshake();
                }
                else {
                    send_options();
                }
                break;
            case CLIENT_STATE_SUPPORTED:
                send_startup();
                break;
            case CLIENT_STATE_READY:
                // send_messages
                break;
            default:
                break;
        }
    }

    void
    consume_buffer(
        uv_buf_t buf)
    {
        if (incomming->consume(buf.base, buf.len)) {
            incomming.reset(new message_t());
        }
    }

    void
    ssl_handshake()
    {
        ssl->handshake(true);
    }

    void
    send_options()
    {
        message_t* message = new message_t(CQL_OPCODE_OPTIONS);
        (void) message;
    }

    void
    send_startup()
    {
        message_t* message = new message_t(CQL_OPCODE_STARTUP);
        (void) message;
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
    on_close(
        uv_handle_t* client)
    {
        client_connection_t* connection = reinterpret_cast<client_connection_t*>(client->data);
        connection->state               = CLIENT_STATE_DISCONNECTED;
        connection->event_received();
    }

    void
    close()
    {
        state = CLIENT_STATE_DISCONNECTING;
        uv_close(reinterpret_cast<uv_handle_t*>(socket), client_connection_t::on_close);
    }

    void
    on_supported(
        message_t* message)
    {
        (void) message;
    }

    static void
    on_read(
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
            connection->close();
            return;
        }

        // TODO SSL
        size_t read = connection->incomming->consume(buf.base, buf.len);
        if (buf.len != read) {
            // TODO
            // the existing message got everything it needs, do something with it.
            connection->incomming.reset(new message_t());
        }
        // TODO
        free_buffer(buf);
        connection->event_received();
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

        uv_read_start((uv_stream_t*) request->data, alloc_buffer, on_read);
        connection->state = CLIENT_STATE_CONNECTED;
        connection->event_received();
    }

    void
    connect()
    {
        // connect to the resolved host
        socket = new uv_tcp_t;
        uv_tcp_init(context->loop, socket);
        uv_tcp_connect(&connect_request, socket, address, client_connection_t::on_connect);
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
            uv_ip4_name((struct sockaddr_in*) res->ai_addr,
                        (char*) connection->address_string,
                        ADDRESS_MAX_LENGTH);
        }
        else if (res->ai_family == AF_INET6) {
            uv_ip6_name((struct sockaddr_in6*) res->ai_addr,
                        (char*) connection->address_string,
                        ADDRESS_MAX_LENGTH);
        }
        connection->address = *(struct sockaddr_in*) res->ai_addr;
        uv_freeaddrinfo(res);

        connection->state = CLIENT_STATE_RESOLVED;
        connection->event_received();
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
    ssl_client_context.init(true, true);

    ssl_context_t ssl_server_context;
    ssl_server_context.init(true, false);

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

    std::deque<uv_buf_t> client_write_input;
    std::deque<uv_buf_t> client_write_output;
    std::deque<uv_buf_t> client_read_output;

    std::deque<uv_buf_t> server_write_input;
    std::deque<uv_buf_t> server_write_output;
    std::deque<uv_buf_t> server_read_output;


    for (;;) {

        std::cout << "client_session->read_write " << client_session->read_write(server_write_output, client_read_output, client_write_input, client_write_output) << std::endl;
        for (std::deque<uv_buf_t>::const_iterator it = client_read_output.begin();
             it != client_read_output.end();
             ++it)
        {
            std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
        }
        clear_buffer_deque(server_write_output);
        clear_buffer_deque(client_read_output);

        std::cout << "server_session->read_write " << server_session->read_write(client_write_output, server_read_output, server_write_input, server_write_output) << std::endl;
        for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
             it != server_read_output.end();
             ++it)
        {
            std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
        }
        clear_buffer_deque(client_write_output);
        clear_buffer_deque(server_read_output);

        if (server_session->handshake_done() && client_session->handshake_done()) {
            char ciphers[1024];
            std::cout << client_session->ciphers(ciphers, sizeof(ciphers)) << std::endl;
            std::cout << server_session->ciphers(ciphers, sizeof(ciphers)) << std::endl;
            break;
        }
    }

    const char* client_input_data   = "foobar                  ";
    uv_buf_t    client_input_buffer = uv_buf_init((char*)client_input_data, strlen(client_input_data));
    client_write_input.push_back(client_input_buffer);

    const char* server_input_data   = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 14\r\nConnection: close\r\n\r\nthis is a test";
    uv_buf_t    server_input_buffer = uv_buf_init((char*)server_input_data, strlen(server_input_data));
    server_write_input.push_back(server_input_buffer);


    for (;;) {
        std::cout << "client_session->read_write " << client_session->read_write(server_write_output, client_read_output, client_write_input, client_write_output) << std::endl;
        client_write_input.clear();
        for (std::deque<uv_buf_t>::const_iterator it = client_read_output.begin();
             it != client_read_output.end();
             ++it)
        {
            std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << " buf.base " << std::string(it->base, it->len) << std::endl;
        }
        clear_buffer_deque(server_write_output);
        clear_buffer_deque(client_read_output);

        std::cout << "server_session->read_write " << server_session->read_write(client_write_output, server_read_output, server_write_input, server_write_output) << std::endl;
        server_write_input.clear();
        for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
             it != server_read_output.end();
             ++it)
        {
            std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << " buf.base " << std::string(it->base, it->len) << std::endl;
        }
        clear_buffer_deque(client_write_output);
        clear_buffer_deque(server_read_output);

        if (client_read_output.empty() && server_read_output.empty() && client_write_output.empty() && server_write_output.empty()) {
            break;
        }
    }

    //client_write_input.push_back(input_buffer);
    // std::cout << "client_session->write_ssl " << client_session->write_ssl(client_write_input, client_write_output) << std::endl;
    // std::cout << "server_session->read_ssl " << server_session->read_ssl(client_write_output, server_read_output) << std::endl;
    // for (std::deque<uv_buf_t>::const_iterator it = server_read_output.begin();
    //      it != server_read_output.end();
    //      ++it)
    // {
    //     std::cout << __FILE__ << ":" << __LINE__ << " buf.len " << it->len << std::endl;
    // }

    return 0;

    // context_t context(uv_default_loop());
    // client_connection_t connection(&context);
    // connection.hostname_resolve();
    // return uv_run(context.loop, UV_RUN_DEFAULT);

    // uv_tcp_t server;
    // uv_tcp_init(context.loop, &server);
    // server.data = &context;

    // struct sockaddr_in bind_addr = uv_ip4_addr("127.0.0.1", 7777);
    // uv_tcp_bind(&server, bind_addr);

    // if (uv_listen(reinterpret_cast<uv_stream_t*>(&server), 128, on_new_connection)) {
    //     fprintf(stderr, "Listen error %s\n", uv_err_name(uv_last_error(context.loop)));
    //     return 1;
    // }
    // return uv_run(context.loop, UV_RUN_DEFAULT);
}
