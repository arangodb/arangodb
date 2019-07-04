////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "HttpConnection.h"

#include "Basics/cpu-relax.h"

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <cassert>

#include <fuerte/FuerteLogger.h>
#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <velocypack/Parser.h>

namespace {
using namespace arangodb::fuerte::v1;
using namespace arangodb::fuerte::v1::http;

int on_message_began(http_parser* parser) { return 0; }
int on_status(http_parser* parser, const char* at, size_t len) {
  RequestItem* data = static_cast<RequestItem*>(parser->data);
  data->_response->header.meta.emplace(std::string("http/") +
                                       std::to_string(parser->http_major) + '.' +
                                       std::to_string(parser->http_minor),
                                       std::string(at, len));
  return 0; 
}
int on_header_field(http_parser* parser, const char* at, size_t len) {
  RequestItem* data = static_cast<RequestItem*>(parser->data);
  if (data->last_header_was_a_value) {
    boost::algorithm::to_lower(data->lastHeaderField); // in-place
    data->_response->header.meta.emplace(std::move(data->lastHeaderField),
                                         std::move(data->lastHeaderValue));
    data->lastHeaderField.assign(at, len);
  } else {
    data->lastHeaderField.append(at, len);
  }
  data->last_header_was_a_value = false;
  return 0;
}
static int on_header_value(http_parser* parser, const char* at, size_t len) {
  RequestItem* data = static_cast<RequestItem*>(parser->data);
  if (data->last_header_was_a_value) {
    data->lastHeaderValue.append(at, len);
  } else {
    data->lastHeaderValue.assign(at, len);
  }
  data->last_header_was_a_value = true;
  return 0;
}
static int on_header_complete(http_parser* parser) {
  RequestItem* data = static_cast<RequestItem*>(parser->data);
  data->_response->header.responseCode =
      static_cast<StatusCode>(parser->status_code);
  if (!data->lastHeaderField.empty()) {
    boost::algorithm::to_lower(data->lastHeaderField); // in-place
    data->_response->header.meta.emplace(std::move(data->lastHeaderField),
                                         std::move(data->lastHeaderValue));
  }
  data->should_keep_alive = http_should_keep_alive(parser);
  // head has no body, but may have a Content-Length
  if (data->_request->header.restVerb == RestVerb::Head) {
    data->message_complete = true;
    return 1; // tells the parser it should not expect a body
  } else if (parser->content_length > 0 && parser->content_length < ULLONG_MAX) {
    uint64_t maxReserve = std::min<uint64_t>(2 << 24, parser->content_length);
    data->_responseBuffer.reserve(maxReserve);
  }
  
  return 0;
}
static int on_body(http_parser* parser, const char* at, size_t len) {
  static_cast<RequestItem*>(parser->data)->_responseBuffer.append(at, len);
  return 0;
}
static int on_message_complete(http_parser* parser) {
  static_cast<RequestItem*>(parser->data)->message_complete = true;
  return 0;
}
}  // namespace

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

namespace fu = ::arangodb::fuerte::v1;
using namespace arangodb::fuerte::detail;

template<SocketType ST>
HttpConnection<ST>::HttpConnection(EventLoopService& loop,
                                   ConnectionConfiguration const& config)
    : Connection(config),
  _io_context(loop.nextIOContext()),
  _protocol(loop, *_io_context),
  _timeout(*_io_context),
  _state(Connection::State::Disconnected),
  _numQueued(0),
  _active(false),
  _queue() {
  // initialize http parsing code
  http_parser_settings_init(&_parserSettings);
  _parserSettings.on_message_begin = ::on_message_began;
  _parserSettings.on_status = ::on_status;
  _parserSettings.on_header_field = ::on_header_field;
  _parserSettings.on_header_value = ::on_header_value;
  _parserSettings.on_headers_complete = ::on_header_complete;
  _parserSettings.on_body = ::on_body;
  _parserSettings.on_message_complete = ::on_message_complete;
  http_parser_init(&_parser, HTTP_RESPONSE);
      
  if (_config._authenticationType == AuthenticationType::Basic) {
    _authHeader.append("Authorization: Basic ");
    _authHeader.append(fu::encodeBase64(_config._user + ":" +
                                        _config._password));
    _authHeader.append("\r\n");
  } else if (_config._authenticationType == AuthenticationType::Jwt) {
    if (_config._jwtToken.empty()) {
      throw std::logic_error("JWT token is not set");
    }
    _authHeader.append("Authorization: bearer ");
    _authHeader.append(_config._jwtToken);
    _authHeader.append("\r\n");
  }
}
  
template<SocketType ST>
HttpConnection<ST>::~HttpConnection() {
  shutdownConnection(Error::Canceled);
}
  
