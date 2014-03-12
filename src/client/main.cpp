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

    inline void
    log(
        int         level,
        const char* message)
    {
        (void) level;
        std::cout << message << std::endl;
    }

    inline void
    log(
        int                level,
        const std::string& message)
    {
        (void) level;
        std::cout << message << std::endl;
    }
};

#define STREAM_ID_MIN                    1
#define STREAM_ID_MAX                    127
#define STREAM_ID_COUNT                  STREAM_ID_MAX - STREAM_ID_MIN + 1
#define ADDRESS_MAX_LENGTH               46
#define CONNECTION_STATE_NEW             0
#define CONNECTION_STATE_RESOLVING       1
#define CONNECTION_STATE_RESOLVED        2
#define CONNECTION_STATE_CONNECTING      3
#define CONNECTION_STATE_CONNECTIED      4
#define CONNECTION_STATE_SSL_NEGOCIATING 5


struct client_connection_t {

    enum client_connection_state_t {
        CLIENT_STATE_NEW,
        CLIENT_STATE_RESOLVED,
        CLIENT_STATE_CONNECTED,
        CLIENT_STATE_HANDSHAKE,
        CLIENT_STATE_SUPPORTED,
        CLIENT_STATE_READY,
        CLIENT_STATE_KS_SET,
        CLIENT_STATE_PREPARING,
        CLIENT_STATE_ACCEPT_REQUESTS,
        CLIENT_STATE_DISCONNECTING,
        CLIENT_STATE_DISCONNECTED
    };

    enum compression_enum_t {
        CLIENT_COMPRESSION_NONE,
        CLIENT_COMPRESSION_SNAPPY,
        CLIENT_COMPRESSION_LZ4
    };

    typedef request_t<message_t, message_t> caller_request_t;
    typedef std::function<void(client_connection_t*, message_t*)> message_callback_t;

    struct write_req_data_t {
        uv_buf_t buf;
        client_connection_t* connection;
    };

    client_connection_state_t  state;
    context_t*                 context;
    std::unique_ptr<message_t> incomming;
    int8_t                     available_streams[STREAM_ID_COUNT];
    size_t                     available_streams_index;
    message_callback_t         callers[STREAM_ID_COUNT];
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
    uv_tcp_t                   socket;
    // ssl stuff
    ssl_session_t*             ssl;
    // supported stuff sent in start up message
    std::string                compression;
    std::string                cql_version;
    std::string                keyspace;

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
        ssl(NULL),
        cql_version("3.0.0")
    {
        for (int i = STREAM_ID_MIN; i < STREAM_ID_MAX + 1; ++i) {
            available_streams[i - STREAM_ID_MIN] = i;
        }
        resolver.data = this;
        connect_request.data = this;
        socket.data = this;

        resolver_hints.ai_family = address_family;
        resolver_hints.ai_socktype = SOCK_STREAM;
        resolver_hints.ai_protocol = IPPROTO_TCP;
        resolver_hints.ai_flags = 0;
        memset(address_string, 0, ADDRESS_MAX_LENGTH);
    }

    void
    event_received()
    {
        context->log(CQL_LOG_DEBUG, "event received");

        switch(state) {
            case CLIENT_STATE_NEW:
                resolve();
                break;
            case CLIENT_STATE_RESOLVED:
                connect();
                break;
            case CLIENT_STATE_CONNECTED:
                ssl_handshake();
                break;
            case CLIENT_STATE_HANDSHAKE:
                send_options();
                break;
            case CLIENT_STATE_SUPPORTED:
                send_startup();
                break;
            case CLIENT_STATE_READY:
                set_keyspace();
                break;
            case CLIENT_STATE_KS_SET:
                prepare_statements();
                break;
            case CLIENT_STATE_ACCEPT_REQUESTS:
                break;
            default:
                assert(false);
        }
    }

    void
    consume(
        char*  input,
        size_t size)
    {
        char* buffer    = input;
        int   remaining = size;

        while (remaining != 0) {
            int consumed = incomming->consume(buffer, remaining);
            if (consumed < 0) {
                // TODO
                fprintf(stderr, "consume error\n");
            }

            if (incomming->body_ready) {
                message_t* message = incomming.release();
                incomming.reset(new message_t());

                char log_message[512];
                snprintf(log_message, sizeof(log_message), "consumed message type %d with stream %d, input %zd, remaining %d", message->opcode, message->stream, size, remaining);
                context->log(CQL_LOG_DEBUG, log_message);
                if (message->stream > 0) {
                    // response to a message sent by us, call the registered callback
                    callers[message->stream](this, message);
                }
                else if (message->stream < 0) {
                    // system event
                    // TODO
                }
                else {
                    switch (message->opcode) {
                        case CQL_OPCODE_SUPPORTED:
                            on_supported(message);
                            break;
                        case CQL_OPCODE_ERROR:
                            on_error_stream_zero(message);
                            break;
                        case CQL_OPCODE_READY:
                            on_ready(message);
                            break;
                        default:
                            break;
                    }
                }
            }
            remaining -= consumed;
            buffer    += consumed;
        }
    }

    void
    ssl_handshake()
    {
        if (ssl) {
            // TODO
            // ssl->handshake(true);
        }
        else {
            state = CLIENT_STATE_HANDSHAKE;
            event_received();
        }
    }

    void
    on_error_stream_zero(
        message_t* response)
    {
        context->log(CQL_LOG_DEBUG, "on_error_stream_zero");
        body_error_t* error = static_cast<body_error_t*>(response->body.get());
        // TODO do something with the supported info
        context->log(CQL_LOG_ERROR, error->message);

        delete response;
        event_received();
    }

