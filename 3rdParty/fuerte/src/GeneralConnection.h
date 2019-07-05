////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
#ifndef ARANGO_CXX_DRIVER_GENERAL_CONNECTION_H
#define ARANGO_CXX_DRIVER_GENERAL_CONNECTION_H 1

#include <fuerte/connection.h>
#include <fuerte/types.h>

#include "AsioSockets.h"

namespace arangodb { namespace fuerte {

// HttpConnection implements a client->server connection using
// the node http-parser
template<SocketType ST>
class GeneralConnection : public fuerte::Connection {
public:
  explicit GeneralConnection(EventLoopService& loop,
                             detail::ConnectionConfiguration const&);
  virtual ~GeneralConnection() {}

  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }
  
  /// @brief cancel the connection, unusable afterwards
  void cancel() override;
  
  // Activate this connection
  void startConnection() override;
  
 protected:
  
  // shutdown connection, cancel async operations
  void shutdownConnection(const fuerte::Error);
  
  // Connect with a given number of retries
  void tryConnect(unsigned retries);
  
  void restartConnection(const Error error);
  
  // Call on IO-Thread: read from socket
  void asyncReadSome();
  
 protected:
  
  virtual void finishConnect() = 0;
  
  /// begin writing
  virtual void startWriting() = 0;
  
  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&) = 0;
  
  /// abort ongoing / unfinished requests
  virtual void abortOngoingRequests(const fuerte::Error) = 0;
  
  /// abort all requests lingering in the queue
  virtual void drainQueue(const fuerte::Error) = 0;

 protected:
  /// @brief io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  /// @brief underlying socket
  Socket<ST> _protocol;
  /// @brief timer to handle connection / request timeouts
  asio_ns::steady_timer _timeout;
  
  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;

  /// @brief is the connection established
  std::atomic<Connection::State> _state;
//  std::chrono::milliseconds _idleTimeout;
};

}}  // namespace arangodb::fuerte

#endif
