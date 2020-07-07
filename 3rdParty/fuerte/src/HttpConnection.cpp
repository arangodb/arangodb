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
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_status(http_parser* parser, const char* at,
                                  size_t len) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  // required for some arango shenanigans
  self->_response->header.addMeta(std::string("http/") +
                                      std::to_string(parser->http_major) + '.' +
                                      std::to_string(parser->http_minor),
                                  std::string(at, len));
  return 0;
}

template <SocketType ST>
int HttpConnection<ST>::on_header_field(http_parser* parser, const char* at,
                                        size_t len) {
  HttpConnection<ST>* self = static_cast<HttpConnection<ST>*>(parser->data);
  if (self->_lastHeaderWasValue) {
    toLowerInPlace(self->_lastHeaderField);  // in-place
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
    toLowerInPlace(self->_lastHeaderField);  // in-place
    self->_response->header.addMeta(std::move(self->_lastHeaderField),
                                    std::move(self->_lastHeaderValue));
  }
  // Adjust idle timeout if necessary
  self->_shouldKeepAlive = http_should_keep_alive(parser);

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
      _active(false),
      _reading(false),
      _writing(false),
      _leased(0),
      _lastHeaderWasValue(false),
      _shouldKeepAlive(false),
      _messageComplete(false),
      _timeoutOnReadWrite(false) {
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

  // preemptively cache
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
HttpConnection<ST>::~HttpConnection() try {
  this->shutdownConnection(Error::Canceled);
  drainQueue(Error::Canceled);
} catch(...) {}

// Start an asynchronous request.
template <SocketType ST>
MessageID HttpConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                          RequestCallback cb) {
  static std::atomic<uint64_t> ticketId(1);

  // construct RequestItem
  auto item = std::make_unique<RequestItem>();
  // requestItem->_response later
  uint64_t mid = ticketId.fetch_add(1, std::memory_order_relaxed);
  item->requestHeader = buildRequestBody(*req);
  item->callback = std::move(cb);
  item->request = std::move(req);

  // Don't send once in Failed state:
  if (this->_state.load(std::memory_order_relaxed) == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "connection already failed\n";
    item->invokeOnError(Error::Canceled);
    return 0;
  }

  // Prepare a new request
  if (!_queue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    item->invokeOnError(Error::QueueCapacityExceeded);
    return 0;
  }
  item.release();  // queue owns this now

  this->_numQueued.fetch_add(1, std::memory_order_relaxed);
  FUERTE_LOG_HTTPTRACE << "queued item: this=" << this << "\n";

  // If the connection was leased explicitly, we set the _leased indicator
  // to 2 here, such that it can be reset to 0 once the idle alarm is
  // enabled explicitly later. We do not have to check if this has worked
  // for the following reason: The value is only ever set to 0, 1 or 2.
  // If it is already 2, no action is needed. If it is still 0, then the
  // connection was not leased and we do not have to mark it here to indicate
  // that a `sendRequest` has happened.
  int exp = 1;
  _leased.compare_exchange_strong(exp, 2);

  // Note that we have first posted on the queue with std::memory_order_seq_cst
  // and now we check _active std::memory_order_seq_cst. This prevents a sleeping
  // barber with the check-set-check combination in `asyncWriteNextRequest`.
  // If we are the ones to exchange the value to `true`, then we post
  // on the `_io_context` to activate the connection. Note that the
  // connection can be in the `Disconnected` or `Connected` or `Failed`
  // state, but not in the `Connecting` state in this case.
  if (!this->_active.exchange(true)) {
    this->_io_context->post([self(Connection::shared_from_this())] {
        auto& me = static_cast<HttpConnection<ST>&>(*self);
        me.activate();
      });
  }

  return mid;
}

template <SocketType ST>
void HttpConnection<ST>::activate() {
  Connection::State state = this->_state.load();
  if (state == Connection::State::Connected) {
    FUERTE_LOG_HTTPTRACE << "activate: connected\n";
    this->asyncWriteNextRequest();
  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_HTTPTRACE << "activate: not connected\n";
    this->startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "activate: queued request on failed connection\n";
    drainQueue(fuerte::Error::ConnectionClosed);
    _active.store(false);    // No more activity from our side
  }
  // If the state is `Connecting`, we do not need to do anything, this can
  // happen if this `activate` was posted when the connection was still
  // `Disconnected`, but in the meantime the connect has started.
}

template <SocketType ST>
size_t HttpConnection<ST>::requestsLeft() const {
  size_t q = this->_numQueued.load(std::memory_order_relaxed);
  if (this->_active.load(std::memory_order_relaxed)) {
    q++;
  }
  return q;
}

template <SocketType ST>
void HttpConnection<ST>::finishConnect() {
  auto exp = Connection::State::Connecting;
  if (this->_state.compare_exchange_strong(exp, Connection::State::Connected)) {
    this->asyncWriteNextRequest();  // starts writing queue if non-empty
  } else {
    FUERTE_LOG_DEBUG << "Fuerte: finishConnect found state other than Connecting, ignoring for now";
  }
}

// Thread-Safe: activate the combined write-read loop
template <SocketType ST>
void HttpConnection<ST>::startWriting() {
  // This is no longer used in this version
  std::abort();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

template <SocketType ST>
std::string HttpConnection<ST>::buildRequestBody(Request const& req) {
  // build the request header
  assert(req.header.restVerb != RestVerb::Illegal);

  std::string header;
  header.reserve(256);  // TODO is there a meaningful size ?
  header.append(fu::to_string(req.header.restVerb));
  header.push_back(' ');

  // construct request path ("/_db/<name>/" prefix)
  if (!req.header.database.empty()) {
    header.append("/_db/", 5);
    http::urlEncode(header, req.header.database);
  }
  // must start with /, also turns /_db/abc into /_db/abc/
  if (req.header.path.empty() || req.header.path[0] != '/') {
    header.push_back('/');
  }

  header.append(req.header.path);
  
  if (!req.header.parameters.empty()) {
    header.push_back('?');
    for (auto const& p : req.header.parameters) {
      if (header.back() != '?') {
        header.push_back('&');
      }
      http::urlEncode(header, p.first);
      header.push_back('=');
      http::urlEncode(header, p.second);
    }
  }
  header.append(" HTTP/1.1\r\n")
      .append("Host: ")
      .append(this->_config._host)
      .append("\r\n");
  if (this->_config._idleTimeout.count() > 0) {  // technically not required for http 1.1
    header.append("Connection: Keep-Alive\r\n");
  } else {
    header.append("Connection: Close\r\n");
  }

  if (req.header.restVerb != RestVerb::Get &&
      req.contentType() != ContentType::Custom) {
    header.append("Content-Type: ")
          .append(to_string(req.contentType()))
          .append("\r\n");
  }
  if (req.acceptType() != ContentType::Custom) {
    header.append("Accept: ")
          .append(to_string(req.acceptType()))
          .append("\r\n");
  }

  bool haveAuth = false;
  for (auto const& pair : req.header.meta()) {
    if (pair.first == fu_content_length_key) {
      continue;  // skip content-length header
    }

    if (pair.first == fu_authorization_key) {
      haveAuth = true;
    }

    header.append(pair.first);
    header.append(": ");
    header.append(pair.second);
    header.append("\r\n");
  }

  if (!haveAuth && !_authHeader.empty()) {
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
  assert(_active.load());
  assert(_item == nullptr);

  http::RequestItem* ptr = nullptr;
  if (!_queue.pop(ptr)) {
    _active.store(false);
    if (_queue.empty()) {
      FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: stopped writing, this="
                           << this << "\n";
      if (_shouldKeepAlive && this->_config._idleTimeout.count() > 0) {
        FUERTE_LOG_HTTPTRACE << "setting idle keep alive timer, this=" << this
                             << "\n";
        // We want to enable the idle timeout alarm here. However, if the
        // connection object has been leased, but no `sendRequest` has been
        // called on it yet, then the alarm must not go off. Therefore,
        // if _leased is 2 (`sendRequest` has been called), we reset it to
        // 0, if _leased is still 1 (`lease` has happened, but `sendRequest`
        // has not yet been called), then we leave it untouched. Therefore,
        // no need to check the result of the compare_exchange_strong.
        int exp = 2;
        _leased.compare_exchange_strong(exp, 0);
        setTimeout(this->_config._idleTimeout, TimeoutType::IDLE);
      } else {
        this->shutdownConnection(Error::CloseRequested);
      }
      return;
    }
    if (_active.exchange(true)) {
      return;   // somebody else did this for us
    };
    _queue.pop(ptr);
  }
  this->_numQueued.fetch_sub(1, std::memory_order_relaxed);

  assert(ptr != nullptr);
  std::unique_ptr<http::RequestItem> item(ptr);
  setTimeout(item->request->timeout(), TimeoutType::WRITE);
  _timeoutOnReadWrite = false;
  _writing = true;

  std::array<asio_ns::const_buffer, 2> buffers;
  buffers[0] =
      asio_ns::buffer(item->requestHeader.data(), item->requestHeader.size());
  // GET and HEAD have no payload
  if (item->request->header.restVerb != RestVerb::Get &&
      item->request->header.restVerb != RestVerb::Head) {
    buffers[1] = item->request->payload();
  }

  asio_ns::async_write(this->_proto.socket, std::move(buffers),
                       [self(Connection::shared_from_this()),
                        req(std::move(item))](asio_ns::error_code const& ec,
                                              std::size_t nwrite) mutable {
    auto& thisPtr = static_cast<HttpConnection<ST>&>(*self);
    thisPtr.asyncWriteCallback(ec, std::move(req), nwrite);
  });
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: done, this=" << this << "\n";
}

// called by the async_write handler (called from IO thread)
template <SocketType ST>
void HttpConnection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                            std::unique_ptr<RequestItem> item,
                                            size_t nwrite) {
  this->_writing = false;       // indicate that no async write is ongoing any more
  this->_timeout.cancel();  // cancel alarm for timeout
  if (ec) {
    // Send failed
    FUERTE_LOG_DEBUG << "asyncWriteCallback (http): error '" << ec.message()
                     << "', this=" << this << "\n";
    
    // keepalive timeout may have expired
    auto err = translateError(ec, Error::WriteError);
    if (ec == asio_ns::error::broken_pipe && nwrite == 0) { // re-queue
      sendRequest(std::move(item->request), item->callback);
    } else {
      // let user know that this request caused the error
      item->callback(err, std::move(item->request), nullptr);
    }

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

  http_parser_init(&_parser, HTTP_RESPONSE);  // reset parser

  setTimeout(_item->request->timeout(), TimeoutType::READ);      // extend timeout
  _timeoutOnReadWrite = false;
  _reading = true;
  this->asyncReadSome();  // listen for the response
}

// ------------------------------------
// Reading data
// ------------------------------------

// called by the async_read handler (called from IO thread)
template <SocketType ST>
void HttpConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  this->_reading = false;
  // Do not cancel timeout now, because we might be going on to read!
  if (ec) {
    FUERTE_LOG_DEBUG << "asyncReadCallback: Error while reading from socket: '"
    << ec.message() << "' , this=" << this << "\n";

    // Restart connection, will invoke _item cb
    this->restartConnection(translateError(ec, Error::ReadError));
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
                       << "', this=" << this << "\n";
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
      
      try {
        _item->callback(Error::NoError, std::move(_item->request),
                        std::move(_response));
      } catch(...) {
        FUERTE_LOG_ERROR << "unhandled exception in fuerte callback\n";
      }

      _item.reset();
      FUERTE_LOG_HTTPTRACE << "asyncReadCallback: completed parsing "
                              "response this=" << this << "\n";

      asyncWriteNextRequest();  // send next request
      return;
    }
  }

  // Remove consumed data from receive buffer.
  this->_receiveBuffer.consume(parsedBytes);
  
  _reading = true;
  FUERTE_LOG_HTTPTRACE << "asyncReadCallback: response not complete yet\n";
  this->asyncReadSome();  // keep reading from socket
  // leave read timeout in place!
}

/// Set timeout accordingly
template <SocketType ST>
void HttpConnection<ST>::setTimeout(std::chrono::milliseconds millis, TimeoutType type) {
  if (millis.count() == 0) {
    FUERTE_LOG_TRACE << "deleting timeout of type " << (int) type << " this=" << this << "\n";
    this->_timeout.cancel();
    return;
  }

  FUERTE_LOG_TRACE << "setting timeout of type " << (int) type << " this=" << this << "\n";

  // expires_after cancels pending ops
  this->_timeout.expires_after(millis);
  this->_timeout.async_wait([self = Connection::weak_from_this(), type](auto const& ec) {
    std::shared_ptr<Connection> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    auto* thisPtr = static_cast<HttpConnection<ST>*>(s.get());
    if ((type == TimeoutType::WRITE && thisPtr->_writing) ||
        (type == TimeoutType::READ && thisPtr->_reading)) {
      FUERTE_LOG_DEBUG << "HTTP-Request timeout" << " this=" << thisPtr << "\n";
      thisPtr->_proto.shutdown();
      thisPtr->_timeoutOnReadWrite = true;
      // We simply cancel all ongoing asynchronous operations, the completion
      // handlers will do the rest.
      return;
    } else if (type == TimeoutType::IDLE && !thisPtr->_writing & !thisPtr->_reading && !thisPtr->_connecting && thisPtr->_leased == 0) {
      if (!thisPtr->_active &&
          thisPtr->_state == Connection::State::Connected) {
        int exp = 0;
        if (thisPtr->_leased.compare_exchange_strong(exp, -1)) {
          // Note that we check _leased here with std::memory_order_seq_cst
          // **before** we call `shutdownConnection` and actually kill the
          // connection. This is important, see below!
          FUERTE_LOG_DEBUG << "HTTP-Request idle timeout"
                           << " this=" << thisPtr << "\n";
          thisPtr->shutdownConnection(Error::CloseRequested);
          thisPtr->_leased = 0;
        }
      }
    }
    // In all other cases we do nothing, since we have been posted to the
    // iocontext but the thing we should be timing out has already completed.
  });
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
}

/// abort all requests lingering in the queue
template <SocketType ST>
void HttpConnection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_queue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    guard->invokeOnError(ec);
  }
}

// Lease this connection (cancel idle alarm)
template <SocketType ST>
bool HttpConnection<ST>::lease() {
  int exp = 0;
  // We have to guard against data races here. Some other thread might call
  // cancel() on the connection, but then it is OK that the next request
  // runs into an error. Somebody was asking for trouble and gets it.
  // The iothread itself can only set the state to Failed if a real
  // error occurs (in which case it is again OK to run into an error with
  // the next request), or if the idle timeout alarm goes off. This code
  // guards against the idle alarm going off despite the fact that the
  // connection was already leased, since we first compare exchange from
  // 0 to 1 and then check again that the connection is not Failed. Both
  // accesses happen with std::memory_order_seq_cst and the check for
  // _leased in the idle alarm handler happens before shutdownConnection
  // is called.
  if (!this->_leased.compare_exchange_strong(exp, 1) ||
      this->_state == Connection::State::Failed) {
    return false;
  }
  FUERTE_LOG_TRACE << "Connection leased: this=" << this;
  return true;   // this is a noop, derived classes can override
}

template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::http::HttpConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::http
