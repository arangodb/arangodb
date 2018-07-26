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

#ifndef ARANGO_CXX_DRIVER_ASIO_CONNECTION_H
#define ARANGO_CXX_DRIVER_ASIO_CONNECTION_H 1

#include <atomic>
#include <mutex>

#include <fuerte/asio_ns.h>
#include <boost/lockfree/queue.hpp>

#include <fuerte/connection.h>
#include <fuerte/loop.h>

#include "MessageStore.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
// Connection object that handles sending and receiving of Velocystream
// Messages.
//
// A AsioConnection tries to create a connection to one of the given peers on a
// a socket. Then it tries to do an asyncronous ssl handshake. When the
// handshake is done it calls the finishInitialization method to notify
// the protocol subclass. Subclasses are responsible for the proper starting
// and stopping of IO operations. They may use the _loopState variable to
// synchronize in a thread-safe manner.
// Error conditions will usually trigger a connection restart. Subclasses can
// perform cleanup operations in their shutdownConnection method
template <typename T>
class AsioConnection : public Connection {
 public:
  AsioConnection(std::shared_ptr<asio_ns::io_context> const&,
                 detail::ConnectionConfiguration const&);

  virtual ~AsioConnection();

 public:
  /// Activate the connection.
  void startConnection() override final;
  
  /// called on shutdown, always call superclass
  virtual void shutdownConnection(const ErrorCondition) override;

  // Return the number of unfinished requests.
  virtual size_t requestsLeft() const override {
    return _loopState.load(std::memory_order_acquire) & WRITE_LOOP_QUEUE_MASK;
  }
  
  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }

  bool hasCapacity() const {
    return (_loopState.load() & WRITE_LOOP_QUEUE_MASK) < 1024;
  }

 private:
  // SOCKET HANDLING /////////////////////////////////////////////////////////
  void initSocket();
  void shutdownSocket();

  // resolve the host into a series of endpoints
  void startResolveHost();

  // establishes connection and initiates handshake
  void startConnect(asio_ns::ip::tcp::resolver::iterator);
  void asyncConnectCallback(asio_ns::error_code const& ec,
                            asio_ns::ip::tcp::resolver::iterator);

  // start intiating an SSL connection (on top of an established TCP socket)
  void startSSLHandshake();

 protected:
  void restartConnection(const ErrorCondition);

  // Thread-Safe: reset io loop flags
  void stopIOLoops();

  // Thread-Safe: activate the writer loop (if off and items are queud)
  // void startWriting();
  // Thread-Safe: stop the write loop
  // void stopWriting();

  // Thread-Safe: queue a new request. Returns loop state
  uint32_t queueRequest(std::unique_ptr<T>);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // Thread-Safe: activate the receiver loop (if needed)
  // void startReading();
  // Thread-Safe: stop the read loop
  // uint32_t stopReading();

  // Call on IO-Thread: read from socket
  void asyncReadSome();

 protected:
  // socket connection is up (with optional SSL)
  virtual void finishInitialization() = 0;

  // fetch the buffers for the write-loop (called from IO thread)
  virtual std::vector<asio_ns::const_buffer> prepareRequest(
      std::shared_ptr<T> const&) = 0;

  // called by the async_write handler (called from IO thread)
  virtual void asyncWriteCallback(asio_ns::error_code const& error,
                                  size_t transferred, std::shared_ptr<T>) = 0;
  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&,
                                 size_t transferred) = 0;

 protected:
  /// io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  // host resolver
  asio_ns::ip::tcp::resolver _resolver;
  /// endpoints to use
  asio_ns::ip::tcp::resolver::iterator _endpoints;

  /// mutex to make socket setup and shutdown exclusive
  std::mutex _socket_mutex;
  /// socket to use
  std::shared_ptr<asio_ns::ip::tcp::socket> _socket;
  /// @brief timer to handle connection / request timeouts
  asio_ns::steady_timer _timeout;

  /// SSL context, may be null
  std::shared_ptr<asio_ns::ssl::context> _sslContext;
  /// SSL steam socket, may be null
  std::shared_ptr<asio_ns::ssl::stream<::asio_ns::ip::tcp::socket&>>
      _sslSocket;

  /// @brief is the connection established
  std::atomic<Connection::State> _state;

  /// stores in-flight messages (thread-safe)
  MessageStore<T> _messageStore;

  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;

  /// highest two bits mean read or write loops are active
  /// low 30 bit contain number of queued request items
  std::atomic<uint32_t> _loopState;
  static constexpr uint32_t READ_LOOP_ACTIVE = 1 << 31;
  static constexpr uint32_t WRITE_LOOP_ACTIVE = 1 << 30;
  static constexpr uint32_t LOOP_FLAGS = READ_LOOP_ACTIVE | WRITE_LOOP_ACTIVE;
  static constexpr uint32_t WRITE_LOOP_QUEUE_INC = 1;
  static constexpr uint32_t WRITE_LOOP_QUEUE_MASK = WRITE_LOOP_ACTIVE - 1;
  static_assert((WRITE_LOOP_ACTIVE & WRITE_LOOP_QUEUE_MASK) == 0, "");
  static_assert((WRITE_LOOP_ACTIVE & READ_LOOP_ACTIVE) == 0, "");

  /// elements to send out
  boost::lockfree::queue<T*> _writeQueue;
};
}}}  // namespace arangodb::fuerte::v1
#endif
