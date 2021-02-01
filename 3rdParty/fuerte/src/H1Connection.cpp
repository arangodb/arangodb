////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2020 ArangoDB GmbH, Cologne, Germany
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

RequestItem::RequestItem(std::unique_ptr<Request>&& req, RequestCallback&& cb,
                         std::string&& header)
    : requestHeader(std::move(header)),
      callback(std::move(cb)),
      request(std::move(req)) {}

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
    : GeneralConnection<ST, RequestItem>(loop, config),
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

  // preemptively cache
  if (this->_config._authenticationType == AuthenticationType::Basic) {
    _authHeader.append("Authorization: Basic ");
    _authHeader.append(fu::encodeBase64(
        this->_config._user + ":" + this->_config._password, true));
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
  abortRequests(Error::ConnectionCanceled, Clock::time_point::max());
} catch (...) {
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
void H1Connection<ST>::finishConnect() {
  // Note that the connection timeout alarm has already been disarmed.
  // If it has already gone off, we might have a completion handler
  // already posted on the iocontext. However, this will not touch anything
  // if we have first set the state to `Connected`.
  FUERTE_ASSERT(this->_active.load());
  auto exp = Connection::State::Connecting;
  if (this->_state.compare_exchange_strong(exp, Connection::State::Connected)) {
    this->asyncWriteNextRequest();  // starts writing if queue non-empty
  } else {
    FUERTE_LOG_ERROR << "finishConnect: found state other than 'Connecting': "
                     << static_cast<int>(exp);
    FUERTE_ASSERT(false);
    // If this happens, we probably have a sleeping barber
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

template <SocketType ST>
std::string H1Connection<ST>::buildRequestHeader(Request const& req) {
  // build the request header
  FUERTE_ASSERT(req.header.restVerb != RestVerb::Illegal);

  std::string header;
  header.reserve(256);  // TODO is there a meaningful size ?
  header.append(fu::to_string(req.header.restVerb));
  header.push_back(' ');

  http::appendPath(req, /*target*/ header);

  header.append(" HTTP/1.1\r\n")
      .append("Host: ")
      .append(this->_config._host)
      .append("\r\n");
  // technically not required for http 1.1
  header.append("Connection: Keep-Alive\r\n");

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
  FUERTE_ASSERT(state == Connection::State::Connected);
  FUERTE_ASSERT(_item == nullptr);

  RequestItem* ptr = nullptr;
  if (!this->_queue.pop(ptr)) {  // check
    this->_active.store(false);  // set
    if (this->_queue.empty()) {  // check again
      FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: stopped writing, this="
                           << this << "\n";
      if (_shouldKeepAlive) {
        this->setIOTimeout();
      } else {
        FUERTE_LOG_HTTPTRACE << "no keep-alive set, this=" << this << "\n";
        this->shutdownConnection(Error::CloseRequested);
      }
      return;
    }
    if (this->_active.exchange(true)) {
      return;  // someone else restarted
    }
    bool success = this->_queue.pop(ptr);
    FUERTE_ASSERT(success);
  }
  uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
  FUERTE_ASSERT(_item.get() == nullptr);
  FUERTE_ASSERT(ptr != nullptr);
  FUERTE_ASSERT(q > 0);

  _item.reset(ptr);

  std::array<asio_ns::const_buffer, 2> buffers;
  buffers[0] =
      asio_ns::buffer(_item->requestHeader.data(), _item->requestHeader.size());
  // GET and HEAD have no payload
  if (_item->request->header.restVerb != RestVerb::Get &&
      _item->request->header.restVerb != RestVerb::Head) {
    buffers[1] = _item->request->payload();
  }

  this->_writing = true;
  this->setIOTimeout();
  asio_ns::async_write(
      this->_proto.socket, std::move(buffers),
      [self(Connection::shared_from_this())](asio_ns::error_code const& ec,
                                             std::size_t nwrite) {
        static_cast<H1Connection<ST>&>(*self).asyncWriteCallback(ec, nwrite);
      });
  FUERTE_LOG_HTTPTRACE << "asyncWriteNextRequest: done, this=" << this << "\n";
}

// called by the async_write handler (called from IO thread)
template <SocketType ST>
void H1Connection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                          size_t nwrite) {
  FUERTE_ASSERT(this->_writing);
  // A connection can go to Closed state essentially at any time
  // and in this case _active will already be false if we get back here.
  // Therefore we cannot assert on it being true, which would otherwise be
  // correct.
  FUERTE_ASSERT(this->_state == Connection::State::Connected ||
                this->_state == Connection::State::Closed);
  this->_writing = false;  // indicate that no async write is ongoing any more
  this->_proto.timer.cancel();  // cancel alarm for timeout

  auto const now = Clock::now();
  if (ec || _item == nullptr || _item->expires < now) {
    // Send failed
    FUERTE_LOG_DEBUG << "asyncWriteCallback (http): error '" << ec.message()
                     << "', this=" << this << "\n";
    if (_item != nullptr) {
      FUERTE_LOG_DEBUG << "asyncWriteCallback (http): timeoutLeft: "
                       << std::chrono::duration_cast<std::chrono::milliseconds>(
                              _item->expires - now)
                              .count()
                       << " milliseconds\n";
    }

    this->shutdownConnection(translateError(ec, Error::WriteError));
    return;
  }
  FUERTE_ASSERT(_item != nullptr);

  // Send succeeded
  FUERTE_LOG_HTTPTRACE << "asyncWriteCallback: send succeeded "
                       << "this=" << this << "\n";

  // request is written we no longer need data for that
  _item->requestHeader.clear();

  this->asyncReadSome();  // listen for the response
}

// ------------------------------------
// Reading data
// ------------------------------------

// called by the async_read handler (called from IO thread)
template <SocketType ST>
void H1Connection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  // Do not cancel timeout now, because we might be going on to read!
  if (ec || _item == nullptr) {
    this->_proto.timer.cancel();

    FUERTE_LOG_DEBUG << "asyncReadCallback: Error while reading from socket: '"
                     << ec.message() << "' , this=" << this << "\n";

    this->shutdownConnection(translateError(ec, Error::ReadError));
    return;
  }
  FUERTE_ASSERT(_item != nullptr);

  // Inspect the data we've received so far.
  size_t nparsed = 0;
  auto buffers = this->_receiveBuffer.data();  // no copy
  for (auto const& buffer : buffers) {
    const char* data = reinterpret_cast<const char*>(buffer.data());
    size_t n =
        http_parser_execute(&_parser, &_parserSettings, data, buffer.size());
    if (n != buffer.size()) {
      /* Handle error. Usually just close the connection. */
      std::string msg = "Invalid HTTP response in parser: '";
      msg.append(http_errno_description(HTTP_PARSER_ERRNO(&_parser)))
          .append("'");
      FUERTE_LOG_ERROR << msg << ", this=" << this << "\n";
      this->shutdownConnection(Error::ProtocolError,
                               msg);  // will cleanup _item
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

  FUERTE_LOG_HTTPTRACE << "asyncReadCallback: response not complete yet this="
                       << this << "\n";
  this->asyncReadSome();  // keep reading from socket
  // leave read timeout in place!
}

/// abort ongoing / unfinished requests
template <SocketType ST>
void H1Connection<ST>::abortRequests(fuerte::Error err, Clock::time_point) {
  // simon: thread-safe, only called from IO-Thread
  // (which holds shared_ptr) and destructors
  if (_item) {
    // Item has failed, remove from message store
    _item->invokeOnError(err);
    _item.reset();
  }
}

/// Set timeout accordingly
template <SocketType ST>
void H1Connection<ST>::setIOTimeout() {
  const bool isIdle = _item == nullptr;
  if (isIdle && !this->_config._useIdleTimeout) {
    asio_ns::error_code ec;
    this->_proto.timer.cancel(ec);
    return;
  }

  const bool wasReading = this->_reading;
  const bool wasWriting = this->_writing;
  auto tp = _item ? _item->expires : Clock::now() + this->_config._idleTimeout;

  // expires_after cancels pending ops
  this->_proto.timer.expires_at(tp);
  this->_proto.timer.async_wait(
      [=, self = Connection::weak_from_this()](auto const& ec) {
        std::shared_ptr<Connection> s;
        if (ec || !(s = self.lock())) {  // was canceled / deallocated
          return;
        }

        auto& me = static_cast<H1Connection<ST>&>(*s);
        if ((wasWriting && me._writing) || (wasReading && me._reading)) {
          FUERTE_LOG_DEBUG << "HTTP-Request timeout"
                           << " this=" << &me << "\n";
          me._timeoutOnReadWrite = true;
          me._proto.cancel();
          // We simply cancel all ongoing asynchronous operations, the
          // completion handlers will do the rest.
          return;
        } else if (isIdle && !me._writing & !me._reading) {
          if (!me._active && me._state == Connection::State::Connected) {
            FUERTE_LOG_DEBUG << "HTTP-Request idle timeout"
                             << " this=" << &me << "\n";
            me.shutdownConnection(Error::CloseRequested);
          }
        }
        // In all other cases we do nothing, since we have been posted to the
        // iocontext but the thing we should be timing out has already
        // completed.
      });
}

template class arangodb::fuerte::v1::http::H1Connection<SocketType::Tcp>;
template class arangodb::fuerte::v1::http::H1Connection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::http::H1Connection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::http
