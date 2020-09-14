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
#pragma once
#ifndef ARANGO_CXX_DRIVER_H1_CONNECTION_H
#define ARANGO_CXX_DRIVER_H1_CONNECTION_H 1

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>

#include <atomic>
#include <chrono>

#include "GeneralConnection.h"
#include "http.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// in-flight request data
struct RequestItem {
  RequestItem(std::unique_ptr<Request>&& req, RequestCallback&& cb,
              std::string&& h);

  /// the request header
  std::string requestHeader;

  /// Callback for when request is done (in error or succeeded)
  RequestCallback callback;

  /// Reference to the request we're processing
  std::unique_ptr<arangodb::fuerte::v1::Request> request;

  /// point in time when the request expires
  std::chrono::steady_clock::time_point expires;

  inline void invokeOnError(Error e) {
    callback(e, std::move(request), nullptr);
  }
};

// Implements a client->server connection using node.js http-parser
template <SocketType ST>
class H1Connection final : public fuerte::GeneralConnection<ST, RequestItem> {
 public:
  explicit H1Connection(EventLoopService& loop,
                        detail::ConnectionConfiguration const&);
  ~H1Connection();

  /// The following public methods can be called from any thread.
 public:
  /// @brief Return the number of requests that have not yet finished.
  size_t requestsLeft() const override;

  /// All methods below here must only be called from the IO thread.
 protected:
  virtual void finishConnect() override;

  /// perform writes
  virtual void doWrite() override { this->asyncWriteNextRequest(); }

  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  virtual void abortRequests(fuerte::Error err, Clock::time_point now) override;

  virtual void setIOTimeout() override;

  virtual std::unique_ptr<RequestItem> createRequest(
      std::unique_ptr<Request>&& req, RequestCallback&& cb) override {
    auto h = buildRequestHeader(*req);
    return std::make_unique<RequestItem>(std::move(req), std::move(cb),
                                         std::move(h));
  }

 private:
  // build request body for given request
  std::string buildRequestHeader(Request const& req);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const&, size_t nwrite);

  fuerte::Error translateError(asio_ns::error_code const& e,
                               fuerte::Error c) const {
#ifdef _WIN32
    if (this->_timeoutOnReadWrite && (c == Error::ReadError ||
                                      c == Error::WriteError)) {
      return Error::RequestTimeout;
    }
#endif
    
    if (e == asio_ns::error::misc_errors::eof ||
        e == asio_ns::error::connection_reset) {
      return fuerte::Error::ConnectionClosed;
    } else if (e == asio_ns::error::operation_aborted) {
      // keepalive timeout may have expired
      return this->_timeoutOnReadWrite ? fuerte::Error::RequestTimeout :
                                         fuerte::Error::ConnectionCanceled;
    }
    return c;
  }

 private:
  static int on_message_begin(http_parser* p);
  static int on_status(http_parser* p, const char* at, size_t len);
  static int on_header_field(http_parser* p, const char* at, size_t len);
  static int on_header_value(http_parser* p, const char* at, size_t len);
  static int on_header_complete(http_parser* p);
  static int on_body(http_parser* p, const char* at, size_t len);
  static int on_message_complete(http_parser* p);

 private:
  /// cached authentication header
  std::string _authHeader;

  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;

  // parser state
  std::string _lastHeaderField;
  std::string _lastHeaderValue;

  /// response buffer, moved after writing
  velocypack::Buffer<uint8_t> _responseBuffer;

  /// currently in-flight request item
  std::unique_ptr<RequestItem> _item;
  /// response data, may be null before response header is received
  std::unique_ptr<arangodb::fuerte::v1::Response> _response;

  bool _lastHeaderWasValue = false;
  bool _shouldKeepAlive = false;
  bool _messageComplete = false;
};
}}}}  // namespace arangodb::fuerte::v1::http

#endif
