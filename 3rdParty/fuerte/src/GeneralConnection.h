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
template <SocketType ST>
class GeneralConnection : public fuerte::Connection {
 public:
  explicit GeneralConnection(EventLoopService& loop,
                             detail::ConnectionConfiguration const&);
  virtual ~GeneralConnection() = default;

  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }

  /// The following public methods can be called from any thread:

  /// @brief cancel the connection, unusable afterwards
  void cancel() override;

  // Activate this connection
  void start() override;

  // Switch off potential idle alarm (used by connection pool). Returns
  // true if lease was successful and connection can be used afterwards.
  // If false is retured the connection is broken beyond repair.
  virtual bool lease() override;

  // Give back a lease when no sendRequest was called.
  virtual void unlease() override;

  /// All protected or private methods below here must only be called on the
  /// IO thread.
 protected:
  void startConnection();
  // shutdown connection, cancel async operations
  void shutdownConnection(const fuerte::Error, std::string const& msg = "",
                          bool mayRestart = false);

  void restartConnection(const Error error);

  // Call on IO-Thread: read from socket
  void asyncReadSome();
  
  virtual void finishConnect() = 0;

  /// The following is called when the connection is permanently failed. It is
  /// used to shut down any activity in the derived classes in a way that avoids
  /// sleeping barbers
  virtual void terminateActivity() = 0;

  /// begin writing,
  virtual void startWriting() = 0;

  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&) = 0;

  /// abort ongoing / unfinished requests (locally)
  virtual void abortOngoingRequests(const fuerte::Error) = 0;

  /// abort all requests lingering in the queue
  virtual void drainQueue(const fuerte::Error) = 0;
 
 private:
  // Connect with a given number of retries
  void tryConnect(unsigned retries, std::chrono::steady_clock::time_point start,
                  asio_ns::error_code const& ec);

 protected:
  /// @brief io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  /// @brief underlying socket
  Socket<ST> _proto;

  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;

  /// @brief is the connection established
  std::atomic<Connection::State> _state;

  std::atomic<uint32_t> _numQueued; /// queued items
};

}}  // namespace arangodb::fuerte

#endif
