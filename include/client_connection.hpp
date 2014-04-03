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

#ifndef __CLIENT_CONNECTION_HPP_INCLUDED__
#define __CLIENT_CONNECTION_HPP_INCLUDED__

#include "client_context.hpp"
#include "common.hpp"
#include "message.hpp"
#include "request.hpp"
#include "ssl_context.hpp"
#include "ssl_session.hpp"
#include "stream_storage.hpp"

#define CQL_ADDRESS_MAX_LENGTH 46
#define CQL_STREAM_ID_MAX      127

namespace cql {

struct ClientConnection {
  enum ClientConnectionState {
    CLIENT_STATE_NEW,
    CLIENT_STATE_RESOLVED,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_HANDSHAKE,
    CLIENT_STATE_SUPPORTED,
    CLIENT_STATE_READY,
    CLIENT_STATE_DISCONNECTING,
    CLIENT_STATE_DISCONNECTED
  };

  enum Compression {
    CLIENT_COMPRESSION_NONE,
    CLIENT_COMPRESSION_SNAPPY,
    CLIENT_COMPRESSION_LZ4
  };

  enum SchemaEventType {
    CLIENT_EVENT_SCHEMA_CREATED,
    CLIENT_EVENT_SCHEMA_UPDATED,
    CLIENT_EVENT_SCHEMA_DROPPED
  };

  typedef int8_t Stream;

  typedef cql::Request<std::string, Error*, Message*> CallerRequest;

  typedef std::function<void(ClientConnection*,
                             Error*)> ConnectionCallback;

  typedef std::function<void(ClientConnection*,
                             const char*, size_t)> KeyspaceCallback;

  typedef std::function<void(ClientConnection*,
                             SchemaEventType,
                             const char*, size_t,
                             const char*, size_t)> SchemaCallback;

  typedef std::function<void(ClientConnection*,
                             Error*,
                             const char*, size_t,
                             const char*, size_t)> PrepareCallback;

  typedef std::function<void(ClientConnection*, Error*)> ErrorCallback;

  typedef cql::StreamStorage<
    Stream,
    CallerRequest*,
    CQL_STREAM_ID_MAX> StreamStorageCollection;

  struct WriteRequestData {
    uv_buf_t buf;
    ClientConnection* connection;
  };

  ClientConnectionState    state;
  ClientContext*           context;
  std::unique_ptr<Message> incomming;
  StreamStorageCollection  stream_storage;
  ConnectionCallback       connect_callback;
  KeyspaceCallback         keyspace_callback;
  PrepareCallback          prepare_callback;
  ErrorCallback            error_callback;

  // DNS and hostname stuff
  struct sockaddr_in       address;
  char*                    address_string[CQL_ADDRESS_MAX_LENGTH];
  int                      address_family;
  std::string              hostname;
  std::string              port;
  uv_getaddrinfo_t         resolver;
  struct addrinfo          resolver_hints;

  // the actual connection
  uv_connect_t             connect_request;
  uv_tcp_t                 socket;
  // ssl stuff
  SSLSession*              ssl;
  bool                     ssl_handshake_done;

  // supported stuff sent in start up message
  std::string              compression;
  std::string              cql_version;

  explicit
  ClientConnection(
      ClientContext* context) :
      state(CLIENT_STATE_NEW),
      context(context),
      incomming(new Message()),
      address_family(PF_INET),  // use ipv4 by default
      hostname("localhost"),
      port("9042"),
      ssl(context->ssl_session_new()),
      ssl_handshake_done(false),
      cql_version("3.0.0") {
    resolver.data = this;
    connect_request.data = this;
    socket.data = this;

    resolver_hints.ai_family = address_family;
    resolver_hints.ai_socktype = SOCK_STREAM;
    resolver_hints.ai_protocol = IPPROTO_TCP;
    resolver_hints.ai_flags = 0;
    memset(address_string, 0, sizeof(address_string));
    if (ssl) {
      ssl->init();
      ssl->handshake(true);
    }
  }

  void
  event_received() {
    context->log(CQL_LOG_DEBUG, "event received");

    switch (state) {
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
        notify_ready();
        break;
      default:
        assert(false);
    }
  }

