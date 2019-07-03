////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_VST_CONNECTION_H
#define ARANGO_CXX_DRIVER_VST_CONNECTION_H 1

#include <boost/lockfree/queue.hpp>
#include <fuerte/connection.h>

#include "AsioSockets.h"
#include "MessageStore.h"
#include "vst.h"

// naming in this file will be closer to asio for internal functions and types
// functions that are exposed to other classes follow ArangoDB conding
// conventions

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

  
// Connection object that handles sending and receiving of Velocystream
// Messages.
template<SocketType ST>
class VstConnection final : public Connection {
 public:
  explicit VstConnection(EventLoopService& loop,
                         detail::ConnectionConfiguration const&);

  ~VstConnection();

 public:
  // this function prepares the request for sending
  // by creating a RequestItem and setting:
  //  - a messageid
  //  - the buffer to be send
  // this item is then moved to the request queue
  // and a write action is triggerd when there is
  // no other write in progress
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;
  
  // Return the number of unfinished requests.
  std::size_t requestsLeft() const override {
    return (_loopState.load(std::memory_order_acquire) & WRITE_LOOP_QUEUE_MASK) + _messageStore.size();
  }
  
  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }
  
  /// @brief cancel the connection, unusable afterwards
  void cancel() override final;
  
 protected:
 
  /// Activate the connection.
  void startConnection() override final;

 private:
  
  // Connect with a given number of retries
  void tryConnect(unsigned retries);
  
  /// shutdown connection, cancel async operations
  void shutdownConnection(const ErrorCondition);
  
  void restartConnection(const ErrorCondition);
  
  void finishInitialization();
  
  // Thread-Safe: reset io loop flags
  void stopIOLoops();
  
  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting();
  
  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();
  
  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& ec, size_t transferred,
                          std::shared_ptr<RequestItem>);
  
  // Thread-Safe: activate the read loop (if needed)
  void startReading();
  
  // Thread-Safe: stops read loop
  void stopReading();
  
  // Call on IO-Thread: read from socket
  void asyncReadSome();
  
  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&, size_t transferred);

 private:
  // Send out the authentication message on this connection
  void sendAuthenticationRequest();

  // Process the given incoming chunk.
  void processChunk(Chunk const& chunk);
  // Create a response object for given RequestItem & received response buffer.
  std::unique_ptr<Response> createResponse(RequestItem& item,
                                           std::unique_ptr<velocypack::Buffer<uint8_t>>&);
      
  // adjust the timeouts (only call from IO-Thread)
  void setTimeout();

 private:
  const VSTVersion _vstVersion;
      
  /// io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  Socket<ST> _protocol;
  
  /// @brief timer to handle connection / request timeouts
  asio_ns::steady_timer _timeout;
  
  /// @brief is the connection established
  std::atomic<Connection::State> _state;
  
  /// stores in-flight messages
  MessageStore<vst::RequestItem> _messageStore;
  
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
  boost::lockfree::queue<vst::RequestItem*,
    boost::lockfree::capacity<1024>> _writeQueue;
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
