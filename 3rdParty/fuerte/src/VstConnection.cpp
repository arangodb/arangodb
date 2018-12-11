////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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

#include "VstConnection.h"

#include "Basics/cpu-relax.h"
#include "vst.h"

#include <fuerte/FuerteLogger.h>
#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>
#include <fuerte/types.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {
namespace fu = arangodb::fuerte::v1;
using arangodb::fuerte::v1::SocketType;

template<SocketType ST>
VstConnection<ST>::VstConnection(
    EventLoopService& loop,
    fu::detail::ConnectionConfiguration const& config)
    : Connection(config),
      _vstVersion(config._vstVersion),
      _io_context(loop.nextIOContext()),
      _protocol(loop, *_io_context),
      _timeout(*_io_context),
      _state(Connection::State::Disconnected),
      _loopState(0),
      _writeQueue() {}

template<SocketType ST>
VstConnection<ST>::~VstConnection() {
  shutdownConnection(ErrorCondition::Canceled);
}

static std::atomic<MessageID> vstMessageId(1);
// sendRequest prepares a RequestItem for the given parameters
// and adds it to the send queue.
template<SocketType ST>
MessageID VstConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                         RequestCallback cb) {
  
  // it does not matter if IDs are reused on different connections
  uint64_t mid = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  // Create RequestItem from parameters
  std::unique_ptr<RequestItem> item(new RequestItem());
  item->_messageID = mid;
  item->_request = std::move(req);
  item->_callback = cb;
  item->_expires = std::chrono::steady_clock::time_point::max();
  item->prepareForNetwork(_vstVersion);
  
  // Add item to send queue
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    throw std::length_error("connection queue capacity exceeded");
  }
  item.release();
  // WRITE_LOOP_ACTIVE, READ_LOOP_ACTIVE are synchronized via cmpxchg
  uint32_t loop = _loopState.fetch_add(WRITE_LOOP_QUEUE_INC, std::memory_order_seq_cst);
  FUERTE_LOG_VSTTRACE << "queued item: this=" << this << "\n";

  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = _state.load(std::memory_order_acquire);
  if (state == Connection::State::Connected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): start sending & reading\n";
    if (!(loop & WRITE_LOOP_ACTIVE)) {
      startWriting(); // try to start write loop
    }
  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): not connected\n";
    startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
  }
  return mid;
}
  
/// @brief cancel the connection, unusable afterwards
template <SocketType ST>
void VstConnection<ST>::cancel() {
  std::weak_ptr<Connection> self = shared_from_this();
  asio_ns::post(*_io_context, [self, this] {
    auto s = self.lock();
    if (s) {
      shutdownConnection(ErrorCondition::Canceled);
      _state.store(State::Failed);
    }
  });
}
  
// Activate this connection.
template <SocketType ST>
void VstConnection<ST>::startConnection() {
  // start connecting only if state is disconnected
  Connection::State exp = Connection::State::Disconnected;
  if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    tryConnect(_config._maxConnectRetries);
  }
}
  
// Connect with a given number of retries
template <SocketType ST>
void VstConnection<ST>::tryConnect(unsigned retries) {
  assert(_state.load(std::memory_order_acquire) == Connection::State::Connecting);
  
  auto self = shared_from_this();
  _protocol.connect(_config, [self, this, retries](asio_ns::error_code const& ec) {
    if (!ec) {
      finishInitialization();
      return;
    }
    FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
    if (retries > 0 && ec != asio_ns::error::operation_aborted) {
      tryConnect(retries - 1);
    } else {
      shutdownConnection(ErrorCondition::CouldNotConnect);
      onFailure(errorToInt(ErrorCondition::CouldNotConnect),
                "connecting failed: " + ec.message());
    }
  });
}
  
// shutdown the connection and cancel all pending messages.
template <SocketType ST>
void VstConnection<ST>::shutdownConnection(const ErrorCondition ec) {
  FUERTE_LOG_CALLBACKS << "shutdownConnection\n";
  
  if (_state.load() != State::Failed) {
    _state.store(State::Disconnected);
  }
  
  // cancel timeouts
  try {
    _timeout.cancel();
  } catch (...) {
    // cancel() may throw, but we are not allowed to throw here
    // as we may be called from the dtor
  }
  
  // Close socket
  _protocol.shutdown();
  
  // Reset the read & write loop
  stopIOLoops();
  
  // Cancel all items and remove them from the message store.
  _messageStore.cancelAll(ec);
  
  RequestItem* item = nullptr;
  while (_writeQueue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_release);
    guard->invokeOnError(errorToInt(ec));
  }
  
  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
  
