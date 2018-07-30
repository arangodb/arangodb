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

#include "AsioConnection.h"

#include <fuerte/FuerteLogger.h>

#include "Basics/cpu-relax.h"
#include "http.h"
#include "vst.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
using bt = ::asio_ns::ip::tcp;
using be = ::asio_ns::ip::tcp::endpoint;

template <typename RT, fuerte::SocketType ST>
AsioConnection<RT, ST>::AsioConnection(
    std::shared_ptr<asio_ns::io_context> const& ctx,
    detail::ConnectionConfiguration const& config)
    : Connection(config),
      _io_context(ctx),
      _protocol(*ctx),
      _timeout(*ctx),
      _state(Connection::State::Disconnected),
      _loopState(0),
      _writeQueue(1024) {
    }

// Deconstruct.
template <typename RT, fuerte::SocketType ST>
AsioConnection<RT, ST>::~AsioConnection() {
  _protocol.shutdown();
  if (!_messageStore.empty()) {
    FUERTE_LOG_TRACE << "DESTROYING CONNECTION WITH: "
                         << _messageStore.size() << " outstanding requests!"
                         << std::endl;
  }
  shutdownConnection(ErrorCondition::Canceled);
}

// Activate this connection.
template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::startConnection() {
  
  // start connecting only if state is disconnected
  Connection::State exp = Connection::State::Disconnected;
  if (!_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    FUERTE_LOG_ERROR << "already resolving endpoint\n";
    return;
  }
  
  auto self = shared_from_this();
  _protocol.connect(_config, [self, this](asio_ns::error_code const& ec) {
    if (ec) {
      FUERTE_LOG_ERROR << "connecting failed: error=" << ec.message() << std::endl;
      this->shutdownConnection(ErrorCondition::CouldNotConnect);
      this->onFailure(errorToInt(ErrorCondition::CouldNotConnect),
                      "connecting failed: error" + ec.message());
    } else {
      this->finishInitialization();
    }
  });
}

// Cleanup //////////////////////////////////////////////////////////

// shutdown the connection and cancel all pending messages.
template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::shutdownConnection(const ErrorCondition ec) {
  FUERTE_LOG_CALLBACKS << "shutdownConnection\n";

  _state.store(State::Disconnected, std::memory_order_release);
  
  // cancel timeouts
  _timeout.cancel();
  
  // Close socket
  _protocol.shutdown();
  
  // Stop the read & write loop
  stopIOLoops();
  
  // Cancel all items and remove them from the message store.
  _messageStore.cancelAll(ec);
  
  RT* item = nullptr;
  while (_writeQueue.pop(item)) {
    std::unique_ptr<RT> guard(item);
    _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_release);
    guard->invokeOnError(errorToInt(ec));
  }
  
  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
}

template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::restartConnection(const ErrorCondition error) {
  // Read & write loop must have been reset by now

  FUERTE_LOG_CALLBACKS << "restartConnection" << std::endl;
  // restarting needs to be an exclusive operation
  Connection::State exp = Connection::State::Connected;
  if (_state.compare_exchange_strong(exp, Connection::State::Disconnected)) {
    shutdownConnection(error); // Terminate connection
    startConnection(); // will check state
  }
}

// Thread-Safe: reset io loop flags
template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::stopIOLoops() {
  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  while (state & LOOP_FLAGS) {
    if (_loopState.compare_exchange_weak(state, state & ~LOOP_FLAGS,
                                         std::memory_order_seq_cst)) {
      FUERTE_LOG_TRACE << "stopIOLoops: stopped" << std::endl;
      return;  // we turned flag off while nothin was queued
    }
    cpu_relax();
  }
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: queue a new request
template <typename RT, fuerte::SocketType ST>
uint32_t AsioConnection<RT, ST>::queueRequest(std::unique_ptr<RT> item) {
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capactiy exceeded" << std::endl;
    throw std::length_error("connection queue capactiy exceeded");
  }
  item.release();
  // WRITE_LOOP_ACTIVE, READ_LOOP_ACTIVE are synchronized via cmpxchg
  return _loopState.fetch_add(WRITE_LOOP_QUEUE_INC, std::memory_order_seq_cst);
}

// writes data from task queue to network using asio_ns::async_write
template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::asyncWriteNextRequest() {
  FUERTE_LOG_TRACE << "asyncWrite: preparing to send next" << std::endl;
  if (_state.load(std::memory_order_acquire) != State::Connected) {
    FUERTE_LOG_TRACE << "asyncReadSome: permanent failure\n";
    stopIOLoops();  // will set the flag
    return;
  }

  // reduce queue length and check active flag
#ifndef NDEBUG
  uint32_t state =
#endif
    _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_acquire);
  assert((state & WRITE_LOOP_QUEUE_MASK) > 0);

  RT* ptr = nullptr;
#ifndef NDEBUG
  bool success =
#endif
    _writeQueue.pop(ptr);
  assert(success); // should never fail here
  std::shared_ptr<RT> item(ptr);

  // we stop the write-loop if we stopped it ourselves.
  auto self = shared_from_this();
  auto cb = [this, self, item](asio_ns::error_code const& ec, std::size_t transferred) {
    asyncWriteCallback(ec, transferred, std::move(item));
  };
  auto buffers = this->prepareRequest(item);
  asio_ns::async_write(_protocol.socket, buffers, cb);

  FUERTE_LOG_TRACE << "asyncWrite: done" << std::endl;
}

// ------------------------------------
// Reading data
// ------------------------------------

// asyncReadSome reads the next bytes from the server.
template <typename RT, fuerte::SocketType ST>
void AsioConnection<RT, ST>::asyncReadSome() {
  FUERTE_LOG_TRACE << "asyncReadSome: this=" << this << std::endl;

  if (!(_loopState.load(std::memory_order_seq_cst) & READ_LOOP_ACTIVE)) {
    FUERTE_LOG_TRACE << "asyncReadSome: read-loop was stopped\n";
    return;  // just stop
  }
  if (_state.load(std::memory_order_acquire) != State::Connected) {
    FUERTE_LOG_TRACE << "asyncReadSome: permanent failure\n";
    stopIOLoops();  // will set the flags
    return;
  }

  // Start reading data from the network.
  FUERTE_LOG_CALLBACKS << "r";
#if ENABLE_FUERTE_LOG_CALLBACKS > 0
  std::cout << "_messageMap = " << _messageStore.keys() << std::endl;
#endif

  assert(_socket);
  auto self = shared_from_this();
  auto cb = [this, self](asio_ns::error_code const& ec, size_t transferred) {
    // received data is "committed" from output sequence to input sequence
    _receiveBuffer.commit(transferred);
    asyncReadCallback(ec, transferred);
  };

  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  _protocol.socket.async_read_some(mutableBuff, std::move(cb));

  FUERTE_LOG_TRACE << "asyncReadSome: done" << std::endl;
}

template class arangodb::fuerte::v1::AsioConnection<
  arangodb::fuerte::v1::vst::RequestItem, SocketType::Tcp>;
template class arangodb::fuerte::v1::AsioConnection<
    arangodb::fuerte::v1::http::RequestItem, SocketType::Tcp>;
  
template class arangodb::fuerte::v1::AsioConnection<
  arangodb::fuerte::v1::vst::RequestItem, SocketType::Ssl>;
template class arangodb::fuerte::v1::AsioConnection<
  arangodb::fuerte::v1::http::RequestItem, SocketType::Ssl>;
}}}  // namespace arangodb::fuerte::v1