  void
  consume(
      char*  input,
      size_t size) {
    char* buffer    = input;
    int   remaining = size;

    while (remaining != 0) {
      int consumed = incomming->consume(buffer, remaining);
      if (consumed < 0) {
        // TODO(mstump)
        fprintf(stderr, "consume error\n");
      }

      if (incomming->body_ready) {
        Message* message = incomming.release();
        incomming.reset(new Message());

        char log_message[512];
        snprintf(
            log_message,
            sizeof(log_message),
            "consumed message type %s with stream %d, input %zd, remaining %d",
            opcode_to_string(message->opcode).c_str(),
            message->stream,
            size,
            remaining);

        context->log(CQL_LOG_DEBUG, log_message);
        if (message->stream < 0) {
          // system event
          // TODO(mstump)
          assert(false);
        } else {
          switch (message->opcode) {
            case CQL_OPCODE_SUPPORTED:
              on_supported(message);
              break;
            case CQL_OPCODE_ERROR:
              on_error(message);
              break;
            case CQL_OPCODE_READY:
              on_ready(message);
              break;
            case CQL_OPCODE_RESULT:
              on_result(message);
              break;
            default:
              assert(false);
              break;
          }
        }
      }
      remaining -= consumed;
      buffer    += consumed;
    }
  }

  static void
  on_close(
      uv_handle_t* client) {
    ClientConnection* connection
        = reinterpret_cast<ClientConnection*>(client->data);

    ClientContext* context = connection->context;

    context->log(CQL_LOG_DEBUG, "on_close");
    connection->state = CLIENT_STATE_DISCONNECTED;
    connection->event_received();
  }

  static void
  on_read(
      uv_stream_t* client,
      ssize_t      nread,
      uv_buf_t     buf) {
    ClientConnection* connection =
        reinterpret_cast<ClientConnection*>(client->data);

    ClientContext* context = connection->context;

    context->log(CQL_LOG_DEBUG, "on_read");
    if (nread == -1) {
      if (uv_last_error(context->loop).code != UV_EOF) {
        fprintf(stderr,
                "Read error %s\n",
                uv_err_name(uv_last_error(context->loop)));
      }
      connection->close();
      return;
    }

    if (connection->ssl) {
      char*  read_input        = buf.base;
      size_t read_input_size   = nread;

      for (;;) {
        size_t read_size         = 0;
        char*  read_output       = NULL;
        size_t read_output_size  = 0;
        char*  write_output      = NULL;
        size_t write_output_size = 0;

        // TODO(mstump) error handling
        connection->ssl->read_write(
            read_input,
            read_input_size,
            read_size,
            &read_output,
            read_output_size,
            NULL,
            0,
            &write_output,
            write_output_size);

        if (read_output && read_output_size) {
          // TODO(mstump) error handling
          connection->consume(read_output, read_output_size);
          delete read_output;
        }

        if (write_output && write_output_size) {
          connection->send_data(write_output, write_output_size);
          // delete of write_output will be handled by on_write
        }

        if (read_size < read_input_size) {
          read_input += read_size;
          read_input_size -= read_size;
        } else {
          break;
        }

        if (!connection->ssl_handshake_done) {
          if (connection->ssl->handshake_done()) {
            connection->state = CLIENT_STATE_HANDSHAKE;
            connection->event_received();
          }
        }
      }
    } else {
      connection->consume(buf.base, nread);
    }
    free_buffer(buf);
  }

  Error*
  send_data(
      char*  input,
      size_t size) {
    return send_data(uv_buf_init(input, size));
  }

  Error*
  send_data(
      uv_buf_t buf) {
    uv_write_t        *req  = new uv_write_t;
    WriteRequestData*  data = new WriteRequestData;
    data->buf               = buf;
    data->connection        = this;
    req->data               = data;
    uv_write(
        req,
        reinterpret_cast<uv_stream_t*>(&socket),
        &buf,
        1,
        ClientConnection::on_write);
    return CQL_ERROR_NO_ERROR;
  }

  void
  close() {
    context->log(CQL_LOG_DEBUG, "close");
    state = CLIENT_STATE_DISCONNECTING;
    uv_close(
        reinterpret_cast<uv_handle_t*>(&socket),
        ClientConnection::on_close);
  }