template <SocketType ST>
void VstConnection<ST>::restartConnection(const ErrorCondition error) {  
  // restarting needs to be an exclusive operation
  Connection::State exp = Connection::State::Connected;
  if (_state.compare_exchange_strong(exp, Connection::State::Disconnected)) {
    FUERTE_LOG_CALLBACKS << "restartConnection\n";
    shutdownConnection(error); // Terminate connection
    startConnection(); // will check state
  }
}
  
// Thread-Safe: reset io loop flags
template<SocketType ST>
void VstConnection<ST>::stopIOLoops() {
  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  while (state & LOOP_FLAGS) {
    if (_loopState.compare_exchange_weak(state, state & ~LOOP_FLAGS,
                                         std::memory_order_seq_cst)) {
      FUERTE_LOG_VSTTRACE << "stopIOLoops: stopped\n";
      return;  // we turned flag off while nothin was queued
    }
    cpu_relax();
  }
}

// socket connection is up (with optional SSL), now initiate the VST protocol.
template<SocketType ST>
void VstConnection<ST>::finishInitialization() {
  FUERTE_LOG_CALLBACKS << "finishInitialization (vst)\n";
  const char* vstHeader;
  switch (_vstVersion) {
    case VST1_0:
      vstHeader = "VST/1.0\r\n\r\n";
      break;
    case VST1_1:
      vstHeader = "VST/1.1\r\n\r\n";
      break;
    default:
      throw std::logic_error("Unknown VST version");
  }

  auto self = shared_from_this();
  asio_ns::async_write(_protocol.socket,
      asio_ns::buffer(vstHeader, strlen(vstHeader)),
      [self, this](asio_ns::error_code const& ec, std::size_t transferred) {
        if (ec) {
          FUERTE_LOG_ERROR << ec.message() << "\n";
          shutdownConnection(ErrorCondition::CouldNotConnect);
          onFailure(errorToInt(ErrorCondition::CouldNotConnect),
                    "unable to initialize connection: error=" + ec.message());
          return;
        }
        FUERTE_LOG_CALLBACKS << "VST connection established\n";
        if (_config._authenticationType != AuthenticationType::None) {
          // send the auth, then set _state == connected
          sendAuthenticationRequest();
        } else {
          _state.store(State::Connected, std::memory_order_release);
          startWriting(); // start writing if something is queued
        }
      });
}

// Send out the authentication message on this connection
template<SocketType ST>
void VstConnection<ST>::sendAuthenticationRequest() {
  assert(_config._authenticationType != AuthenticationType::None);
  
  // Part 1: Build ArangoDB VST auth message (1000)
  auto item = std::make_shared<RequestItem>();
  item->_messageID = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  item->_expires = std::chrono::steady_clock::now() + Request::defaultTimeout;
  item->_request = nullptr; // should not break anything
  
  if (_config._authenticationType == AuthenticationType::Basic) {
    item->_requestMetadata = vst::message::authBasic(_config._user, _config._password);
  } else if (_config._authenticationType == AuthenticationType::Jwt) {
    item->_requestMetadata = vst::message::authJWT(_config._jwtToken);
  }
  assert(item->_requestMetadata.size() < defaultMaxChunkSize);
  asio_ns::const_buffer header(item->_requestMetadata.data(),
                               item->_requestMetadata.byteSize());

  item->prepareForNetwork(_vstVersion, header, asio_ns::const_buffer(0,0));

  auto self = shared_from_this();
  item->_callback = [self, this](Error error, std::unique_ptr<Request>,
                                 std::unique_ptr<Response> resp) {
    if (error || resp->statusCode() != StatusOK) {
      _state.store(State::Failed, std::memory_order_release);
      onFailure(error, "authentication failed");
    }
  };
  
  _messageStore.add(item); // add message to store
  setTimeout();                  // set request timeout
  
  // actually send auth request
  asio_ns::post(*_io_context, [this, self, item] {
    auto cb = [self, item, this](asio_ns::error_code const& ec,
                                 std::size_t transferred) {
      if (ec) {
        asyncWriteCallback(ec, transferred, std::move(item)); // error handling
        return;
      }
      _state.store(Connection::State::Connected, std::memory_order_release);
      asyncWriteCallback(ec, transferred, std::move(item)); // calls startReading()
      startWriting(); // start writing if something was queued
    };
    asio_ns::async_write(_protocol.socket, item->_requestBuffers, std::move(cb));
  });
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: activate the writer loop (if off and items are queud)
template<SocketType ST>
void VstConnection<ST>::startWriting() {
  assert(_state.load(std::memory_order_acquire) == State::Connected);
  FUERTE_LOG_VSTTRACE << "startWriting (vst): this=" << this << "\n";

  uint32_t state = _loopState.load(std::memory_order_acquire);
  // start the loop if necessary
  while (!(state & WRITE_LOOP_ACTIVE) && (state & WRITE_LOOP_QUEUE_MASK) > 0) {
    if (_loopState.compare_exchange_weak(state, state | WRITE_LOOP_ACTIVE,
                                               std::memory_order_seq_cst)) {
      FUERTE_LOG_VSTTRACE << "startWriting (vst): starting write\n";
      auto self = shared_from_this(); // only one thread can get here per connection
      asio_ns::post(*_io_context, [self, this] {
        asyncWriteNextRequest();
      });
      return;
    }
    cpu_relax();
  }
  /*if ((state & WRITE_LOOP_QUEUE_MASK) == 0) {
    FUERTE_LOG_VSTTRACE << "startWriting (vst): nothing is queued\n";
  }*/
}
  
// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void VstConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_VSTTRACE << "asyncWrite: preparing to send next\n";
  
  // reduce queue length and check active flag
#ifndef NDEBUG
  uint32_t state =
#endif
  _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_acquire);
  assert((state & WRITE_LOOP_QUEUE_MASK) > 0);
  
  RequestItem* ptr = nullptr;
