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
#ifndef ARANGO_CXX_DRIVER_H1_CONNECTION_H
#define ARANGO_CXX_DRIVER_H1_CONNECTION_H 1

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <llhttp.h>

#include "GeneralConnection.h"
#include "http.h"


namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// Implements a client->server connection using node.js http-parser
template <SocketType ST>
class H1Connection final : public fuerte::GeneralConnection<ST> {
 public:
  explicit H1Connection(EventLoopService& loop,
                        detail::ConnectionConfiguration const&);
  ~H1Connection();

 public:
  /// Start an asynchronous request.
  void sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  /// @brief Return the number of requests that have not yet finished.
  size_t requestsLeft() const override;

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
  // in-flight request data
  struct RequestItem {
    /// the request header
    std::string requestHeader;

    /// Callback for when request is done (in error or succeeded)
    RequestCallback callback;

    /// Reference to the request we're processing
    std::unique_ptr<arangodb::fuerte::v1::Request> request;

    inline void invokeOnError(Error e) {
      callback(e, std::move(request), nullptr);
    }
  };

  // build request body for given request
  std::string buildRequestBody(Request const& req);

  /// set the timer accordingly
  void setTimeout(std::chrono::milliseconds);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const&, size_t nwrite);

 private:
  static int on_message_begin(llhttp_t* p);
  static int on_status(llhttp_t* p, const char* at, size_t len);
  static int on_header_field(llhttp_t* p, const char* at, size_t len);
  static int on_header_value(llhttp_t* p, const char* at, size_t len);
  static int on_header_complete(llhttp_t* p);
  static int on_body(llhttp_t* p, const char* at, size_t len);
  static int on_message_complete(llhttp_t* p);

 private:
  /// elements to send out
  boost::lockfree::queue<RequestItem*, boost::lockfree::capacity<64>> _queue;

  /// cached authentication header
  std::string _authHeader;

  /// the node http-parser
  llhttp_t _parser;
  llhttp_settings_t _parserSettings;

  std::atomic<bool> _active;  /// is loop active

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
