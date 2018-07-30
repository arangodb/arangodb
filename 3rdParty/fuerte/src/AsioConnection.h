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
  
template<SocketType T>
struct Socket {};

template<>
struct Socket<SocketType::Tcp>  {
  Socket(asio_ns::io_context& ctx)
    : resolver(ctx), socket(ctx) {}
  
  ~Socket() {
    resolver.cancel();
    shutdown();
  }
  
  void connect(detail::ConnectionConfiguration const& config,
               std::function<void(asio_ns::error_code const&)> done) {
    auto cb = [this, done](asio_ns::error_code const& ec,
                     asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      // Start the asynchronous connect operation.
      asio_ns::async_connect(socket, it,
                             [done](asio_ns::error_code const& ec,
                                    asio_ns::ip::tcp::resolver::iterator const&) {
                               done(ec);
                             });
    };
    // Resolve the host asynchronous into a series of endpoints
    resolver.async_resolve({config._host, config._port}, cb);
  }
  void shutdown() {
    if (socket.is_open()) {
      asio_ns::error_code error; // prevents exceptions
      socket.cancel(error);
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, error);
      socket.close(error);
    }
  }
  
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
};

template<>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(asio_ns::io_context& ctx)
  : context(asio_ns::ssl::context::sslv23), resolver(ctx), socket(ctx, context) {}
  
  ~Socket() {
    resolver.cancel();
    shutdown();
  }
  
  void connect(detail::ConnectionConfiguration const& config,
               std::function<void(asio_ns::error_code const&)> done) {
    auto rcb = [this, done, &config](asio_ns::error_code const& ec,
                            asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      auto cbc = [this, done, &config](asio_ns::error_code const& ec,
                                       asio_ns::ip::tcp::resolver::iterator const&) {
        if (ec) {
          done(ec);
          return;
        }
        
        /*if (config._verifyHost) {
          // Perform SSL handshake and verify the remote host's certificate.
          socket.lowest_layer().set_option(asio_ns::ip::tcp::no_delay(true));
          socket.set_verify_mode(asio_ns::ssl::verify_peer);
          socket.set_verify_callback(asio_ns::ssl::rfc2818_verification(config._host));
        } else {*/
          socket.set_verify_mode(asio_ns::ssl::verify_none);
        //}
        
        //FUERTE_LOG_CALLBACKS << "starting ssl handshake " << std::endl;
        socket.async_handshake(asio_ns::ssl::stream_base::client,
                               [done](asio_ns::error_code const& ec) {
                                 done(ec);
                               });
      };
      
      // Start the asynchronous connect operation.
      asio_ns::async_connect(socket.lowest_layer(), it, cbc);
    };
    // Resolve the host asynchronous into a series of endpoints
    resolver.async_resolve({config._host, config._port}, rcb);
  }
  void shutdown() {
    //socket.cancel();
    if (socket.lowest_layer().is_open()) {
      asio_ns::error_code error;
      socket.shutdown(error);
      socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, error);
      socket.lowest_layer().close(error);
    }
  }
  
  // TODO move to EventLoop
  asio_ns::ssl::context context;
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
};

template<>
struct Socket<fuerte::SocketType::Unix> {
  asio_ns::local::stream_protocol _socket;
};
  
  
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
template <typename RT, fuerte::SocketType ST>
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

  /*bool hasCapacity() const {
    return (_loopState.load() & WRITE_LOOP_QUEUE_MASK) < 1024;
  }*/

 protected:
  void restartConnection(const ErrorCondition);

  // Thread-Safe: reset io loop flags
  void stopIOLoops();

  // Thread-Safe: queue a new request. Returns loop state
  uint32_t queueRequest(std::unique_ptr<RT>);

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
      std::shared_ptr<RT> const&) = 0;

  // called by the async_write handler (called from IO thread)
  virtual void asyncWriteCallback(asio_ns::error_code const& error,
                                  size_t transferred, std::shared_ptr<RT>) = 0;
  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&,
                                 size_t transferred) = 0;

 public:
  /// io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  Socket<ST> _protocol;
  
  /// @brief timer to handle connection / request timeouts
  asio_ns::steady_timer _timeout;

  /// @brief is the connection established
  std::atomic<Connection::State> _state;

  /// stores in-flight messages (thread-safe)
  MessageStore<RT> _messageStore;

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
  boost::lockfree::queue<RT*> _writeQueue;
};
  
}}}  // namespace arangodb::fuerte::v1
#endif
