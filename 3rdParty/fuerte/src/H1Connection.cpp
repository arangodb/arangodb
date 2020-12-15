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

#include "H1Connection.h"

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <velocypack/Parser.h>

#include <atomic>
#include <cassert>

#include "debugging.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

namespace fu = ::arangodb::fuerte::v1;
using namespace arangodb::fuerte::detail;
using namespace arangodb::fuerte::v1;
using namespace arangodb::fuerte::v1::http;

template <SocketType ST>
int H1Connection<ST>::on_message_begin(http_parser* p) {
  H1Connection<ST>* self = static_cast<H1Connection<ST>*>(p->data);
  self->_lastHeaderField.clear();
  self->_lastHeaderValue.clear();
  self->_lastHeaderWasValue = false;
  self->_shouldKeepAlive = false;
  self->_messageComplete = false;
  self->_response.reset(new Response());
  return 0;
}

template <SocketType ST>
int H1Connection<ST>::on_status(http_parser* parser, const char* at,
                                size_t len) {
  H1Connection<ST>* self = static_cast<H1Connection<ST>*>(parser->data);
  
  self->_response->header.addMeta(std::string("http/") +
                                      std::to_string(parser->http_major) + '.' +
                                      std::to_string(parser->http_minor),
                                  std::to_string(parser->status_code) + ' ' + std::string(at, len));
  return 0;
}

template <SocketType ST>
int H1Connection<ST>::on_header_field(http_parser* parser, const char* at,
                                      size_t len) {
  H1Connection<ST>* self = static_cast<H1Connection<ST>*>(parser->data);
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
int H1Connection<ST>::on_header_value(http_parser* parser, const char* at,
                                      size_t len) {
  H1Connection<ST>* self = static_cast<H1Connection<ST>*>(parser->data);
  if (self->_lastHeaderWasValue) {
    self->_lastHeaderValue.append(at, len);
  } else {
    self->_lastHeaderValue.assign(at, len);
  }
  self->_lastHeaderWasValue = true;
  return 0;
}

template <SocketType ST>
int H1Connection<ST>::on_header_complete(http_parser* parser) {
  H1Connection<ST>* self = static_cast<H1Connection<ST>*>(parser->data);
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
int H1Connection<ST>::on_body(http_parser* parser, const char* at, size_t len) {
  static_cast<H1Connection<ST>*>(parser->data)->_responseBuffer.append(at, len);
  return 0;
}

template <SocketType ST>
int H1Connection<ST>::on_message_complete(http_parser* parser) {
  static_cast<H1Connection<ST>*>(parser->data)->_messageComplete = true;
  return 0;
}

template <SocketType ST>
H1Connection<ST>::H1Connection(EventLoopService& loop,
                               ConnectionConfiguration const& config)
    : GeneralConnection<ST>(loop, config),
      _queue(),
      _active(false),
      _reading(false),
      _writing(false),
      _writeStart(),
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
        fu::encodeBase64(this->_config._user + ":" + this->_config._password, true));
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
H1Connection<ST>::~H1Connection() try {
  drainQueue(Error::Canceled);
  abortOngoingRequests(Error::Canceled);
} catch (...) {
}

// Start an asynchronous request.
template <SocketType ST>
void H1Connection<ST>::sendRequest(std::unique_ptr<Request> req,
                                   RequestCallback cb) {
  // construct RequestItem
  auto item = std::make_unique<RequestItem>();
  // requestItem->_response later
  item->requestHeader = buildRequestBody(*req);
  item->callback = std::move(cb);
  item->request = std::move(req);

  // Don't send once in Failed state:
  if (this->_state.load(std::memory_order_relaxed) == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "connection already failed\n";
    item->invokeOnError(Error::Canceled);
    return;
  }

  // Prepare a new request
  this->_numQueued.fetch_add(1, std::memory_order_relaxed);
  if (!_queue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);
    item->invokeOnError(Error::QueueCapacityExceeded);
    return;
  }
  item.release();  // queue owns this now

  FUERTE_LOG_HTTPTRACE << "queued item: this=" << this << "\n";

  // If the connection was leased explicitly, we set the _leased indicator
  // to 2 here, such that it can be reset to 0 once the idle alarm is
  // enabled explicitly later. We do not have to check if this has worked
  // for the following reason: The value is only ever set to -1, 0, 1 or 2.
  // If it is already 2, no action is needed. If it is still 0, then the
  // connection was not leased and we do not have to mark it here to indicate
  // that a `sendRequest` has happened. If it is currently -1, then the
  // connection will be shut down anyway. In the TLS case, this will set
  // the _state to Failed and the current operation will fail anyway.
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
        auto& me = static_cast<H1Connection<ST>&>(*self);
        me.activate();
      });
  }
}

