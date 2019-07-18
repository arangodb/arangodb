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
  : fuerte::GeneralConnection<ST>(loop, config),
      _writeQueue(),
      _vstVersion(config._vstVersion),
      _loopState(0) {}

template<SocketType ST>
VstConnection<ST>::~VstConnection() {
  this->shutdownConnection(Error::Canceled);
  drainQueue(Error::Canceled);
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
  
  
  const size_t payloadSize = item->_request->payloadSize();
  
  // Add item to send queue
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    throw std::length_error("connection queue capacity exceeded");
  }
  item.release(); // queue owns this now
  
  this->_bytesToSend.fetch_add(payloadSize, std::memory_order_relaxed);
  
  FUERTE_LOG_VSTTRACE << "queued item: this=" << this << "\n";
  
  // WRITE_LOOP_ACTIVE, READ_LOOP_ACTIVE are synchronized via cmpxchg
  uint32_t loop = _loopState.fetch_add(WRITE_LOOP_QUEUE_INC, std::memory_order_seq_cst);

  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = this->_state.load(std::memory_order_acquire);
  if (state == Connection::State::Connected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): start sending & reading\n";
    if (!(loop & WRITE_LOOP_ACTIVE)) {
      startWriting(); // try to start write loop
    }
  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): not connected\n";
    this->startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
  }
  return mid;
}
  
// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// socket connection is up (with optional SSL), now initiate the VST protocol.
template<SocketType ST>
void VstConnection<ST>::finishConnect() {
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

  auto self = Connection::shared_from_this();
  asio_ns::async_write(this->_protocol.socket,
      asio_ns::buffer(vstHeader, strlen(vstHeader)),
      [self](asio_ns::error_code const& ec, std::size_t transferred) {
        auto* thisPtr = static_cast<VstConnection<ST>*>(self.get());
        if (ec) {
          FUERTE_LOG_ERROR << ec.message() << "\n";
          thisPtr->shutdownConnection(Error::CouldNotConnect);
          thisPtr->drainQueue(Error::CouldNotConnect);
          thisPtr->onFailure(Error::CouldNotConnect,
                           "unable to initialize connection: error=" + ec.message());
          return;
        }
        FUERTE_LOG_CALLBACKS << "VST connection established\n";
        if (thisPtr->_config._authenticationType != AuthenticationType::None) {
          // send the auth, then set _state == connected
          thisPtr->sendAuthenticationRequest();
        } else {
          thisPtr->_state.store(Connection::State::Connected, std::memory_order_release);
          thisPtr->startWriting(); // start writing if something is queued
        }
      });
}