#ifndef NDEBUG
  bool success =
#endif
  _writeQueue.pop(ptr);
  assert(success); // should never fail here
  std::shared_ptr<RequestItem> item(ptr);
  
  // set the point-in-time when this request expires
  if (item->_request && item->_request->timeout().count() > 0) {
    item->_expires = std::chrono::steady_clock::now() + item->_request->timeout();
  }
  
  _messageStore.add(item);  // Add item to message store
  startReading();           // Make sure we're listening for a response
  setTimeout();             // prepare request / connection timeouts
  
  auto self = shared_from_this();
  auto cb = [self, item, this](asio_ns::error_code const& ec, std::size_t transferred) {
    asyncWriteCallback(ec, transferred, std::move(item));
  };
  asio_ns::async_write(_protocol.socket, item->_requestBuffers, cb);
  FUERTE_LOG_VSTTRACE << "asyncWrite: done\n";
}

// callback of async_write function that is called in sendNextRequest.
template<SocketType ST>
void VstConnection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                           std::size_t transferred,
                                           std::shared_ptr<RequestItem> item) {

  // auto pendingAsyncCalls = --_connection->_async_calls;
  if (ec) {
    // Send failed
    FUERTE_LOG_CALLBACKS << "asyncWriteCallback (vst): error "
                         << ec.message() << "\n";

    // Item has failed, remove from message store
    _messageStore.removeByID(item->_messageID);

    auto err = checkEOFError(ec, ErrorCondition::WriteError);
    // let user know that this request caused the error
    item->_callback.invoke(errorToInt(err), std::move(item->_request), nullptr);
    // Stop current connection and try to restart a new one.
    restartConnection(err);
    return;
  }
  // Send succeeded
  FUERTE_LOG_CALLBACKS << "asyncWriteCallback (vst): send succeeded, "
                       << transferred << " bytes transferred\n";

  // request is written we no longer need data for that
  item->resetSendData();

  // check the queue length, stop write loop if necessary
  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  // nothing is queued, lets try to halt the write queue while
  // the write loop is active and nothing is queued
  while ((state & WRITE_LOOP_ACTIVE) && (state & WRITE_LOOP_QUEUE_MASK) == 0) {
    if (_loopState.compare_exchange_weak(state, state & ~WRITE_LOOP_ACTIVE)) {
      FUERTE_LOG_VSTTRACE << "asyncWrite: no more queued items\n";
      state = state & ~WRITE_LOOP_ACTIVE;
      break;  // we turned flag off while nothin was queued
    }
    cpu_relax();
  }

  if (!(state & READ_LOOP_ACTIVE)) {
    startReading();  // Make sure we're listening for a response
  }

  // Continue with next request (if any)
  FUERTE_LOG_CALLBACKS
      << "asyncWriteCallback (vst): send next request (if any)\n";

  if (state & WRITE_LOOP_ACTIVE) {
    asyncWriteNextRequest();  // continue writing
  }
}