template <SocketType ST>
size_t H1Connection<ST>::requestsLeft() const {
  size_t q = this->_numQueued.load(std::memory_order_relaxed);
  if (this->_active.load(std::memory_order_relaxed)) {
    q++;
  }
  return q;
}

template <SocketType ST>
void H1Connection<ST>::activate() {
  Connection::State state = this->_state.load();
  FUERTE_ASSERT(state != Connection::State::Connecting);
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
void H1Connection<ST>::finishConnect() {
  // Note that the connection timeout alarm has already been disarmed.
  // If it has already gone off, we might have a completion handler
  // already posted on the iocontext. However, this will not touch anything
  // if we have first set the state to `Connected`.
  FUERTE_ASSERT(_active.load());
  auto exp = Connection::State::Connecting;
  if (this->_state.compare_exchange_strong(exp, Connection::State::Connected)) {
    this->asyncWriteNextRequest();  // starts writing if queue non-empty
  } else {
    FUERTE_LOG_ERROR << "finishConnect: found state other than 'Connecting': " << static_cast<int>(exp);
    FUERTE_ASSERT(false);
    // If this happens, we probably have a sleeping barber
  }
}

// This is no longer used, we call directly `asyncWriteNextRequest` if we need
// to start writing. We will remove it once H2Connection and VstConnection
// have also lost it (if possible).
//
// startWriting must only be executed on the IO thread and only with active==true
// and with state `Connected`
template <SocketType ST>
void H1Connection<ST>::startWriting() {
  FUERTE_ASSERT(_active.load());
  FUERTE_ASSERT(this->_state.load() == Connection::State::Connected);
  FUERTE_LOG_HTTPTRACE << "startWriting: active=true, this=" << this << "\n";
  this->asyncWriteNextRequest();
}

/// The following is called when the connection is permanently failed. It is
/// used to shut down any activity in the derived classes in a way that avoids
/// sleeping barbers
template <SocketType ST>
void H1Connection<ST>::terminateActivity() {
  // Usually, we are `active == true` when we get here, except for the following
  // case: If we are inactive but the connection is still open and then the
  // idle timeout goes off, then we shutdownConnection and in the TLS case we
  // call this method here. In this case it is OK to simply proceed, therefore
  // no assertion on `_active`.
  FUERTE_ASSERT(this->_state.load() == Connection::State::Failed);
  FUERTE_LOG_HTTPTRACE << "terminateActivity: active=true, this=" << this << "\n";
  while (true) {
    drainQueue(Error::Canceled);
    _active.store(false);
    // Now need to check again:
    if (_queue.empty()) {
      return;
    }
    _active.store(true);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

template <SocketType ST>
std::string H1Connection<ST>::buildRequestBody(Request const& req) {
  // build the request header
  FUERTE_ASSERT(req.header.restVerb != RestVerb::Illegal);

  std::string header;
  header.reserve(256);  // TODO is there a meaningful size ?
  header.append(fu::to_string(req.header.restVerb));
  header.push_back(' ');

  http::appendPath(req, /*target*/header);

  header.append(" HTTP/1.1\r\n")
      .append("Host: ")
      .append(this->_config._host)
      .append("\r\n");
  if (this->_config._idleTimeout.count() >
      0) {  // technically not required for http 1.1
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
void H1Connection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: this=" << this << "\n";
  FUERTE_ASSERT(this->_active.load());
  auto state = this->_state.load();
  FUERTE_ASSERT(state == Connection::State::Connected ||
                state == Connection::State::Failed);
  if (state != Connection::State::Connected &&
      state != Connection::State::Failed) {
    FUERTE_LOG_ERROR << "asyncWriteNextRequest found an unexpected state: "
                     << static_cast<int>(state)
                     << " instead of the expected 'Connected'\n";
  }
  FUERTE_ASSERT(_item == nullptr);

  RequestItem* ptr = nullptr;
  if (!_queue.pop(ptr)) {     // check
    _active.store(false);      // set
    if (_queue.empty()) {        // check again
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
        // has not yet been called), then we leave it untouched. Since
        // this method here is only ever executed on the iothread and
        // the value -1 only happens for a brief period in the iothread,
        // it will never be observed here. Therefore, no actual need to check
        // the result of the compare_exchange_strong.
        int exp = 2;
        if (!_leased.compare_exchange_strong(exp, 0)) {
          FUERTE_ASSERT(exp != -1);
        }
        setTimeout(this->_config._idleTimeout, TimeoutType::IDLE);
      } else {
        this->shutdownConnection(Error::CloseRequested);
      }
      return;
    }
    if (_active.exchange(true)) {
      return; // someone else restarted
    }
    bool success = _queue.pop(ptr);
    FUERTE_ASSERT(success);
  }
  uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
  FUERTE_ASSERT(_item.get() == nullptr);
  FUERTE_ASSERT(ptr != nullptr);
  FUERTE_ASSERT(q > 0);
  
  _item.reset(ptr);

  setTimeout(_item->request->timeout(), TimeoutType::WRITE);
  _timeoutOnReadWrite = false;
  _writeStart = std::chrono::steady_clock::now();
  _writing = true;

  std::array<asio_ns::const_buffer, 2> buffers;
  buffers[0] =
      asio_ns::buffer(_item->requestHeader.data(), _item->requestHeader.size());
  // GET and HEAD have no payload
  if (_item->request->header.restVerb != RestVerb::Get &&
      _item->request->header.restVerb != RestVerb::Head) {
    buffers[1] = _item->request->payload();
  }

  asio_ns::async_write(
      this->_proto.socket, std::move(buffers),
      [self(Connection::shared_from_this())](
          asio_ns::error_code const& ec, std::size_t nwrite) {
        static_cast<H1Connection<ST>&>(*self).asyncWriteCallback(ec, nwrite);
      });
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: done, this=" << this << "\n";
}

// called by the async_write handler (called from IO thread)
template <SocketType ST>
void H1Connection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                          size_t nwrite) {
  FUERTE_ASSERT(this->_writing);
  // In the TLS case a connection can go to Failed state essentially at any time
  // and in this case _active will already be false if we get back here. Therefore
  // we cannot assert on it being true, which would otherwise be correct.
  FUERTE_ASSERT(this->_state != Connection::State::Connecting);
  this->_writing = false;       // indicate that no async write is ongoing any more
  this->_proto.timer.cancel();  // cancel alarm for timeout
  auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - _writeStart);
  if (ec || _item == nullptr || _item->request->timeout() <= timePassed) {
    // Send failed
    FUERTE_LOG_DEBUG << "asyncWriteCallback (http): error '" << ec.message()
                     << "', this=" << this << "\n";
    if (_item != nullptr) {
      FUERTE_LOG_DEBUG << "asyncWriteCallback (http): timeoutLeft: "
                       << (_item->request->timeout() - timePassed).count()
                       << " milliseconds\n";
    }
    auto item = std::move(_item);

    // keepalive timeout may have expired
    auto err = translateError(ec, Error::WriteError);
    if (this->_timeoutOnReadWrite && err == fuerte::Error::Canceled) {
      err = fuerte::Error::Timeout;
    }
    if (item) { // may be null if connection was canceled
      if (ec == asio_ns::error::broken_pipe && nwrite == 0) {  // re-queue
        // Note that this has the potential to change the order in which requests
        // are sent off from the client call order to something else!
        sendRequest(std::move(item->request), item->callback);
      } else {
        // let user know that this request caused the error
        item->callback(err, std::move(item->request), nullptr);
      }
    } else {
      err = Error::Canceled;
    }

    auto state = this->_state.load();
    if (state != Connection::State::Failed) {
      // Stop current connection and try to restart a new one.
      this->restartConnection(err);
    } else {
      drainQueue(Error::Canceled);
      abortOngoingRequests(Error::Canceled);
      this->terminateActivity();    // will reset `active`
    }
    return;
  }
  FUERTE_ASSERT(_item != nullptr);

  // Send succeeded
  FUERTE_LOG_HTTPTRACE << "asyncWriteCallback: send succeeded "
                       << "this=" << this << "\n";

  // request is written we no longer need data for that
  _item->requestHeader.clear();

  // Continue with a read, use the remaining part of the timeout as
  // timeout:
  setTimeout(_item->request->timeout() - timePassed, TimeoutType::READ);    // extend timeout
  _timeoutOnReadWrite = false;

  _reading = true;
  this->asyncReadSome();  // listen for the response
}

// ------------------------------------
// Reading data
// ------------------------------------

// called by the async_read handler (called from IO thread)
template <SocketType ST>
void H1Connection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  this->_reading = false;
  // Do not cancel timeout now, because we might be going on to read!
  if (ec || _item == nullptr) {
    this->_proto.timer.cancel();

    FUERTE_LOG_DEBUG << "asyncReadCallback: Error while reading from socket: '"
                     << ec.message() << "' , this=" << this << "\n";

    auto state = this->_state.load();
    if (state != Connection::State::Failed) {
      // Restart connection, will invoke _item cb
      auto err = translateError(ec, Error::ReadError);
      if (_timeoutOnReadWrite && err == fuerte::Error::Canceled) {
        err = fuerte::Error::Timeout;
      }
      this->restartConnection(err);
    } else {
      drainQueue(Error::Canceled);
      abortOngoingRequests(Error::Canceled);
      this->terminateActivity();    // will reset `active`
    }
    return;
  }
  FUERTE_ASSERT(_item != nullptr);
  
  // Inspect the data we've received so far.
  size_t nparsed = 0;
  auto buffers = this->_receiveBuffer.data();  // no copy
  for (auto const& buffer : buffers) {
    const char* data = reinterpret_cast<const char*>(buffer.data());
    size_t n = http_parser_execute(&_parser, &_parserSettings, data, buffer.size());
    if (n != buffer.size()) {
      /* Handle error. Usually just close the connection. */
      std::string msg = "Invalid HTTP response in parser: '";
      msg.append(http_errno_description(HTTP_PARSER_ERRNO(&_parser))).append("'");
      FUERTE_LOG_ERROR << msg << ", this=" << this << "\n";
      this->shutdownConnection(Error::ProtocolError, msg);  // will cleanup _item
      return;
    }
    nparsed += n;
  }

  // Remove consumed data from receive buffer.
  this->_receiveBuffer.consume(nparsed);
  
  if (_messageComplete) {
    this->_proto.timer.cancel();  // got response in time

    // thread-safe access on IO-Thread
    if (!_responseBuffer.empty()) {
      _response->setPayload(std::move(_responseBuffer), 0);
    }

    try {
      _item->callback(Error::NoError, std::move(_item->request),
                      std::move(_response));
    } catch (...) {
      FUERTE_LOG_ERROR << "unhandled exception in fuerte callback\n";
    }

    _item.reset();
    FUERTE_LOG_HTTPTRACE << "asyncReadCallback: completed parsing "
                            "response this="
                         << this << "\n";

    asyncWriteNextRequest();  // send next request
    return;
  }

  _reading = true;
  FUERTE_LOG_HTTPTRACE << "asyncReadCallback: response not complete yet\n";
  this->asyncReadSome();  // keep reading from socket
  // leave read timeout in place!
}