// Send out the authentication message on this connection
template<SocketType ST>
void VstConnection<ST>::sendAuthenticationRequest() {
  assert(this->_config._authenticationType != AuthenticationType::None);
  
  // Part 1: Build ArangoDB VST auth message (1000)
  auto item = std::make_shared<RequestItem>();
  item->_messageID = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  item->_expires = std::chrono::steady_clock::now() + Request::defaultTimeout;
  item->_request = nullptr; // should not break anything
  
  auto self = Connection::shared_from_this();
  item->_callback = [self](Error error, std::unique_ptr<Request>,
                           std::unique_ptr<Response> resp) {
    if (error != Error::NoError || resp->statusCode() != StatusOK) {
      auto* thisPtr = static_cast<VstConnection<ST>*>(self.get());
      thisPtr->_state.store(Connection::State::Failed, std::memory_order_release);
      thisPtr->shutdownConnection(Error::CouldNotConnect);
      thisPtr->onFailure(error, "authentication failed");
    }
  };
  
  _messageStore.add(item); // add message to store
  setTimeout();            // set request timeout
  
  if (this->_config._authenticationType == AuthenticationType::Basic) {
    vst::message::authBasic(this->_config._user, this->_config._password, item->_buffer);
  } else if (this->_config._authenticationType == AuthenticationType::Jwt) {
    vst::message::authJWT(this->_config._jwtToken, item->_buffer);
  }
  assert(item->_buffer.size() < defaultMaxChunkSize);
  
  // actually send auth request
  asio_ns::post(*this->_io_context, [this, self, item] {
    auto cb = [self, item, this](asio_ns::error_code const& ec,
                                 std::size_t transferred) {
      if (ec) {
        asyncWriteCallback(ec, transferred, std::move(item)); // error handling
        return;
      }
      this->_state.store(Connection::State::Connected, std::memory_order_release);
      asyncWriteCallback(ec, transferred, std::move(item)); // calls startReading()
      startWriting(); // start writing if something was queued
    };
    std::vector<asio_ns::const_buffer> buffers;
    vst::message::prepareForNetwork(_vstVersion, item->messageID(), item->_buffer,
                                    /*payload*/asio_ns::const_buffer(), buffers);
    asio_ns::async_write(this->_protocol.socket, buffers, std::move(cb));
  });
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: activate the writer loop (if off and items are queud)
template<SocketType ST>
void VstConnection<ST>::startWriting() {
  assert(this->_state.load(std::memory_order_acquire) == Connection::State::Connected);
  FUERTE_LOG_VSTTRACE << "startWriting (vst): this=" << this << "\n";

  uint32_t state = _loopState.load(std::memory_order_acquire);
  // start the loop if necessary
  while (!(state & WRITE_LOOP_ACTIVE) && (state & WRITE_LOOP_QUEUE_MASK) > 0) {
    if (_loopState.compare_exchange_weak(state, state | WRITE_LOOP_ACTIVE,
                                               std::memory_order_seq_cst)) {
      FUERTE_LOG_VSTTRACE << "startWriting (vst): starting write\n";
      auto self = Connection::shared_from_this(); // only one thread can get here per connection
      asio_ns::post(*this->_io_context, [self, this] {
        asyncWriteNextRequest();
      });
      return;
    }
    cpu_relax();
  }
}
  
// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void VstConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_VSTTRACE << "asyncWrite: preparing to send next\n";
  
  // reduce queue length and check active flag
#ifdef FUERTE_DEBUG
  uint32_t state =
#endif
  _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_acquire);
  assert((state & WRITE_LOOP_QUEUE_MASK) > 0);
  
  RequestItem* ptr = nullptr;
#ifdef FUERTE_DEBUG
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
  
  auto self = Connection::shared_from_this();
  auto cb = [self, item, this](asio_ns::error_code const& ec, std::size_t transferred) {
    this->_bytesToSend.fetch_sub(item->_request->payloadSize(), std::memory_order_relaxed);
    asyncWriteCallback(ec, transferred, std::move(item));
  };
  std::vector<asio_ns::const_buffer> buffers = item->prepareForNetwork(_vstVersion);
  asio_ns::async_write(this->_protocol.socket, buffers, cb);
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

    auto err = checkEOFError(ec, Error::WriteError);
    // let user know that this request caused the error
    item->_callback(err, std::move(item->_request), nullptr);
    // Stop current connection and try to restart a new one.
    this->restartConnection(err);
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
      // only one thread can get here per connection
      auto self = Connection::shared_from_this();
      asio_ns::post(*this->_io_context, [self, this] {
        assert((_loopState.load(std::memory_order_acquire) & READ_LOOP_ACTIVE));
        this->asyncReadSome();
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

// asyncReadCallback is called when asyncReadSome is resulting in some data.
template<SocketType ST>
void VstConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  if (ec) {
    FUERTE_LOG_CALLBACKS
    << "asyncReadCallback: Error while reading form socket: " << ec.message();
    this->restartConnection(checkEOFError(ec, Error::ReadError));
    return;
  }

  // Inspect the data we've received so far.
  auto recvBuffs = this->_receiveBuffer.data();  // no copy
  auto cursor = asio_ns::buffer_cast<const uint8_t*>(recvBuffs);
  auto available = asio_ns::buffer_size(recvBuffs);
  // TODO technically buffer_cast is deprecated
  
  size_t parsedBytes = 0;
  while (true) {
    
    Chunk chunk;
    if (_vstVersion == VST1_1) {
      if (!vst::parser::readChunkHeaderVST1_1(chunk, cursor, available)) {
        break;
      }
    } else if (_vstVersion == VST1_0) {
      if (!vst::parser::readChunkHeaderVST1_0(chunk, cursor, available)) {
        break;
      }
    } else { // actually should never happen
      FUERTE_LOG_ERROR << "Unknown VST version";
      this->shutdownConnection(Error::ProtocolError);
      return;
    }
    
    // move cursors
    cursor += chunk.header.chunkLength();
    available -= chunk.header.chunkLength();
    parsedBytes += chunk.header.chunkLength();
    
    // Process chunk
    processChunk(chunk);
  }

  // Remove consumed data from receive buffer.
  this->_receiveBuffer.consume(parsedBytes);

  // check for more messages that could arrive
  if (_messageStore.empty() &&
      !(_loopState.load(std::memory_order_acquire) & WRITE_LOOP_ACTIVE)) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read";
    stopReading();
    return;  // write-loop restarts read-loop if necessary
  }

  assert((_loopState.load(std::memory_order_acquire) & READ_LOOP_ACTIVE));
  this->asyncReadSome();  // Continue read loop
}

