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

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>
#include <fuerte/types.h>
#include <velocypack/velocypack-aliases.h>

#include "debugging.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {
namespace fu = arangodb::fuerte::v1;
using arangodb::fuerte::v1::SocketType;

template <SocketType ST>
VstConnection<ST>::VstConnection(
    EventLoopService& loop, fu::detail::ConnectionConfiguration const& config)
    : fuerte::GeneralConnection<ST, vst::RequestItem>(loop, config),
      _numMessages(0),
      _vstVersion(config._vstVersion) {}

template <SocketType ST>
VstConnection<ST>::~VstConnection() try {
  abortOngoingRequests(Error::Canceled);
} catch (...) {
}

template <SocketType ST>
std::size_t VstConnection<ST>::requestsLeft() const {
  uint32_t qd = this->_numQueued.load(std::memory_order_relaxed);
  qd += _numMessages.load(std::memory_order_relaxed);
  FUERTE_LOG_VSTTRACE << "requestsLeft = " << qd << "\n";
  return qd;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// socket connection is up (with optional SSL), now initiate the VST protocol.
template <SocketType ST>
void VstConnection<ST>::finishConnect() {
  if (this->state() != Connection::State::Connecting) {
    return;
  }
  
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
      this->_proto.socket, asio_ns::buffer(vstHeader, strlen(vstHeader)),
      [self](asio_ns::error_code const& ec, std::size_t nsend) {
        auto* me = static_cast<VstConnection<ST>*>(self.get());
        if (ec || !me->_active.load()) {
          FUERTE_LOG_ERROR << ec.message() << "\n";
          me->shutdownConnection(Error::CouldNotConnect,
                                 "unable to connect: " + ec.message());
          return;
        }
        FUERTE_LOG_VSTTRACE << "VST connection established\n";
        if (me->_config._authenticationType != AuthenticationType::None) {
          // send the auth, then set _state == connected
          me->sendAuthenticationRequest();
        } else {
          me->_state.store(Connection::State::Connected);
          me->asyncWriteNextRequest();
        }
      });
}

// Send out the authentication message on this connection
template <SocketType ST>
void VstConnection<ST>::sendAuthenticationRequest() {
  FUERTE_ASSERT(this->_config._authenticationType != AuthenticationType::None);
  FUERTE_ASSERT(this->_active.load());

  // Part 1: Build ArangoDB VST auth message (1000)
  auto self = Connection::shared_from_this();
  auto item = std::make_shared<RequestItem>(nullptr,
                                            [self](Error error, std::unique_ptr<Request>,
                                                   std::unique_ptr<Response> resp) {
    auto& me = static_cast<VstConnection<ST>&>(*self);
    if (error != Error::NoError || resp->statusCode() != StatusOK) {
      me.shutdownConnection(Error::VstUnauthorized, "could not authenticate");
    } else {
      me._state.store(Connection::State::Connected);
      if (!me._writing) {
        me.asyncWriteNextRequest();
      }
    }
  });
  item->expires = std::chrono::steady_clock::now() + Request::defaultTimeout;

  _messages.emplace(item->_messageID, item);  // add message to store
  _numMessages.fetch_add(1, std::memory_order_relaxed);

  if (this->_config._authenticationType == AuthenticationType::Basic) {
    vst::message::authBasic(this->_config._user, this->_config._password,
                            item->_buffer);
  } else if (this->_config._authenticationType == AuthenticationType::Jwt) {
    vst::message::authJWT(this->_config._jwtToken, item->_buffer);
  }
  FUERTE_ASSERT(item->_buffer.size() < defaultMaxChunkSize);

  // actually send auth request
  std::vector<asio_ns::const_buffer> buffers;
  vst::message::prepareForNetwork(_vstVersion, item->_messageID, item->_buffer,
                                  /*payload*/ asio_ns::const_buffer(), buffers);
  
  this->_writing = true;
  this->setIOTimeout(); // has to be after setting _writing
  asio_ns::async_write(this->_proto.socket, buffers, [this, self, item](asio_ns::error_code const& ec,
                               std::size_t nsend) {
    if (ec) {
      this->shutdownConnection(Error::CouldNotConnect,
                               "authorization message failed");
    } else {
      asyncWriteCallback(ec, item, nsend);
    }
  });
}

// ------------------------------------
// Writing data
// ------------------------------------

// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void VstConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_VSTTRACE << "asyncWrite: preparing to send next\n";
  
  auto state = this->_state.load();
  FUERTE_ASSERT(state == Connection::State::Connected ||
                state == Connection::State::Closed);

  RequestItem* ptr = nullptr;
  if (!this->_queue.pop(ptr)) {
    FUERTE_LOG_VSTTRACE << "asyncWriteNextRequest (vst): write queue empty\n";

    // careful now, we need to consider that someone queues
    // a new request item
    this->_active.store(false);
    if (this->_queue.empty()) {
      FUERTE_LOG_VSTTRACE << "asyncWriteNextRequest (vst): write stopped\n";
      FUERTE_ASSERT(!this->_writing);
      return;  // done, someone else may restart
    }

    if (this->_active.exchange(true)) {
      return;  // someone else restarted writing
    }
    bool success = this->_queue.pop(ptr);
    FUERTE_ASSERT(success);
  }
  std::shared_ptr<RequestItem> item(ptr);
  FUERTE_ASSERT(ptr != nullptr);
  
  uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
  FUERTE_ASSERT(q > 0);

  _messages.emplace(item->_messageID, item);  // add message to store
  _numMessages.fetch_add(1, std::memory_order_relaxed);

  auto buffers = item->prepareForNetwork(_vstVersion);

  this->_writing = true;
  this->setIOTimeout();  // prepare request / connection timeouts
  asio_ns::async_write(
      this->_proto.socket, std::move(buffers),
      [self(Connection::shared_from_this()), req(std::move(item))](
          auto const& ec, std::size_t nwrite) mutable {
        auto& me = static_cast<VstConnection<ST>&>(*self);
        me._writing = false;
        me.asyncWriteCallback(ec, std::move(req), nwrite);
      });

  FUERTE_LOG_VSTTRACE << "asyncWrite: done\n";
}

// callback of async_write function that is called in sendNextRequest.
template <SocketType ST>
void VstConnection<ST>::asyncWriteCallback(asio_ns::error_code const& ec,
                                           std::shared_ptr<RequestItem> item,
                                           std::size_t nwrite) {
  FUERTE_ASSERT(item != nullptr);

  // auto pendingAsyncCalls = --_connection->_async_calls;
  if (ec) {
    // Send failed
    FUERTE_LOG_VSTTRACE << "asyncWriteCallback: error " << ec.message() << "\n";

    // Item has failed, remove from message store
    _messages.erase(item->_messageID);
    uint32_t q = _numMessages.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);

    auto err = translateError(ec, Error::WriteError);
    try {
      // let user know that this request caused the error
      item->_callback(err, std::move(item->request), nullptr);
    } catch (...) {
    }
    
    // Stop current connection and try to restart a new one.
    this->shutdownConnection(err, "write error");
    return;
  }
  // Send succeeded
  FUERTE_LOG_VSTTRACE << "asyncWriteCallback: send succeeded, " << nwrite
                      << " bytes send\n";

  // request is written we no longer need data for that
  item->resetSendData();

  if (!(this->_reading)) {
    this->asyncReadSome();
  }

  // Continue with next request (if any)
  asyncWriteNextRequest();  // continue writing
}

// ------------------------------------
// Reading data
// ------------------------------------

// asyncReadCallback is called when asyncReadSome is resulting in some data.
template <SocketType ST>
void VstConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  if (ec) {
    FUERTE_LOG_VSTTRACE
        << "asyncReadCallback: Error while reading from socket: "
        << ec.message();
    this->shutdownConnection(translateError(ec, Error::ReadError));
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
      this->shutdownConnection(Error::ProtocolError, "Invalid VST chunk");
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
  if (_messages.empty() && !(this->_writing)) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read\n";
    FUERTE_ASSERT(!this->_reading);
    setIOTimeout(); // may set idle timeout
    return;  // write-loop restarts read-loop if necessary
  }

  this->asyncReadSome();  // Continue read loop
}