/// Set timeout accordingly
template <SocketType ST>
void H1Connection<ST>::setTimeout(std::chrono::milliseconds millis, TimeoutType type) {
  if (millis.count() == 0) {
    FUERTE_LOG_TRACE << "deleting timeout of type " << (int) type << " this=" << this << "\n";
    this->_proto.timer.cancel();
    return;
  }

  FUERTE_LOG_TRACE << "setting timeout of type " << (int) type << " this=" << this << "\n";

  // expires_after cancels pending ops
  this->_proto.timer.expires_after(millis);
  this->_proto.timer.async_wait(
      [type, self = Connection::weak_from_this()](auto const& ec) {
        std::shared_ptr<Connection> s;
        if (ec || !(s = self.lock())) {  // was canceled / deallocated
          return;
        }
        auto* me = static_cast<H1Connection<ST>*>(s.get());
        if ((type == TimeoutType::WRITE && me->_writing) ||
            (type == TimeoutType::READ && me->_reading)) {
          FUERTE_LOG_DEBUG << "HTTP-Request timeout" << " this=" << me << "\n";
          me->_proto.cancel();
          me->_timeoutOnReadWrite = true;
          // We simply cancel all ongoing asynchronous operations, the completion
          // handlers will do the rest.
          return;
        } else if (type == TimeoutType::IDLE && !me->_writing & !me->_reading && me->_leased == 0) {
          if (!me->_active && me->_state == Connection::State::Connected) {
            int exp = 0;
            if (me->_leased.compare_exchange_strong(exp, -1)) {
              // Note that we check _leased here with std::memory_order_seq_cst
              // **before** we call `shutdownConnection` and actually kill the
              // connection. This is important, see below!
              FUERTE_LOG_DEBUG << "HTTP-Request idle timeout"
                    << " this=" << me << "\n";
              me->shutdownConnection(Error::CloseRequested);
              me->_leased.store(0, std::memory_order_seq_cst);
            }
          }
        }
        // In all other cases we do nothing, since we have been posted to the
        // iocontext but the thing we should be timing out has already completed.
      });
}

/// abort ongoing / unfinished requests
template <SocketType ST>
void H1Connection<ST>::abortOngoingRequests(const fuerte::Error ec) {
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
void H1Connection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_queue.pop(item)) {
    FUERTE_ASSERT(item);
    std::unique_ptr<RequestItem> guard(item);
    uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);
    guard->invokeOnError(ec);
  }
}

// Lease this connection (cancel idle alarm)
template <SocketType ST>
bool H1Connection<ST>::lease() {
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

// Unlease this connection, has to be done if no sendRequest was called
// on it after the lease.
template <SocketType ST>
void H1Connection<ST>::unlease() {
  int exp = 1;
  // Undo the lease, we do not care if this has not worked.
  this->_leased.compare_exchange_strong(exp, 0);
  FUERTE_LOG_TRACE << "Connection un leased: this=" << this;
}

template class arangodb::fuerte::v1::http::H1Connection<SocketType::Tcp>;
template class arangodb::fuerte::v1::http::H1Connection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::http::H1Connection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::http