// ------------------------------------
// Reading data
// ------------------------------------

// Thread-Safe: activate the read loop (if needed)
template<SocketType ST>
void VstConnection<ST>::startReading() {
  FUERTE_LOG_VSTTRACE << "startReading: this=" << this << "\n";

  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  // start the loop if necessary
  while (!(state & READ_LOOP_ACTIVE)) {
    if (_loopState.compare_exchange_weak(state, state | READ_LOOP_ACTIVE)) {
      auto self = shared_from_this(); // only one thread can get here per connection
      asio_ns::post(*_io_context, [self, this] {
        asyncReadSome();
      });
      return;
    }
    cpu_relax();
  }
  // There is already a read loop, do nothing
}

// Thread-Safe: Stop the read loop
template<SocketType ST>
void VstConnection<ST>::stopReading() {
  FUERTE_LOG_VSTTRACE << "stopReading: this=" << this << "\n";

  uint32_t state = _loopState.load(std::memory_order_relaxed);
  // start the loop if necessary
  while (state & READ_LOOP_ACTIVE) {
    if (_loopState.compare_exchange_weak(state, state & ~READ_LOOP_ACTIVE)) {
      return;
    }
  }
}
// asyncReadSome reads the next bytes from the server.
template<SocketType ST>
void VstConnection<ST>::asyncReadSome() {
  FUERTE_LOG_VSTTRACE << "asyncReadSome: this=" << this << "\n";
  
  assert((_loopState.load(std::memory_order_acquire) & READ_LOOP_ACTIVE));
  /*if (!(_loopState.load(std::memory_order_seq_cst) & READ_LOOP_ACTIVE)) {
    FUERTE_LOG_VSTTRACE << "asyncReadSome: read-loop was stopped\n";
    return;  // just stop
  }*/
  
  // Start reading data from the network.
  FUERTE_LOG_CALLBACKS << "r";
#if ENABLE_FUERTE_LOG_CALLBACKS > 0
  std::cout << "_messageMap = " << _messageStore.keys() << "\n";
#endif
  
  auto self = shared_from_this();
  auto cb = [self, this](asio_ns::error_code const& ec, size_t transferred) {
    // received data is "committed" from output sequence to input sequence
    _receiveBuffer.commit(transferred);
    asyncReadCallback(ec, transferred);
  };
  
  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  _protocol.socket.async_read_some(mutableBuff, std::move(cb));
  
  FUERTE_LOG_VSTTRACE << "asyncReadSome: done\n";
}

// asyncReadCallback is called when asyncReadSome is resulting in some data.
template<SocketType ST>
void VstConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec,
                                          std::size_t transferred) {

  if (ec) {
    FUERTE_LOG_CALLBACKS
    << "asyncReadCallback: Error while reading form socket: " << ec.message();
    restartConnection(checkEOFError(ec, ErrorCondition::ReadError));
    return;
  }
  
  FUERTE_LOG_CALLBACKS
      << "asyncReadCallback: received " << transferred << " bytes\n";

  // Inspect the data we've received so far.
  auto recvBuffs = _receiveBuffer.data();  // no copy
  auto cursor = asio_ns::buffer_cast<const uint8_t*>(recvBuffs);
  auto available = asio_ns::buffer_size(recvBuffs);
  // TODO technically buffer_cast is deprecated
  
  size_t parsedBytes = 0;
  while (vst::parser::isChunkComplete(cursor, available)) {
    // Read chunk
    ChunkHeader chunk;
    asio_ns::const_buffer buffer;
    switch (_vstVersion) {
      case VST1_0:
        std::tie(chunk, buffer) = vst::parser::readChunkHeaderVST1_0(cursor);
        break;
      case VST1_1:
        std::tie(chunk, buffer) = vst::parser::readChunkHeaderVST1_1(cursor);
        break;
      default:
        throw std::logic_error("Unknown VST version");
    }
    
    if (available < chunk.chunkLength()) { // prevent reading beyond buffer
      FUERTE_LOG_ERROR << "invalid chunk header";
      shutdownConnection(ErrorCondition::ProtocolError);
      return;
    }
    
    // move cursors
    cursor += chunk.chunkLength();
    available -= chunk.chunkLength();
    parsedBytes += chunk.chunkLength();

    // Process chunk
    processChunk(std::move(chunk), buffer);
  }
  
  // Remove consumed data from receive buffer.
  _receiveBuffer.consume(parsedBytes);

  // check for more messages that could arrive
  if (_messageStore.empty(true) &&
      !(_loopState.load(std::memory_order_acquire) & WRITE_LOOP_ACTIVE)) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read";
    stopReading();
    return;  // write-loop restarts read-loop if necessary
  }

  asyncReadSome();  // Continue read loop
}