// Process the given incoming chunk.
template <SocketType ST>
void VstConnection<ST>::processChunk(Chunk const& chunk) {
  auto msgID = chunk.header.messageID();
  FUERTE_LOG_VSTTRACE << "processChunk: messageID=" << msgID << "\n";

  // Find requestItem for this chunk.
  auto it = _messages.find(chunk.header.messageID());
  if (it == _messages.end()) {
    FUERTE_LOG_ERROR << "got chunk with unknown message ID: " << msgID << "\n";
    return;
  }
  std::shared_ptr<vst::RequestItem> item = it->second;

  // We've found the matching RequestItem.
  item->addChunk(chunk);

  // Try to assembly chunks in RequestItem to complete response.
  auto completeBuffer = item->assemble();
  if (completeBuffer) {
    FUERTE_LOG_VSTTRACE << "processChunk: complete response received\n";
    this->_proto.timer.cancel();

    // Message is complete
    // Remove message from store
    _messages.erase(item->_messageID);
    uint32_t q = _numMessages.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);

    try {
      // Create response
      auto resp = createResponse(*item, completeBuffer);
      auto err = resp != nullptr ? Error::NoError : Error::ProtocolError;
      item->_callback(err, std::move(item->request), std::move(resp));
    } catch (...) {
      FUERTE_LOG_ERROR << "unhandled exception in fuerte callback\n";
    }

    this->setIOTimeout();  // readjust timeout
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
  MessageType type = MessageType::Undefined;
  
  try {
    type = parser::validateAndExtractMessageType(itemCursor, itemLength, headerLength);
  } catch(std::exception const& e) {
    FUERTE_LOG_ERROR << "invalid VST message: '" << e.what() << "'";
  }
  
  if (type != MessageType::Response) {
    FUERTE_LOG_ERROR << "received unsupported vst message ("
    << static_cast<int>(type) << ") from server\n";
    if (type != MessageType::Undefined) {
      FUERTE_LOG_ERROR << "got '" << VPackSlice(itemCursor).toJson() << "'\n";
    }
    return nullptr;
  }

  ResponseHeader header =
      parser::responseHeaderFromSlice(VPackSlice(itemCursor));
  auto response = std::make_unique<Response>(std::move(header));
  response->setPayload(std::move(*responseBuffer), /*offset*/ headerLength);

  return response;
}

/// abort ongoing / unfinished requests
template <SocketType ST>
void VstConnection<ST>::abortOngoingRequests(const fuerte::Error err) {
  FUERTE_LOG_VSTTRACE << "aborting ongoing requests";
  // Reset the read & write loop
  // Cancel all items and remove them from the message store.
  if (err != Error::VstUnauthorized) {  // prevents stack overflow
    for (auto const& pair : _messages) {
      pair.second->invokeOnError(err);
    }
    _messages.clear();
    _numMessages.store(0);
  }
}

/// Set timeout accordingly
template <SocketType ST>
void VstConnection<ST>::setIOTimeout() {
  const bool isIdle = _messages.empty();
  if (isIdle && !this->_config._useIdleTimeout){
    return;
  }
  
  auto tp = std::chrono::steady_clock::time_point::max();
  if (isIdle) {  // use default connection timeout
    tp = std::chrono::steady_clock::now() + this->_config._idleTimeout;
  } else {
    for (auto const& pair : _messages) {
      tp = std::min(tp, pair.second->expires);
    }
  }

  const bool wasReading = this->_reading;
  const bool wasWriting = this->_writing;
  this->_timeoutOnReadWrite = false;

  // expires_after cancels pending ops
  this->_proto.timer.expires_at(tp);
  this->_proto.timer.async_wait(
      [=, self = Connection::weak_from_this()](auto const& ec) {
    std::shared_ptr<Connection> s;
    if (ec || !(s = self.lock())) {  // was canceled / deallocated
      return;
    }
    
    auto& me = static_cast<VstConnection<ST>&>(*s);
    if ((wasWriting && me._writing) ||
        (wasReading && me._reading)) {
      FUERTE_LOG_DEBUG << "VST-Request timeout" << " this=" << &me << "\n";
      FUERTE_ASSERT(!_messages.empty());
      
      me.abortExpiredRequests();
      
      if (_messages.empty()) {
        me._proto.cancel();
        me._timeoutOnReadWrite = true;
        // We simply cancel all ongoing asynchronous operations, the completion
        // handlers will do the rest.
        return;
      }
      
      // we still have messages left
      setIOTimeout();
      
    } else if (isIdle && !me._writing & !me._reading) {
      if (!me._active && me._state == Connection::State::Connected) {
        FUERTE_LOG_DEBUG << "HTTP-Request idle timeout"
              << " this=" << &me << "\n";
        me.shutdownConnection(Error::CloseRequested);
      }
    }
    // In all other cases we do nothing, since we have been posted to the
    // iocontext but the thing we should be timing out has already completed.
  });
}

// abort all expired requests
template <SocketType ST>
void VstConnection<ST>::abortExpiredRequests() {
  auto now = std::chrono::steady_clock::now();
  auto it = _messages.begin();
  while (it != _messages.end()) {
    if (it->second->expires < now) {
      FUERTE_LOG_DEBUG << "VST-Request timeout\n";
      it->second->invokeOnError(Error::Timeout);
      it = _messages.erase(it);
      uint32_t q = _numMessages.fetch_sub(1, std::memory_order_relaxed);
      FUERTE_ASSERT(q > 0);
    } else {
      it++;
    }
  }
}

template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