// Start an asynchronous request.
template<SocketType ST>
MessageID HttpConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                          RequestCallback cb) {
  static std::atomic<uint64_t> ticketId(1);
  
  // construct RequestItem
  std::unique_ptr<RequestItem> item(new RequestItem());
  // requestItem->_response later
  item->_messageID = ticketId.fetch_add(1, std::memory_order_relaxed);
  item->_requestHeader = buildRequestBody(*req);
  item->_request = std::move(req);
  item->_callback = std::move(cb);
  
  const size_t payloadSize = item->_request->payloadSize();
  // Prepare a new request
  uint64_t id = item->_messageID;
  if (!_queue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    throw std::length_error("connection queue capacity exceeded");
  }
  item.release(); // queue owns this now
  
  _numQueued.fetch_add(1, std::memory_order_release);
  _bytesToSend.fetch_add(payloadSize, std::memory_order_release);
  
  FUERTE_LOG_HTTPTRACE << "queued item: this=" << this << "\n";
  
  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = _state.load();
  if (state == Connection::State::Connected) {
    startWriting();
  } else if (state == State::Disconnected) {
    FUERTE_LOG_HTTPTRACE << "sendRequest: not connected\n";
    startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
  }
  return id;
}
  
/// @brief cancel the connection, unusable afterwards
template <SocketType ST>
void HttpConnection<ST>::cancel() {
  FUERTE_LOG_CALLBACKS << "cancel: this=" << this << "\n";
  std::weak_ptr<Connection> self = shared_from_this();
  asio_ns::post(*_io_context, [self, this] {
    auto s = self.lock();
    if (s) {
      shutdownConnection(Error::Canceled);
      _state.store(State::Failed);
    }
  });
}
  
// Activate this connection.
template <SocketType ST>
void HttpConnection<ST>::startConnection() {
  // start connecting only if state is disconnected
  Connection::State exp = Connection::State::Disconnected;
  if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    tryConnect(_config._maxConnectRetries);
  }
}
  
// Connect with a given number of retries
template <SocketType ST>
void HttpConnection<ST>::tryConnect(unsigned retries) {
  assert(_state.load() == Connection::State::Connecting);
  
  auto self = shared_from_this();
  _protocol.connect(_config, [self, this, retries](asio_ns::error_code const& ec) {
    if (!ec) {
      _state.store(Connection::State::Connected);
      startWriting();  // starts writing queue if non-empty
      return;
    }
    FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
    if (retries > 0 && ec != asio_ns::error::operation_aborted) {
      tryConnect(retries - 1);
    } else {
      shutdownConnection(Error::CouldNotConnect);
      onFailure(Error::CouldNotConnect,
                "connecting failed: " + ec.message());
    }
  });
}

// shutdown the connection and cancel all pending messages.
template<SocketType ST>
void HttpConnection<ST>::shutdownConnection(const Error ec) {
  FUERTE_LOG_CALLBACKS << "shutdownConnection: this=" << this << "\n";

  if (_state.load() != State::Failed) {
    _state.store(State::Disconnected);
  }
  
  // cancel() may throw, but we are not allowed to throw here
  try {
    _timeout.cancel();
  } catch (...) {}
  try {
    _protocol.shutdown(); // Close socket
  } catch(...) {}
  
  _active.store(false); // no IO operations running
  
  RequestItem* item = nullptr;
  while (_queue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    _numQueued.fetch_sub(1, std::memory_order_release);
    _bytesToSend.fetch_sub(item->_request->payloadSize(), std::memory_order_release);
    guard->invokeOnError(ec);
  }
  
  // simon: thread-safe, only called from IO-Thread
  // (which holds shared_ptr) and destructors
  if (_inFlight) {
    // Item has failed, remove from message store
    _inFlight->invokeOnError(ec);
    _inFlight.reset();
  }
  
  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
}
  
// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
  
template<SocketType ST>
void HttpConnection<ST>::restartConnection(const Error error) {
  // restarting needs to be an exclusive operation
  Connection::State exp = Connection::State::Connected;
  if (_state.compare_exchange_strong(exp, Connection::State::Disconnected)) {
    FUERTE_LOG_CALLBACKS << "restartConnection\n";
    shutdownConnection(error); // Terminate connection
    startConnection(); // will check state
  }
}

