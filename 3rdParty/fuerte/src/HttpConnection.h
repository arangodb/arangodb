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

#include <fuerte/connection.h>
#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>
#include <fuerte/types.h>

#include "AsioSockets.h"
#include "http.h"
#include "http_parser/http_parser.h"
#include "MessageStore.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// HttpConnection implements a client->server connection using
// the node http-parser 
template<SocketType ST>
class HttpConnection final : public fuerte::Connection {
 public:
  explicit HttpConnection(EventLoopService& loop,
                          detail::ConnectionConfiguration const&);
  ~HttpConnection();

 public:
  
  /// Start an asynchronous request.
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;
  
  /// @brief Return the number of unfinished requests.
  std::size_t requestsLeft() const override {
    return _numQueued.load(std::memory_order_acquire);
  }
  
  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }

  /// @brief cancel the connection, unusable afterwards
  void cancel() override;
  
 protected:
  
  // Activate this connection
  void startConnection() override;
  
 private:
  
  // Connect with a given number of retries
  void tryConnect(unsigned retries);
  
  // shutdown connection, cancel async operations
  void shutdownConnection(const ErrorCondition);
  
  // restart connection
  void restartConnection(const ErrorCondition);
  
  // build request body for given request
  std::string buildRequestBody(Request const& req);
  
  /// set the timer accordingly
  void setTimeout(std::chrono::milliseconds);
  
  /// Thread-Safe: activate the writer if needed
  void startWriting();
  
  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();
  
  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& error,
                          size_t transferred,
                          std::shared_ptr<RequestItem>);
  
  // Call on IO-Thread: read from socket
  void asyncReadSome();

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&,
                         size_t transferred);

 private:
  class Options {
   public:
    double connectionTimeout = 2.0;
  };
  
 private:
  
  /// @brief io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  Socket<ST> _protocol;
  /// @brief timer to handle connection / request timeouts
  asio_ns::steady_timer _timeout;
  
  /// @brief is the connection established
  std::atomic<Connection::State> _state;
  
  /// is loop active
  std::atomic<uint32_t> _numQueued;
  std::atomic<bool> _active;
  
  /// elements to send out
  boost::lockfree::queue<fuerte::v1::http::RequestItem*,
    boost::lockfree::capacity<1024>> _queue;
  
  /// cached authentication header
  std::string _authHeader;
  
  /// currently in-flight request
  std::shared_ptr<RequestItem> _inFlight;
  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;
  
  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;
};
}}}}  // namespace arangodb::fuerte::v1::http

#endif
