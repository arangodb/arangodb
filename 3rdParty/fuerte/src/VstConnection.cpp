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

template <SocketType ST>
VstConnection<ST>::VstConnection(
    EventLoopService& loop, fu::detail::ConnectionConfiguration const& config)
    : fuerte::GeneralConnection<ST>(loop, config),
      _writeQueue(),
      _vstVersion(config._vstVersion),
      _reading(false),
      _writing(false) {}

template <SocketType ST>
VstConnection<ST>::~VstConnection() try {
  this->shutdownConnection(Error::Canceled);
  drainQueue(Error::Canceled);
} catch(...) {}

static std::atomic<MessageID> vstMessageId(1);
// sendRequest prepares a RequestItem for the given parameters
// and adds it to the send queue.
template <SocketType ST>
MessageID VstConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                         RequestCallback cb) {
  // it does not matter if IDs are reused on different connections
  uint64_t mid = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  // Create RequestItem from parameters
  auto item = std::make_unique<RequestItem>();
  item->_messageID = mid;
  item->_request = std::move(req);
  item->_callback = cb;
  item->_expires = std::chrono::steady_clock::time_point::max();

  // Add item to send queue
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    throw std::length_error("connection queue capacity exceeded");
  }
  item.release();  // queue owns this now
  
  this->_numQueued.fetch_add(1, std::memory_order_relaxed);
  FUERTE_LOG_VSTTRACE << "queued item: this=" << this << "\n";
  
  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = this->_state.load(std::memory_order_acquire);
  if (state == Connection::State::Connected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): start sending & reading\n";
    startWriting();  // try to start write loop

  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): not connected\n";
    this->startConnection();
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
    drainQueue(fuerte::Error::ConnectionClosed);
  }
  return mid;
}