  static void
  on_connect(
      uv_connect_t*     request,
      int               status) {
    ClientConnection* connection
        = reinterpret_cast<ClientConnection*>(request->data);

    ClientContext* context = connection->context;

    context->log(CQL_LOG_DEBUG, "on_connect");
    if (status == -1) {
      // TODO(mstump)
      fprintf(
          stderr,
          "connect failed error %s\n",
          uv_err_name(uv_last_error(connection->context->loop)));
      return;
    }

    uv_read_start(
        reinterpret_cast<uv_stream_t*>(&connection->socket),
        alloc_buffer,
        on_read);

    connection->state = CLIENT_STATE_CONNECTED;
    connection->event_received();
  }

  void
  connect() {
    context->log(CQL_LOG_DEBUG, "connect");
    // connect to the resolved host
    uv_tcp_init(context->loop, &socket);
    uv_tcp_connect(
        &connect_request,
        &socket,
        address,
        ClientConnection::on_connect);
  }

  void
  ssl_handshake() {
    if (ssl) {
      // calling read on a handshaked initiated ssl pipe
      // will gives us the first message to send to the server
      on_read(
          reinterpret_cast<uv_stream_t*>(&socket),
          0,
          alloc_buffer(0));
    } else {
      state = CLIENT_STATE_HANDSHAKE;
      event_received();
    }
  }

  void
  on_result(
      Message* response) {
    context->log(CQL_LOG_DEBUG, "on_result");

    Error*         err     = NULL;
    CallerRequest* request = NULL;
    BodyResult*    result  = static_cast<BodyResult*>(response->body.get());

    switch (result->kind) {
      case CQL_RESULT_KIND_SET_KEYSPACE:
        if (keyspace_callback) {
          keyspace_callback(this, result->keyspace, result->keyspace_size);
        }
        break;

      case CQL_RESULT_KIND_PREPARED:
        err = stream_storage.get_stream(response->stream, request);
        if (prepare_callback) {
          if (!err) {
            prepare_callback(
                this,
                NULL,
                request->data.c_str(),
                request->data.size(),
                result->prepared,
                result->prepared_size);
          } else {
            prepare_callback(
                this,
                err,
                request->data.c_str(),
                request->data.size(),
                result->prepared,
                result->prepared_size);
          }
        }

        if (!err) {
          request->result = response;
          request->notify(context->loop);
        } else {
          delete err;
        }
        break;

      default:
        err = stream_storage.get_stream(response->stream, request);
        if (!err) {
          request->result = response;
          request->notify(context->loop);
        } else {
          if (error_callback) {
            error_callback(this, err);
          }
        }
        break;
    }
  }

  void
  on_error(
      Message* response) {
    context->log(CQL_LOG_DEBUG, "on_error");
    BodyError* error = static_cast<BodyError*>(response->body.get());

    if (state < CLIENT_STATE_READY) {
      notify_error(
          new Error(
              CQL_ERROR_SOURCE_SERVER,
              0,
              error->message,
              __FILE__,
              __LINE__));
    }
    delete response;
  }

  void
  on_ready(
      Message* response) {
    context->log(CQL_LOG_DEBUG, "on_ready");
    delete response;
    state = CLIENT_STATE_READY;
    event_received();
  }

  void
  on_supported(
      Message* response) {
    context->log(CQL_LOG_DEBUG, "on_supported");
    BodySupported* supported
        = static_cast<BodySupported*>(response->body.get());

    // TODO(mstump) do something with the supported info
    (void) supported;

    delete response;
    state = CLIENT_STATE_SUPPORTED;
    event_received();
  }

  void
  set_keyspace(
      const std::string& keyspace) {
    Message    message(CQL_OPCODE_QUERY);
    BodyQuery* query = static_cast<BodyQuery*>(message.body.get());
    query->query_string("USE " + keyspace);
    send_message(&message, NULL);
  }

  void
  notify_ready() {
    context->log(CQL_LOG_DEBUG, "notify_ready");
    if (connect_callback) {
      connect_callback(this, NULL);
    }
  }

  void
  notify_error(
      Error* err) {
    context->log(CQL_LOG_DEBUG, "notify_error");
    if (connect_callback) {
      connect_callback(this, err);
    }
  }

  void
  send_options() {
    context->log(CQL_LOG_DEBUG, "send_options");
    Message message(CQL_OPCODE_OPTIONS);
    send_message(&message, NULL);
  }

  void
  send_startup() {
    context->log(CQL_LOG_DEBUG, "send_startup");
    Message      message(CQL_OPCODE_STARTUP);
    BodyStartup* startup = static_cast<BodyStartup*>(message.body.get());
    startup->cql_version = cql_version;
    send_message(&message, NULL);
  }