// Process the given incoming chunk.
template<SocketType ST>
void VstConnection<ST>::processChunk(ChunkHeader&& chunk,
                                     asio_ns::const_buffer const& data) {
  auto msgID = chunk.messageID();
  FUERTE_LOG_VSTTRACE << "processChunk: messageID=" << msgID << "\n";

  // Find requestItem for this chunk.
  auto item = _messageStore.findByID(chunk._messageID);
  if (!item) {
    FUERTE_LOG_ERROR << "got chunk with unknown message ID: " << msgID
                     << "\n";
    return;
  }

  // We've found the matching RequestItem.
  item->addChunk(std::move(chunk), data);

  // Try to assembly chunks in RequestItem to complete response.
  auto completeBuffer = item->assemble();
  if (completeBuffer) {
    FUERTE_LOG_VSTTRACE << "processChunk: complete response received\n";
    _timeout.cancel();
    
    // Message is complete
    // Remove message from store
    _messageStore.removeByID(item->_messageID);

    // Create response
    auto response = createResponse(*item, completeBuffer);
    if (response == nullptr) {
      item->_callback.invoke(errorToInt(ErrorCondition::ProtocolError),
                             std::move(item->_request), nullptr);
      // Notify listeners
      FUERTE_LOG_VSTTRACE
      << "processChunk: notifying RequestItem error callback"
      << "\n";
      return;
    }

    // Notify listeners
    FUERTE_LOG_VSTTRACE
        << "processChunk: notifying RequestItem success callback"
        << "\n";
    item->_callback.invoke(0, std::move(item->_request), std::move(response));
    
    setTimeout();     // readjust timeout
  }
}

// Create a response object for given RequestItem & received response buffer.
template<SocketType ST>
std::unique_ptr<fu::Response> VstConnection<ST>::createResponse(
    RequestItem& item, std::unique_ptr<VPackBuffer<uint8_t>>& responseBuffer) {
  FUERTE_LOG_VSTTRACE << "creating response for item with messageid: "
  << item._messageID << "\n";
  auto itemCursor = responseBuffer->data();
  auto itemLength = responseBuffer->byteSize();
  
  // first part of the buffer contains the response buffer
  std::size_t headerLength;
  MessageType type = parser::validateAndExtractMessageType(itemCursor, itemLength, headerLength);
  if (type != MessageType::Response) {
    FUERTE_LOG_ERROR << "received unsupported vst message from server";
    return nullptr;
  }
  
  ResponseHeader header = parser::responseHeaderFromSlice(VPackSlice(itemCursor));
  auto response = std::unique_ptr<Response>(new Response(std::move(header)));
  response->setPayload(std::move(*responseBuffer), /*offset*/headerLength);

  return response;
}

// called when the connection expired
template<SocketType ST>
void VstConnection<ST>::setTimeout() {
  // set to smallest point in time
  auto expires = std::chrono::steady_clock::time_point::max();
  size_t waiting = _messageStore.invokeOnAll([&](RequestItem* item) {
    if (expires > item->_expires) {
      expires = item->_expires;
    }
    return true;
  });

  if (waiting == 0) {
    _timeout.cancel();
    return;
  } else if (expires == std::chrono::steady_clock::time_point::max()) {
    // use default connection timeout
    expires = std::chrono::steady_clock::now() + Request::defaultTimeout;
  }
  
  _timeout.expires_at(expires);
  std::weak_ptr<Connection> self = shared_from_this();
  _timeout.async_wait([self, this](asio_ns::error_code const& ec) {
    if (ec) {  // was canceled
      return;
    }
    auto s = self.lock();
    if (!s) {
      return;
    }
      
    // cancel expired requests
    auto now = std::chrono::steady_clock::now();
    size_t waiting =
    _messageStore.invokeOnAll([&](RequestItem* item) {
      if (item->_expires < now) {
        FUERTE_LOG_DEBUG << "VST-Request timeout\n";
        item->invokeOnError(errorToInt(ErrorCondition::Timeout));
        return false;
      }
      return true;
    });
    if (waiting == 0) { // no more messages to wait on
      FUERTE_LOG_DEBUG << "VST-Connection timeout\n";
      restartConnection(ErrorCondition::Timeout);
    } else {
      setTimeout();
    }
  });
}

template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
