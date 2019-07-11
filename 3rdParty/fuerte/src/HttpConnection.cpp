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

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

namespace fu = ::arangodb::fuerte::v1;
using namespace arangodb::fuerte::detail;
using namespace arangodb::fuerte::v1;
using namespace arangodb::fuerte::v1::http;

template <SocketType ST>
int HttpConnection<ST>::on_message_begin(http_parser* parser) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  self->_lastHeaderField.clear();
  self->_lastHeaderValue.clear();
  self->_lastHeaderWasValue = false;
  self->_shouldKeepAlive = false;
  self->_messageComplete = false;
  self->_response.reset(new Response());
  self->_idleTimeout = self->_config._idleTimeout;
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_status(http_parser* parser, const char* at,
                                  size_t len) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  self->_response->header.meta.emplace(
      std::string("http/") + std::to_string(parser->http_major) + '.' +
          std::to_string(parser->http_minor),
      std::string(at, len));
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_header_field(http_parser* parser, const char* at,
                                        size_t len) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  if (self->_lastHeaderWasValue) {
    boost::algorithm::to_lower(self->_lastHeaderField);  // in-place
    self->_response->header.addMeta(std::move(self->_lastHeaderField),
                                    std::move(self->_lastHeaderValue));
    self->_lastHeaderField.assign(at, len);
  } else {
    self->_lastHeaderField.append(at, len);
  }
  self->_lastHeaderWasValue = false;
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_header_value(http_parser* parser, const char* at,
                                        size_t len) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  if (self->_lastHeaderWasValue) {
    self->_lastHeaderValue.append(at, len);
  } else {
    self->_lastHeaderValue.assign(at, len);
  }
  self->_lastHeaderWasValue = true;
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_header_complete(http_parser* parser) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  self->_response->header.responseCode =
      static_cast<StatusCode>(parser->status_code);
  if (!self->_lastHeaderField.empty()) {
    boost::algorithm::to_lower(self->_lastHeaderField);  // in-place
    self->_response->header.addMeta(std::move(self->_lastHeaderField),
                                    std::move(self->_lastHeaderValue));
  }
  // Adjust idle timeout if necessary
  self->_shouldKeepAlive = http_should_keep_alive(parser);
  if (self->_shouldKeepAlive) {  // check for exact idle timeout
    std::string const& ka =
        self->_response->header.metaByKey(fu_keep_alive_key);
    size_t pos = ka.find("timeout=");
    if (pos != std::string::npos) {
      try {
        std::chrono::milliseconds to(std::stoi(ka.substr(pos + 8)) * 1000);
        if (to.count() > 1000) {
          self->_idleTimeout = std::min(self->_config._idleTimeout, to);
        }
      } catch (...) {
      }
    }
  }
  // head has no body, but may have a Content-Length
  if (self->_item->request->header.restVerb == RestVerb::Head) {
    return 1;  // tells the parser it should not expect a body
  } else if (parser->content_length > 0 &&
             parser->content_length < ULLONG_MAX) {
    uint64_t maxReserve = std::min<uint64_t>(2 << 24, parser->content_length);
    self->_responseBuffer.reserve(maxReserve);
  }

  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_body(http_parser* parser, const char* at,
                                size_t len) {
  static_cast<HttpConnection<ST>*>(parser->data)
      ->_responseBuffer.append(at, len);
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_message_complete(http_parser* parser) {
  static_cast<HttpConnection<ST>*>(parser->data)->_messageComplete = true;
  return 0;
}

template <SocketType ST>
HttpConnection<ST>::HttpConnection(EventLoopService& loop,
                                   ConnectionConfiguration const& config)
    : GeneralConnection<ST>(loop, config),
      _queue(),
      _numQueued(0),
      _active(false),
      _idleTimeout(this->_config._idleTimeout),
      _lastHeaderWasValue(false),
      _shouldKeepAlive(false),
      _messageComplete(false) {
  // initialize http parsing code
  http_parser_settings_init(&_parserSettings);
  _parserSettings.on_message_begin = &on_message_begin;
  _parserSettings.on_status = &on_status;
  _parserSettings.on_header_field = &on_header_field;
  _parserSettings.on_header_value = &on_header_value;
  _parserSettings.on_headers_complete = &on_header_complete;
  _parserSettings.on_body = &on_body;
  _parserSettings.on_message_complete = &on_message_complete;
  http_parser_init(&_parser, HTTP_RESPONSE);
  _parser.data = static_cast<void*>(this);

  // preemtively cache
  if (this->_config._authenticationType == AuthenticationType::Basic) {
    _authHeader.append("Authorization: Basic ");
    _authHeader.append(
        fu::encodeBase64(this->_config._user + ":" + this->_config._password));
    _authHeader.append("\r\n");
  } else if (this->_config._authenticationType == AuthenticationType::Jwt) {
    if (this->_config._jwtToken.empty()) {
      throw std::logic_error("JWT token is not set");
    }
    _authHeader.append("Authorization: bearer ");
    _authHeader.append(this->_config._jwtToken);
    _authHeader.append("\r\n");
  }

  FUERTE_LOG_TRACE << "creating http connection: this=" << this << "\n";
}

template <SocketType ST>
HttpConnection<ST>::~HttpConnection() {
  this->shutdownConnection(Error::Canceled);
  drainQueue(Error::Canceled);
}

// Start an asynchronous request.
template <SocketType ST>
MessageID HttpConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                          RequestCallback cb) {
  static std::atomic<uint64_t> ticketId(1);

  // construct RequestItem
  std::unique_ptr<RequestItem> item(new RequestItem());
  // requestItem->_response later
  uint64_t mid = ticketId.fetch_add(1, std::memory_order_relaxed);
  item->requestHeader = buildRequestBody(*req);
  item->callback = std::move(cb);
  item->request = std::move(req);

  // Prepare a new request
  if (!_queue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    throw std::length_error("connection queue capacity exceeded");
  }
  item.release();  // queue owns this now

  _numQueued.fetch_add(1, std::memory_order_release);
  FUERTE_LOG_HTTPTRACE << "queued item: this=" << this << "\n";

  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = this->_state.load();
  if (state == Connection::State::Connected) {
    startWriting();
  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_HTTPTRACE << "sendRequest: not connected\n";
    this->startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
  }
  return mid;
}

