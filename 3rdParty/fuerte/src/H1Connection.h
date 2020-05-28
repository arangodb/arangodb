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

#include "GeneralConnection.h"
#include "http.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// Implements a client->server connection using node.js http-parser
template <SocketType ST>
class H1Connection final : public fuerte::GeneralConnection<ST> {
 public:
  explicit H1Connection(EventLoopService& loop,
                        detail::ConnectionConfiguration const&);
  ~H1Connection();

  /// The following public methods can be called from any thread.
 public:
  /// Start an asynchronous request.
  void sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  /// @brief Return the number of requests that have not yet finished.
  size_t requestsLeft() const override;

  /// All methods below here must only be called from the IO thread.
 protected:
  /// This is posted by `sendRequest` to the _io_context thread, the `_active`
  /// flag is already set to `true` by an exchange operation
  void activate();

  void finishConnect() override;

  /// The following is called when the connection is permanently failed. It is
  /// used to shut down any activity in a way that avoids sleeping barbers
  void terminateActivity() override;

  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting() override;

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  void abortOngoingRequests(const fuerte::Error) override;

  /// abort all requests lingering in the queue
  void drainQueue(const fuerte::Error) override;

 private:
  // Reason for timeout:
  enum class TimeoutType: int {
    IDLE = 0,
    READ = 1,
    WRITE = 2
  };

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
  void setTimeout(std::chrono::milliseconds, TimeoutType type);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const&, size_t nwrite);

 private:
  static int on_message_begin(http_parser* p);
  static int on_status(http_parser* p, const char* at, size_t len);
  static int on_header_field(http_parser* p, const char* at, size_t len);
  static int on_header_value(http_parser* p, const char* at, size_t len);
  static int on_header_complete(http_parser* p);
  static int on_body(http_parser* p, const char* at, size_t len);
  static int on_message_complete(http_parser* p);

 private:
  /// elements to send out
  boost::lockfree::queue<RequestItem*, boost::lockfree::capacity<64>> _queue;

  /// cached authentication header
  std::string _authHeader;

  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;
  
  std::atomic<bool> _active;  /// is loop active
  bool _reading;    // set between starting an asyncRead operation and executing
                    // the completion handler
  bool _writing;    // set between starting an asyncWrite operation and executing
                    // the completion handler
  // both are used in the timeout handlers to decide if the timeout still
  // has to have an effect or if it is merely still on the iocontext and is now
  // obsolete.
  std::chrono::steady_clock::time_point _writeStart;
  // This is the time when the latest write operation started.
  // We use this to compute the timeout of the corresponding read if the
  // write was done.

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
