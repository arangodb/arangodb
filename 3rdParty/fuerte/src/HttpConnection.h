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
#pragma once
#ifndef ARANGO_CXX_DRIVER_HTTP_CONNECTION_H
#define ARANGO_CXX_DRIVER_HTTP_CONNECTION_H 1

#include <atomic>
#include <chrono>

#include <boost/lockfree/queue.hpp>

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>

#include "GeneralConnection.h"

#include "http.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// Implements a client->server connection using node.js http-parser
template <SocketType ST>
class HttpConnection final : public fuerte::GeneralConnection<ST> {
 public:
  explicit HttpConnection(EventLoopService& loop,
                          detail::ConnectionConfiguration const&);
  ~HttpConnection();

 public:
  /// Start an asynchronous request.
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  /// @brief Return the number of requests that have not yet finished.
  size_t requestsLeft() const override {
    return _numQueued.load(std::memory_order_acquire);
  }

 protected:
  void finishConnect() override;

  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting() override;

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  void abortOngoingRequests(const fuerte::Error) override;

  /// abort all requests lingering in the queue
  void drainQueue(const fuerte::Error) override;

 private:
  // build request body for given request
  std::string buildRequestBody(Request const& req);

  /// set the timer accordingly
  void setTimeout(std::chrono::milliseconds);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCb(asio_ns::error_code const&, std::unique_ptr<RequestItem>);

 private:
  static int on_message_begin(http_parser* parser);
  static int on_status(http_parser* parser, const char* at, size_t len);
  static int on_header_field(http_parser* parser, const char* at, size_t len);
  static int on_header_value(http_parser* parser, const char* at, size_t len);
  static int on_header_complete(http_parser* parser);
  static int on_body(http_parser* parser, const char* at, size_t len);
  static int on_message_complete(http_parser* parser);

 private:
  /// elements to send out
  boost::lockfree::queue<fuerte::v1::http::RequestItem*,
                         boost::lockfree::capacity<1024>>
      _queue;

  /// cached authentication header
  std::string _authHeader;

  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;

  /// is loop active
  std::atomic<uint32_t> _numQueued;
  std::atomic<bool> _active;

  // parser state
  std::string _lastHeaderField;
  std::string _lastHeaderValue;

  /// response buffer, moved after writing
  velocypack::Buffer<uint8_t> _responseBuffer;

  /// currently in-flight request item
  std::unique_ptr<RequestItem> _item;
  /// response data, may be null before response header is received
  std::unique_ptr<arangodb::fuerte::v1::Response> _response;

  std::chrono::milliseconds _idleTimeout;
  bool _lastHeaderWasValue = false;
  bool _shouldKeepAlive = false;
  bool _messageComplete = false;
};
}}}}  // namespace arangodb::fuerte::v1::http

#endif