template <SocketType ST>
void HttpConnection<ST>::finishConnect() {
  this->_state.store(Connection::State::Connected);
  startWriting();  // starts writing queue if non-empty
}

// Thread-Safe: activate the combined write-read loop
template <SocketType ST>
void HttpConnection<ST>::startWriting() {
  FUERTE_LOG_HTTPTRACE << "startWriting: this=" << this << "\n";
  if (!_active) {
    FUERTE_LOG_HTTPTRACE << "startWriting: active=true, this=" << this << "\n";
    auto cb = [self = Connection::shared_from_this()] {
      auto* thisPtr = static_cast<HttpConnection<ST>*>(self.get());
      if (!thisPtr->_active.exchange(true)) {
        thisPtr->asyncWriteNextRequest();
      }
    };
    asio_ns::post(*this->_io_context, std::move(cb));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

template <SocketType ST>
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
  header.append(this->_config._host);
  header.append("\r\n");
  if (_idleTimeout.count() > 0) { // technically not required for http 1.1
    header.append("Connection: Keep-Alive\r\n");
  } else {
    header.append("Connection: Close\r\n");
  }
  for (auto const& pair : req.header.meta) {
    if (boost::iequals(fu_content_length_key, pair.first)) {
      continue;  // skip content-length header
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

// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void HttpConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: this=" << this << "\n";
  assert(_active.load(std::memory_order_acquire));

  http::RequestItem* ptr = nullptr;
  if (!_queue.pop(ptr)) {
    _active.store(false);
    if (!_queue.pop(ptr)) {
      FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: stopped writing, this="
                           << this << "\n";
      if (_shouldKeepAlive && _idleTimeout.count() > 0) {
        FUERTE_LOG_HTTPTRACE << "setting idle keep alive timer, this=" << this
                             << "\n";
        setTimeout(_idleTimeout);
      } else {
        this->shutdownConnection(Error::CloseRequested);
      }
      return;
    }
    _active.store(true);
  }
  _numQueued.fetch_sub(1, std::memory_order_release);

  std::unique_ptr<http::RequestItem> item(ptr);
  setTimeout(item->request->timeout());

  std::array<asio_ns::const_buffer, 2> buffers;
  buffers[0] =
      asio_ns::buffer(item->requestHeader.data(), item->requestHeader.size());
  // GET and HEAD have no payload
  if (item->request->header.restVerb != RestVerb::Get &&
      item->request->header.restVerb != RestVerb::Head) {
    buffers[1] = item->request->payload();
  }

  auto self = Connection::shared_from_this();
  auto cb = [self, ri = std::move(item)](asio_ns::error_code const& ec,
                                         std::size_t transferred) mutable {
    auto* thisPtr = static_cast<HttpConnection<ST>*>(self.get());
    thisPtr->asyncWriteCb(ec, std::move(ri));
  };

  asio_ns::async_write(this->_protocol.socket, std::move(buffers),
                       std::move(cb));
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: done, this=" << this << "\n";
}

// called by the async_write handler (called from IO thread)
template <SocketType ST>
void HttpConnection<ST>::asyncWriteCb(asio_ns::error_code const& ec,
                                      std::unique_ptr<RequestItem> item) {
  if (ec) {
    // Send failed
    FUERTE_LOG_DEBUG << "asyncWriteCallback (http): error " << ec.message()
                     << "\n";
    assert(item->callback);

    auto err = checkEOFError(ec, Error::WriteError);
    // let user know that this request caused the error
    item->callback(err, std::move(item->request), nullptr);
    // Stop current connection and try to restart a new one.
    this->restartConnection(err);
    return;
  }

  // Send succeeded
  FUERTE_LOG_HTTPTRACE << "asyncWriteCallback: send succeeded "
                       << "this=" << this << "\n";

  // request is written we no longer need data for that
  item->requestHeader.clear();

  // thead-safe we are on the single IO-Thread
  assert(_item == nullptr);
  _item = std::move(item);

  http_parser_init(&_parser, HTTP_RESPONSE);

  // check queue length later
  this->asyncReadSome();  // listen for the response
}

// ------------------------------------
// Reading data
// ------------------------------------

// called by the async_read handler (called from IO thread)
template <SocketType ST>
void HttpConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  if (ec) {
    FUERTE_LOG_DEBUG << "asyncReadCallback: Error while reading from socket: '";

    // Restart connection, will invoke _item cb
    this->restartConnection(checkEOFError(ec, Error::ReadError));
    return;
  }

  if (!_item) {  // should not happen
    assert(false);
    this->shutdownConnection(Error::Canceled);
    return;
  }

  // Inspect the data we've received so far.
  size_t parsedBytes = 0;
  auto buffers = this->_receiveBuffer.data();  // no copy
  for (auto const& buffer : buffers) {
    /* Start up / continue the parser.
     * Note we pass recved==0 to signal that EOF has been received.
     */
    size_t nparsed = http_parser_execute(
        &_parser, &_parserSettings, static_cast<const char*>(buffer.data()),
        buffer.size());
    parsedBytes += nparsed;

    if (_parser.upgrade) {
      /* handle new protocol */
      FUERTE_LOG_ERROR << "Upgrading is not supported\n";
      this->shutdownConnection(Error::ProtocolError);  // will cleanup _item
      return;
    } else if (nparsed != buffer.size()) {
      /* Handle error. Usually just close the connection. */
      FUERTE_LOG_ERROR << "Invalid HTTP response in parser: '"
                       << http_errno_description(HTTP_PARSER_ERRNO(&_parser))
                       << "'\n";
      this->shutdownConnection(Error::ProtocolError);  // will cleanup _item
      return;
    } else if (_messageComplete) {
      this->_timeout.cancel();  // got response in time
      // Remove consumed data from receive buffer.
      this->_receiveBuffer.consume(parsedBytes);

      // thread-safe access on IO-Thread
      if (!_responseBuffer.empty()) {
        _response->setPayload(std::move(_responseBuffer), 0);
      }
      _item->callback(Error::NoError, std::move(_item->request),
                      std::move(_response));
      _item.reset();
      FUERTE_LOG_HTTPTRACE << "asyncReadCallback: completed parsing "
                              "response this="
                           << this << "\n";

      asyncWriteNextRequest();  // send next request
      return;
    }
  }

  // Remove consumed data from receive buffer.
  this->_receiveBuffer.consume(parsedBytes);

  FUERTE_LOG_HTTPTRACE << "asyncReadCallback: response not complete yet\n";
  this->asyncReadSome();  // keep reading from socket
}

/// Set timeout accordingly
template <SocketType ST>
void HttpConnection<ST>::setTimeout(std::chrono::milliseconds millis) {
  this->_timeout.cancel();
  if (millis.count() == 0) {
    return;
  }

  this->_timeout.expires_after(millis);
  std::weak_ptr<Connection> self = Connection::shared_from_this();
  auto cb = [self](asio_ns::error_code const& ec) {
    std::shared_ptr<Connection> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    auto* thisPtr = static_cast<HttpConnection<ST>*>(s.get());

    FUERTE_LOG_DEBUG << "HTTP-Request timeout\n";
    if (thisPtr->_active) {
      thisPtr->restartConnection(Error::Timeout);
    } else { // close an idle connection
      thisPtr->shutdownConnection(Error::CloseRequested);
    }
  };

  this->_timeout.async_wait(std::move(cb));
}

/// abort ongoing / unfinished requests
template <SocketType ST>
void HttpConnection<ST>::abortOngoingRequests(const fuerte::Error ec) {
  // simon: thread-safe, only called from IO-Thread
  // (which holds shared_ptr) and destructors
  if (_item) {
    // Item has failed, remove from message store
    _item->invokeOnError(ec);
    _item.reset();
  }
  _active.store(false);  // no IO operations running
}

/// abort all requests lingering in the queue
template <SocketType ST>
void HttpConnection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_queue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    _numQueued.fetch_sub(1, std::memory_order_release);
    guard->invokeOnError(ec);
  }
}

template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::http