// Process the given incoming chunk.
template<SocketType ST>
void VstConnection<ST>::processChunk(Chunk const& chunk) {
  auto msgID = chunk.header.messageID();
  FUERTE_LOG_VSTTRACE << "processChunk: messageID=" << msgID << "\n";

  // Find requestItem for this chunk.
  auto item = _messageStore.findByID(chunk.header.messageID());
  if (!item) {
    FUERTE_LOG_ERROR << "got chunk with unknown message ID: " << msgID
                     << "\n";
    return;
  }

  // We've found the matching RequestItem.
  item->addChunk(chunk);

  // Try to assembly chunks in RequestItem to complete response.
  auto completeBuffer = item->assemble();
  if (completeBuffer) {
    FUERTE_LOG_VSTTRACE << "processChunk: complete response received\n";
    this->_timeout.cancel();
    
    // Message is complete
    // Remove message from store
    _messageStore.removeByID(item->_messageID);

    // Create response
    auto response = createResponse(*item, completeBuffer);
    if (response == nullptr) {
      item->_callback(Error::ProtocolError,
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
    item->_callback(Error::NoError,
                    std::move(item->_request),
                    std::move(response));
    
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
  std::unique_ptr<Response> response(new Response(std::move(header)));
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

  if (waiting == 0) { // use default connection timeout
    expires = std::chrono::steady_clock::now() + this->_config._idleTimeout;
  }
  
  this->_timeout.expires_at(expires);
  std::weak_ptr<Connection> self = Connection::shared_from_this();
  this->_timeout.async_wait([self](asio_ns::error_code const& ec) {
    std::shared_ptr<Connection> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    auto* thisPtr = static_cast<VstConnection<ST>*>(s.get());
      
    // cancel expired requests
    auto now = std::chrono::steady_clock::now();
    size_t waiting =
    thisPtr->_messageStore.invokeOnAll([&](RequestItem* item) {
      if (item->_expires < now) {
        FUERTE_LOG_DEBUG << "VST-Request timeout\n";
        item->invokeOnError(Error::Timeout);
        return false; // remove
      }
      return true;
    });
    if (waiting == 0) { // no more messages to wait on
      FUERTE_LOG_DEBUG << "VST-Connection timeout\n";
      thisPtr->shutdownConnection(Error::Timeout);
    } else {
      thisPtr->setTimeout();
    }
  });
}
  
/// abort ongoing / unfinished requests
template<SocketType ST>
void VstConnection<ST>::abortOngoingRequests(const fuerte::Error ec) {
  // Reset the read & write loop
  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  while (state & LOOP_FLAGS) {
    if (_loopState.compare_exchange_weak(state, state & ~LOOP_FLAGS,
                                         std::memory_order_seq_cst)) {
      FUERTE_LOG_VSTTRACE << "stopIOLoops: stopped\n";
      return;  // we turned flag off while nothin was queued
    }
    cpu_relax();
  }
  
  // Cancel all items and remove them from the message store.
  _messageStore.cancelAll(ec);
}

/// abort all requests lingering in the queue
template<SocketType ST>
void VstConnection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_writeQueue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_release);
    this->_bytesToSend.fetch_sub(item->_request->payloadSize(), std::memory_order_relaxed);
    guard->invokeOnError(ec);
  }
}
  
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