    void
    on_ready(
        message_t* response)
    {
        context->log(CQL_LOG_DEBUG, "on_ready");
        delete response;
        state = CLIENT_STATE_READY;
        event_received();
    }

    void
    on_supported(
        message_t* response)
    {
        context->log(CQL_LOG_DEBUG, "on_supported");
        body_supported_t* supported = static_cast<body_supported_t*>(response->body.get());
        // TODO do something with the supported info
        (void) supported;

        delete response;
        state = CLIENT_STATE_SUPPORTED;
        event_received();
    }

    void
    set_keyspace(
        std::string& ks)
    {
        (void) ks;
    }

    void
    prepare_statements()
    {

    }

    void
    set_keyspace()
    {
        if (!keyspace.empty()) {

        }
        else {
            state = CLIENT_STATE_KS_SET;
            event_received();
        }
        context->log(CQL_LOG_DEBUG, "set_keyspace");
        // message_t message(CQL_OPCODE_OPTIONS);
        // send_message(&message);
    }

    void
    send_options()
    {
        context->log(CQL_LOG_DEBUG, "send_options");
        message_t message(CQL_OPCODE_OPTIONS);
        send_message(&message);
    }

    void
    send_startup()
    {
        context->log(CQL_LOG_DEBUG, "send_startup");
        message_t message(CQL_OPCODE_STARTUP);
        body_startup_t* startup = static_cast<body_startup_t*>(message.body.get());
        startup->cql_version = cql_version;
        send_message(&message);
    }

    int
    send_message(
        message_t*         message,
        message_callback_t callback)
    {
        if (available_streams_index == 0) {
            return CQL_ERROR_NO_STREAMS;
        }

        int8_t stream_id = available_streams[available_streams_index--];
        callers[stream_id] = callback;
        message->stream = stream_id;
        return send_message(message);
    }

    int
    send_message(
        message_t* message)
    {
        uv_buf_t buf;
        message->prepare(&buf.base, buf.len);

        uv_write_t        *req  = new uv_write_t;
        write_req_data_t*  data = new write_req_data_t;
        data->buf               = buf;
        data->connection        = this;
        req->data               = data;
        uv_write(req, reinterpret_cast<uv_stream_t*>(&socket), &buf, 1, client_connection_t::on_write);
        return CQL_ERROR_NO_ERROR;
    }

    static void
    on_close(
        uv_handle_t* client)
    {
        client_connection_t* connection = reinterpret_cast<client_connection_t*>(client->data);
        context_t*           context    = connection->context;

        context->log(CQL_LOG_DEBUG, "on_close");
        connection->state = CLIENT_STATE_DISCONNECTED;
        connection->event_received();
    }

    void
    close()
    {
        context->log(CQL_LOG_DEBUG, "close");
        state = CLIENT_STATE_DISCONNECTING;
        uv_close(reinterpret_cast<uv_handle_t*>(&socket), client_connection_t::on_close);
    }

    static void
    on_read(
        uv_stream_t* client,
        ssize_t      nread,
        uv_buf_t     buf)
    {
        client_connection_t* connection = reinterpret_cast<client_connection_t*>(client->data);
        context_t*           context    = connection->context;

        context->log(CQL_LOG_DEBUG, "on_read");
        if (nread == -1) {
            if (uv_last_error(context->loop).code != UV_EOF) {
                fprintf(stderr, "Read error %s\n", uv_err_name(uv_last_error(context->loop)));
            }
            connection->close();
            return;
        }

        // TODO SSL
        connection->consume(buf.base, nread);
        free_buffer(buf);
    }

    static void
    on_write(
        uv_write_t* req,
        int         status)
    {
        write_req_data_t*    data       = reinterpret_cast<write_req_data_t*>(req->data);
        client_connection_t* connection = data->connection;
        context_t*           context    = connection->context;

        context->log(CQL_LOG_DEBUG, "on_write");
        if (status == -1) {
            fprintf(stderr, "Write error %s\n", uv_err_name(uv_last_error(context->loop)));
        }
        delete data->buf.base;
        delete data;
        delete req;
    }


    static void
    on_connect(
        uv_connect_t *request,
        int           status)
    {
        client_connection_t* connection = (client_connection_t*) request->data;
        context_t*           context    = connection->context;

        context->log(CQL_LOG_DEBUG, "on_connect");
        if (status == -1) {
            // TODO
            fprintf(stderr, "connect failed error %s\n", uv_err_name(uv_last_error(connection->context->loop)));
            return;
        }

        uv_read_start(reinterpret_cast<uv_stream_t*>(&connection->socket), alloc_buffer, on_read);
        connection->state = CLIENT_STATE_CONNECTED;
        connection->event_received();
    }

    void
    connect()
    {
        context->log(CQL_LOG_DEBUG, "connect");
        // connect to the resolved host
        uv_tcp_init(context->loop, &socket);
        uv_tcp_connect(&connect_request, &socket, address, client_connection_t::on_connect);
    }

    static void
    on_resolve(
        uv_getaddrinfo_t *resolver,
        int               status,
        struct addrinfo  *res)
    {
        client_connection_t* connection = (client_connection_t*) resolver->data;
        context_t*           context    = connection->context;

        context->log(CQL_LOG_DEBUG, "on_resolve");
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
    resolve()
    {
        context->log(CQL_LOG_DEBUG, "resolve");
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


int
main() {
    context_t context(uv_default_loop());
    client_connection_t connection(&context);
    connection.resolve();
    return uv_run(context.loop, UV_RUN_DEFAULT);

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