  static void
  on_write(
      uv_write_t* req,
      int         status) {
    WriteRequestData* data
        = reinterpret_cast<WriteRequestData*>(req->data);

    ClientConnection* connection = data->connection;
    ClientContext*    context    = connection->context;

    context->log(CQL_LOG_DEBUG, "on_write");
    if (status == -1) {
      fprintf(
          stderr,
          "Write error %s\n",
          uv_err_name(uv_last_error(context->loop)));
    }
    delete data->buf.base;
    delete data;
    delete req;
  }

  CallerRequest*
  prepare(
      const char*             statement,
      size_t                  size,
      CallerRequest::Callback callback = NULL) {
    CallerRequest* request = new CallerRequest();
    Message*       message = new Message(CQL_OPCODE_PREPARE);
    BodyPrepare*   prepare = static_cast<BodyPrepare*>(message->body.get());
    prepare->prepare_string(statement, size);

    request->callback = callback;
    request->data.assign(statement, size);

    Error* err = send_message(message, request);
    if (err) {
      request->error = err;
      request->notify(context->loop);
    }
    return request;
  }

  CallerRequest*
  exec(
      Message*                message,
      CallerRequest::Callback callback = NULL) {
    CallerRequest* request = new CallerRequest();
    request->callback = callback;
    Error* err = send_message(message, request);
    if (err) {
      request->error = err;
      request->notify(context->loop);
    }
    return request;
  }

  Error*
  send_message(
      Message* message,
      CallerRequest* request = NULL) {
    uv_buf_t   buf;
    Error*     err = stream_storage.set_stream(request, message->stream);
    if (err) {
      return err;
    }
    message->prepare(&buf.base, buf.len);

    char log_message[512];
    snprintf(
        log_message,
        sizeof(log_message),
        "sending message type %s with stream %d, size %zd",
        opcode_to_string(message->opcode).c_str(),
        message->stream,
        buf.len);

    context->log(CQL_LOG_DEBUG, log_message);
    return send_data(buf);
  }

  static void
  on_resolve(
      uv_getaddrinfo_t* resolver,
      int               status,
      struct addrinfo*  res) {
    ClientConnection* connection
        = reinterpret_cast<ClientConnection*>(resolver->data);

    ClientContext* context = connection->context;

    context->log(CQL_LOG_DEBUG, "on_resolve");
    if (status == -1) {
      // TODO(mstump)
      fprintf(
          stderr,
          "getaddrinfo request error %s\n",
          uv_err_name(uv_last_error(connection->context->loop)));
      return;
    }

    // store the human readable address
    if (res->ai_family == AF_INET) {
      uv_ip4_name((struct sockaddr_in*) res->ai_addr,
                  reinterpret_cast<char*>(connection->address_string),
                  CQL_ADDRESS_MAX_LENGTH);
    } else if (res->ai_family == AF_INET6) {
      uv_ip6_name((struct sockaddr_in6*) res->ai_addr,
                  reinterpret_cast<char*>(connection->address_string),
                  CQL_ADDRESS_MAX_LENGTH);
    }
    connection->address = *(struct sockaddr_in*) res->ai_addr;
    uv_freeaddrinfo(res);

    connection->state = CLIENT_STATE_RESOLVED;
    connection->event_received();
  }

  void
  resolve() {
    context->log(CQL_LOG_DEBUG, "resolve");
    uv_getaddrinfo(
        context->loop,
        &resolver,
        ClientConnection::on_resolve,
        hostname.c_str(),
        port.c_str(),
        &resolver_hints);
  }

  void
  init(
      ConnectionCallback connect     = NULL,
      ErrorCallback      error       = NULL,
      KeyspaceCallback   keyspace    = NULL
      // SchemaCallback     schema   = NULL,
      // TopologyCallback   topology = NULL,
      // StatusCallback     status   = NULL
       ) {
    connect_callback     = connect;
    keyspace_callback    = keyspace;
    error_callback       = error;

    // schema_callback   = schema;
    // topology_callback = topology;
    // status_callback   = status;
    event_received();
  }

 private:
  ClientConnection(const ClientConnection&) {}
  void operator=(const ClientConnection&) {}
};
}
#endif