template<SocketType ST>
std::string HttpConnection<ST>::buildRequestBody(Request const& req) {
  // build the request header
  assert(req.header.restVerb != RestVerb::Illegal);

  std::string header;
  header.reserve(230);  // TODO is there a meaningful size ?
  header.append(fu::to_string(req.header.restVerb));
  header.push_back(' ');

  // construct request path ("/_db/<name>/" prefix)
  if (!req.header.database.empty()) {
    header.append("/_db/");
    header.append(http::urlEncode(req.header.database));
  }
  // must start with /, also turns /_db/abc into /_db/abc/
  if (req.header.path.empty() || req.header.path[0] != '/') {
    header.push_back('/');
  }

  if (req.header.parameters.empty()) {
    header.append(req.header.path);
  } else {
    header.append(req.header.path);
    header.push_back('?');
    for (auto p : req.header.parameters) {
      if (header.back() != '?') {
        header.push_back('&');
      }
      header.append(http::urlEncode(p.first) + "=" + http::urlEncode(p.second));
    }
  }
  header.append(" HTTP/1.1\r\n");
  header.append("Host: ");
  header.append(_config._host);
  header.append("\r\n");
  // TODO add option to configuration
  if (_config._connectionTimeout.count() > 0) {
    header.append("Connection: Keep-Alive\r\n");
//    header.append("Keep-Alive: timeout=60\r\n");
  } else {
    header.append("Connection: Close\r\n");
  }
  for (auto const& pair : req.header.meta) {
    if (boost::iequals(fu_content_length_key, pair.first)) {
      continue; // skip content-length header
    }
    
    header.append(pair.first);
    header.append(": ");
    header.append(pair.second);
    header.append("\r\n");
  }

  if (!_authHeader.empty()) {
    header.append(_authHeader);
  }

  if (req.header.restVerb != RestVerb::Get &&
      req.header.restVerb != RestVerb::Head) {
    header.append("Content-Length: ");
    header.append(std::to_string(req.payloadSize()));
    header.append("\r\n\r\n");
  } else {
    header.append("\r\n");
  }
  // body will be appended seperately
  return header;
}

// Thread-Safe: activate the combined write-read loop
template<SocketType ST>
void HttpConnection<ST>::startWriting() {
  FUERTE_LOG_HTTPTRACE << "startWriting: this=" << this << "\n";
  
  if (!_active) {
    auto self = shared_from_this();
    asio_ns::post(*_io_context, [this, self] {
      if (!_active.exchange(true)) {
        FUERTE_LOG_HTTPTRACE << "startWriting: active=true, this=" << this << "\n";
        asyncWriteNextRequest();
      }
    });
  }
}
  
// writes data from task queue to network using asio_ns::async_write
template<SocketType ST>
void HttpConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: this=" << this << "\n";
  assert(_active.load(std::memory_order_acquire));
  
  http::RequestItem* ptr = nullptr;
  if (!_queue.pop(ptr)) {
    _active.store(false);
    if (!_queue.pop(ptr)) {
      FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: stopped writing, this=" << this << "\n";
      return;
    }
    // a request got queued in-between last minute
    _active.store(true);
  }
  _numQueued.fetch_sub(1, std::memory_order_release);
  
  std::unique_ptr<http::RequestItem> item(ptr);
  setTimeout(item->_request->timeout());
  std::vector<asio_ns::const_buffer> buffers;
  buffers.reserve(2);
  buffers.emplace_back(item->_requestHeader.data(),
                       item->_requestHeader.size());
  // GET and HEAD have no payload
  if (item->_request->header.restVerb != RestVerb::Get &&
      item->_request->header.restVerb != RestVerb::Head) {
    buffers.emplace_back(item->_request->payload());
  }
  
  auto self = shared_from_this();
  asio_ns::async_write(_protocol.socket, std::move(buffers),
                       [this, self, ri = std::move(item)](asio_ns::error_code const& ec,
                                                          std::size_t transferred) mutable {
    _bytesToSend.fetch_sub(ri->_request->payloadSize(), std::memory_order_release);
    asyncWriteCallback(ec, transferred, std::move(ri));
  });
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: done, this=" << this << "\n";
}

// called by the async_write handler (called from IO thread)
template<SocketType ST>
void HttpConnection<ST>::asyncWriteCallback(
    asio_ns::error_code const& ec, size_t transferred,
    std::unique_ptr<RequestItem> item) {
  if (ec) {
    // Send failed
    FUERTE_LOG_CALLBACKS << "asyncWriteCallback (http): error "
                         << ec.message() << "\n";
    assert(item->_callback);
    auto err = checkEOFError(ec, Error::WriteError);
    // let user know that this request caused the error
    item->_callback(err, std::move(item->_request), nullptr);
    // Stop current connection and try to restart a new one.
    restartConnection(err);
    return;
  }
  
  // Send succeeded
  FUERTE_LOG_CALLBACKS << "asyncWriteCallback (http): send succeeded, "
                       << transferred << " bytes transferred\n";

  // request is written we no longer need data for that
  item->_requestHeader.clear();

  // thead-safe we are on the single IO-Thread
  assert(_inFlight == nullptr);
  _inFlight = std::move(item);
  assert(_inFlight->_response == nullptr);
  _inFlight->_response.reset(new Response());

  http_parser_init(&_parser, HTTP_RESPONSE);
  _parser.data = static_cast<void*>(_inFlight.get());

  // check queue length later
  asyncReadSome();  // listen for the response

  FUERTE_LOG_HTTPTRACE << "asyncWriteCallback: waiting for response\n";
}
  