template <SocketType ST>
std::size_t VstConnection<ST>::requestsLeft() const {
  uint32_t qd = this->_numQueued.load(std::memory_order_relaxed);
  
  if (_reading.load() || _writing.load()) {
    qd++;
  }
  return qd;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// socket connection is up (with optional SSL), now initiate the VST protocol.
template <SocketType ST>
void VstConnection<ST>::finishConnect() {
  FUERTE_LOG_VSTTRACE << "finishInitialization (vst)\n";
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
  asio_ns::async_write(
      this->_proto->socket, asio_ns::buffer(vstHeader, strlen(vstHeader)),
      [self](asio_ns::error_code const& ec, std::size_t nsend) {
        auto* thisPtr = static_cast<VstConnection<ST>*>(self.get());
        if (ec) {
          FUERTE_LOG_ERROR << ec.message() << "\n";
          thisPtr->shutdownConnection(Error::CouldNotConnect,
                                      "unable to connect: " + ec.message());
          thisPtr->drainQueue(Error::CouldNotConnect);
          return;
        }
        FUERTE_LOG_VSTTRACE << "VST connection established\n";
        if (thisPtr->_config._authenticationType != AuthenticationType::None) {
          // send the auth, then set _state == connected
          thisPtr->sendAuthenticationRequest();
        } else {
          thisPtr->_state.store(Connection::State::Connected,
                                std::memory_order_release);
          thisPtr->startWriting();  // start writing if something is queued
        }
      });
}

// Send out the authentication message on this connection
template <SocketType ST>
void VstConnection<ST>::sendAuthenticationRequest() {
  assert(this->_config._authenticationType != AuthenticationType::None);

  // Part 1: Build ArangoDB VST auth message (1000)
  auto item = std::make_shared<RequestItem>();
  item->_messageID = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  item->_expires = std::chrono::steady_clock::now() + Request::defaultTimeout;
  auto self = Connection::shared_from_this();
  item->_callback = [self](Error error, std::unique_ptr<Request>,
                           std::unique_ptr<Response> resp) {
    auto* thisPtr = static_cast<VstConnection<ST>*>(self.get());
    if (error != Error::NoError || resp->statusCode() != StatusOK) {
      thisPtr->_state.store(Connection::State::Failed,
                            std::memory_order_release);
      thisPtr->shutdownConnection(Error::VstUnauthorized,
                                  "could not authenticate");
      thisPtr->drainQueue(Error::VstUnauthorized);
    } else {
      thisPtr->_state.store(Connection::State::Connected);
      thisPtr->startWriting();
    }
  };

  _messageStore.add(item);  // add message to store

  if (this->_config._authenticationType == AuthenticationType::Basic) {
    vst::message::authBasic(this->_config._user, this->_config._password,
                            item->_buffer);
  } else if (this->_config._authenticationType == AuthenticationType::Jwt) {
    vst::message::authJWT(this->_config._jwtToken, item->_buffer);
  }
  assert(item->_buffer.size() < defaultMaxChunkSize);

  // actually send auth request
  auto cb = [this, self, item](asio_ns::error_code const& ec,
                               std::size_t nsend) {
    if (ec) {
      this->_state.store(Connection::State::Failed);
      this-> shutdownConnection(Error::CouldNotConnect,
                                "authorization message failed");
      this->drainQueue(Error::CouldNotConnect);
    } else {
      asyncWriteCallback(ec, item, nsend);
    }
  };
  std::vector<asio_ns::const_buffer> buffers;
  vst::message::prepareForNetwork(_vstVersion, item->messageID(), item->_buffer,
                                  /*payload*/ asio_ns::const_buffer(), buffers);
  asio_ns::async_write(this->_proto->socket, buffers, std::move(cb));
  
  setTimeout();
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: activate the writer loop (if off and items are queud)
template <SocketType ST>
void VstConnection<ST>::startWriting() {
  assert(this->_state.load(std::memory_order_acquire) ==
         Connection::State::Connected);
  FUERTE_LOG_VSTTRACE << "startWriting: this=" << this << "\n";
  
  if (_writing.load() || _writing.exchange(true)) {
    return;  // There is already a write loop, do nothing
  }
  
  // we are the only ones here now

  FUERTE_LOG_HTTPTRACE << "startWriting: active=true, this=" << this << "\n";
  
  asio_ns::post(*this->_io_context,
                [self = Connection::shared_from_this(), this] {
    
    // we have been in a race with shutdownConnection()
    Connection::State state = this->_state.load();
    if (state != Connection::State::Connected) {
      this->_writing.store(false);
      if (state == Connection::State::Disconnected) {
        this->startConnection();
      }
    } else {
      this->asyncWriteNextRequest();
    }
  });
}

// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void VstConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_VSTTRACE << "asyncWrite: preparing to send next\n";
  
   while(true) { // loop instead of recursion
    
      RequestItem* ptr = nullptr;
      if (!_writeQueue.pop(ptr)) {
        
        FUERTE_LOG_VSTTRACE
            << "asyncWriteNextRequest (vst): write queue empty\n";
              
        // careful now, we need to consider that someone queues
        // a new request item
        _writing.store(false);
        if (_writeQueue.empty()) {
          FUERTE_LOG_VSTTRACE
              << "asyncWriteNextRequest (vst): write stopped\n";
          break; // done, someone else may restart
        }

        bool expected = false; // may fail in a race
        if (_writing.compare_exchange_strong(expected, true)) {
          continue; // we re-start writing
        }
        assert(expected == true);
        break; // someone else restarted writing
      }
     
     this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    
    std::shared_ptr<RequestItem> item(ptr);

    // set the point-in-time when this request expires
    if (item->_request && item->_request->timeout().count() > 0) {
      item->_expires =
          std::chrono::steady_clock::now() + item->_request->timeout();
    }
     
    _messageStore.add(item);  // Add item to message store
    setTimeout();             // prepare request / connection timeouts

    asio_ns::async_write(this->_proto->socket, item->prepareForNetwork(_vstVersion),
                        [self = Connection::shared_from_this(), req(std::move(item))]
                        (asio_ns::error_code const& ec, std::size_t nwrite) mutable {
     auto& thisPtr = static_cast<VstConnection<ST>&>(*self);
     thisPtr.asyncWriteCallback(ec, std::move(req), nwrite);
    });

    break; // done
  }
  
  FUERTE_LOG_VSTTRACE << "asyncWrite: done\n";
}

// callback of async_write function that is called in sendNextRequest.
template <SocketType ST>
void VstConnection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                           std::shared_ptr<RequestItem> item,
                                           std::size_t nwrite) {
  // auto pendingAsyncCalls = --_connection->_async_calls;
  if (ec) {
    // Send failed
    FUERTE_LOG_VSTTRACE << "asyncWriteCallback: error " << ec.message() << "\n";

    // Item has failed, remove from message store
    _messageStore.removeByID(item->_messageID);
    
    auto err = translateError(ec, Error::WriteError);
    try {
      // let user know that this request caused the error
      item->_callback(err, std::move(item->_request), nullptr);
    } catch(...) {}
    // Stop current connection and try to restart a new one.
    this->restartConnection(err);
    return;
  }
  // Send succeeded
  FUERTE_LOG_VSTTRACE << "asyncWriteCallback: send succeeded, "
                       << nwrite << " bytes send\n";

  // request is written we no longer need data for that
  item->resetSendData();

  startReading();  // Make sure we're listening for a response

  // Continue with next request (if any)
  asyncWriteNextRequest();  // continue writing
}

// ------------------------------------
// Reading data
// ------------------------------------

// Thread-Safe: activate the read loop (if needed)
template <SocketType ST>
void VstConnection<ST>::startReading() {
  FUERTE_LOG_VSTTRACE << "startReading: this=" << this << "\n";
  
  if (_reading.load() || _reading.exchange(true)) {
    return; // There is already a read loop, do nothing
  }
  
  FUERTE_LOG_VSTTRACE << "startReading: active=true, this=" << this << "\n";
  this->asyncReadSome();
}