// ------------------------------------
// Reading data
// ------------------------------------

// asyncReadSome reads the next bytes from the server.
template<SocketType ST>
void HttpConnection<ST>::asyncReadSome() {
  FUERTE_LOG_HTTPTRACE << "asyncReadSome: this=" << this << "\n";
  
  auto self = shared_from_this();
  auto cb = [this, self](asio_ns::error_code const& ec, size_t transferred) {
    // received data is "committed" from output sequence to input sequence
    _receiveBuffer.commit(transferred);
    asyncReadCallback(ec, transferred);
  };
  
  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  _protocol.socket.async_read_some(mutableBuff, std::move(cb));
  
  FUERTE_LOG_HTTPTRACE << "asyncReadSome: done\n";
}

// called by the async_read handler (called from IO thread)
template<SocketType ST>
void HttpConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec,
                                           size_t transferred) {
  
  if (ec) {
    FUERTE_LOG_CALLBACKS
        << "asyncReadCallback: Error while reading from socket";
    FUERTE_LOG_ERROR << ec.message() << "\n";
    // Restart connection, will invoke _inFlight cb
    restartConnection(checkEOFError(ec, Error::ReadError));
    return;
  }
  FUERTE_LOG_CALLBACKS
      << "asyncReadCallback: received " << transferred << " bytes\n";

  if (!_inFlight) { // should not happen
    assert(false);
    shutdownConnection(Error::Canceled);
  }
    
  // Inspect the data we've received so far.
  size_t parsedBytes = 0;
  auto buffers = _receiveBuffer.data(); // no copy
  for (auto const& buffer : buffers) {
    
    /* Start up / continue the parser.
     * Note we pass recved==0 to signal that EOF has been received.
     */
    size_t nparsed = http_parser_execute(&_parser, &_parserSettings,
                                         static_cast<const char *>(buffer.data()),
                                         buffer.size());
    parsedBytes += nparsed;
    
    if (_parser.upgrade) {
      /* handle new protocol */
      FUERTE_LOG_ERROR << "Upgrading is not supported\n";
      shutdownConnection(Error::ProtocolError);  // will cleanup _inFlight
      return;
    } else if (nparsed != buffer.size()) {
      /* Handle error. Usually just close the connection. */
      FUERTE_LOG_ERROR << "Invalid HTTP response in parser: '"
      << http_errno_description(HTTP_PARSER_ERRNO(&_parser)) << "'\n";
      shutdownConnection(Error::ProtocolError);  // will cleanup _inFlight
      return;
    } else if (_inFlight->message_complete) {
      _timeout.cancel(); // got response in time
      // Remove consumed data from receive buffer.
      _receiveBuffer.consume(parsedBytes);
      
      // thread-safe access on IO-Thread
      if (!_inFlight->_responseBuffer.empty()) {
        _inFlight->_response->setPayload(std::move(_inFlight->_responseBuffer), 0);
      }
      _inFlight->_callback(Error::NoError,
                           std::move(_inFlight->_request),
                           std::move(_inFlight->_response));
      if (!_inFlight->should_keep_alive) {
        shutdownConnection(Error::CloseRequested);
        return;
      }
      _inFlight.reset();
      
      FUERTE_LOG_HTTPTRACE
      << "asyncReadCallback: completed parsing response\n";

      asyncWriteNextRequest();  // send next request
      return;
    }
  }
  
  // Remove consumed data from receive buffer.
  _receiveBuffer.consume(parsedBytes);
  
  FUERTE_LOG_HTTPTRACE
      << "asyncReadCallback: response not complete yet\n";
  asyncReadSome();  // keep reading from socket
}
  
/// Set timeout accordingly
template<SocketType ST>
void HttpConnection<ST>::setTimeout(std::chrono::milliseconds millis) {
  if (millis.count() == 0) {
    _timeout.cancel();
    return;
  }
  assert(millis.count() > 0);
  _timeout.expires_after(millis);
  
  std::weak_ptr<Connection> self = shared_from_this();
  _timeout.async_wait([self, this] (asio_ns::error_code const& ec) {
    if (!ec) {
      auto s = self.lock();
      if (s) {
        FUERTE_LOG_DEBUG << "HTTP-Request timeout\n";
        restartConnection(Error::Timeout);
      }
    }
  });
}
  
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::http