// asyncReadCallback is called when asyncReadSome is resulting in some data.
template <SocketType ST>
void VstConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  if (ec) {
    FUERTE_LOG_VSTTRACE
        << "asyncReadCallback: Error while reading form socket: "
        << ec.message();
    this->restartConnection(translateError(ec, Error::ReadError));
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
    parser::ChunkState state = parser::ChunkState::Invalid;
    if (_vstVersion == VST1_1) {
      state = vst::parser::readChunkVST1_1(chunk, cursor, available);
    } else if (_vstVersion == VST1_0) {
      state = vst::parser::readChunkVST1_0(chunk, cursor, available);
    }

    if (parser::ChunkState::Incomplete == state) {
      break;
    } else if (parser::ChunkState::Invalid == state) {
      this->shutdownConnection(Error::ProtocolError,
                               "Invalid VST chunk");
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
  if (_messageStore.empty()/* && !_writing.load()*/) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read";
    _reading.store(false);
    return;  // write-loop restarts read-loop if necessary
  }

  assert(_reading.load());
  this->asyncReadSome();  // Continue read loop
}

// Process the given incoming chunk.
template <SocketType ST>
void VstConnection<ST>::processChunk(Chunk const& chunk) {
  auto msgID = chunk.header.messageID();
  FUERTE_LOG_VSTTRACE << "processChunk: messageID=" << msgID << "\n";

  // Find requestItem for this chunk.
  auto item = _messageStore.findByID(chunk.header.messageID());
  if (!item) {
    FUERTE_LOG_ERROR << "got chunk with unknown message ID: " << msgID << "\n";
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

    try {
      // Create response
      auto resp = createResponse(*item, completeBuffer);
      auto err = resp != nullptr ? Error::NoError : Error::ProtocolError;
      item->_callback(err, std::move(item->_request), std::move(resp));
    } catch(...) {
      FUERTE_LOG_ERROR << "unhandled exception in fuerte callback\n";
    }

    setTimeout();  // readjust timeout
  }
}

// Create a response object for given RequestItem & received response buffer.
template <SocketType ST>
std::unique_ptr<fu::Response> VstConnection<ST>::createResponse(
    RequestItem& item, std::unique_ptr<VPackBuffer<uint8_t>>& responseBuffer) {
  FUERTE_LOG_VSTTRACE << "creating response for item with messageid: "
                      << item._messageID << "\n";
  auto itemCursor = responseBuffer->data();
  auto itemLength = responseBuffer->byteSize();

  // first part of the buffer contains the response buffer
  std::size_t headerLength;
  MessageType type = parser::validateAndExtractMessageType(
      itemCursor, itemLength, headerLength);
  if (type != MessageType::Response) {
    FUERTE_LOG_ERROR << "received unsupported vst message from server";
    return nullptr;
  }

  ResponseHeader header =
      parser::responseHeaderFromSlice(VPackSlice(itemCursor));
  auto response = std::make_unique<Response>(std::move(header));
  response->setPayload(std::move(*responseBuffer), /*offset*/ headerLength);

  return response;
}

// adjust the timeouts (only call from IO-Thread)
template <SocketType ST>
void VstConnection<ST>::setTimeout() {

  // set to smallest point in time
  auto expires = std::chrono::steady_clock::time_point::max();
  size_t waiting = _messageStore.invokeOnAll([&](RequestItem* item) {
    if (expires > item->_expires) {
      expires = item->_expires;
    }
    return true;
  });

  if (waiting == 0) {  // use default connection timeout
    expires = std::chrono::steady_clock::now() + this->_config._idleTimeout;
  }

  this->_timeout.expires_at(expires);
  this->_timeout.async_wait([self = Connection::weak_from_this()](asio_ns::error_code const& ec) {
    std::shared_ptr<Connection> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    auto* thisPtr = static_cast<VstConnection<ST>*>(s.get());

    // cancel expired requests
    auto now = std::chrono::steady_clock::now();
    size_t waiting = thisPtr->_messageStore.invokeOnAll([&](RequestItem* item) {
      if (item->_expires < now) {
        FUERTE_LOG_DEBUG << "VST-Request timeout\n";
        item->invokeOnError(Error::Timeout);
        return false;  // remove
      }
      return true;
    });
    if (waiting == 0) {  // no more messages to wait on
      FUERTE_LOG_DEBUG << "VST-Connection timeout\n";
      thisPtr->shutdownConnection(Error::Timeout);
    } else {
      thisPtr->setTimeout();
    }
  });
}

/// abort ongoing / unfinished requests
template <SocketType ST>
void VstConnection<ST>::abortOngoingRequests(const fuerte::Error err) {
  FUERTE_LOG_VSTTRACE << "aborting ongoing requests";
  // Reset the read & write loop
  // Cancel all items and remove them from the message store.
  if (err != Error::VstUnauthorized) { // prevents stack overflow
    _messageStore.cancelAll(err);
  }
  _reading.store(false);
  _writing.store(false);
}

/// abort all requests lingering in the queue
template <SocketType ST>
void VstConnection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_writeQueue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    guard->invokeOnError(ec);
  }
}

template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
