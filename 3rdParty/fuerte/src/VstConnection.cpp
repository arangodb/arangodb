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
    : fuerte::GeneralConnection<ST>(loop, config),
      _writeQueue(),
      _numMessages(0),
      _vstVersion(config._vstVersion),
      _reading(false),
      _writing(false) {}

template <SocketType ST>
VstConnection<ST>::~VstConnection() try {
  drainQueue(Error::Canceled);
  abortOngoingRequests(Error::Canceled);
} catch (...) {
}

static std::atomic<MessageID> vstMessageId(1);
// sendRequest prepares a RequestItem for the given parameters
// and adds it to the send queue.
template <SocketType ST>
void VstConnection<ST>::sendRequest(std::unique_ptr<Request> req,
                                    RequestCallback cb) {
  FUERTE_ASSERT(req != nullptr);
  if (req->header.path.find("/_db/") != std::string::npos) {
    FUERTE_LOG_ERROR << "path: " << req->header.path << " \n";
  }
  FUERTE_ASSERT(req->header.path.find("/_db/") == std::string::npos);
  FUERTE_ASSERT(req->header.path.find('?') == std::string::npos);
  
  // it does not matter if IDs are reused on different connections
  uint64_t mid = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  // Create RequestItem from parameters
  auto item = std::make_unique<RequestItem>();
  item->_messageID = mid;
  item->_request = std::move(req);
  item->_callback = cb;
  // set the point-in-time when this request expires
  if (item->_request->timeout().count() > 0) {
    item->_expires =
        std::chrono::steady_clock::now() + item->_request->timeout();
  } else {
    item->_expires = std::chrono::steady_clock::time_point::max();
  }

  // Add item to send queue
  this->_numQueued.fetch_add(1, std::memory_order_relaxed);
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
    uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);
    item->invokeOnError(Error::QueueCapacityExceeded);
    return;
  }
  item.release();  // queue owns this now

  FUERTE_LOG_VSTTRACE << "queued item: this=" << this << "\n";

  // _state.load() after queuing request, to prevent race with connect
  Connection::State state = this->_state.load(std::memory_order_acquire);
  if (state == Connection::State::Connected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): start sending & reading\n";
    startWriting();  // try to start write loop

  } else if (state == Connection::State::Disconnected) {
    FUERTE_LOG_VSTTRACE << "sendRequest (vst): not connected\n";
    this->start(); // <- thread-safe connection start
  } else if (state == Connection::State::Failed) {
    FUERTE_LOG_ERROR << "queued request on failed connection\n";
    drainQueue(fuerte::Error::ConnectionClosed);
  }
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
        if (ec) {
          FUERTE_LOG_ERROR << ec.message() << "\n";
          me->shutdownConnection(Error::CouldNotConnect,
                                 "unable to connect: " + ec.message());
          me->drainQueue(Error::CouldNotConnect);
          return;
        }
        FUERTE_LOG_VSTTRACE << "VST connection established\n";
        if (me->_config._authenticationType != AuthenticationType::None) {
          // send the auth, then set _state == connected
          me->sendAuthenticationRequest();
        } else {
          me->_state.store(Connection::State::Connected);
          me->startWriting();  // start writing if something is queued
        }
      });
}

// Send out the authentication message on this connection
template <SocketType ST>
void VstConnection<ST>::sendAuthenticationRequest() {
  FUERTE_ASSERT(this->_config._authenticationType != AuthenticationType::None);

  // Part 1: Build ArangoDB VST auth message (1000)
  auto item = std::make_shared<RequestItem>();
  item->_messageID = vstMessageId.fetch_add(1, std::memory_order_relaxed);
  item->_expires = std::chrono::steady_clock::now() + Request::defaultTimeout;
  auto self = Connection::shared_from_this();
  item->_callback = [self](Error error, std::unique_ptr<Request>,
                           std::unique_ptr<Response> resp) {
    auto* thisPtr = static_cast<VstConnection<ST>*>(self.get());
    if (error != Error::NoError || resp->statusCode() != StatusOK) {
      thisPtr->_state.store(Connection::State::Failed);
      thisPtr->shutdownConnection(Error::VstUnauthorized,
                                  "could not authenticate");
      thisPtr->drainQueue(Error::VstUnauthorized);
    } else {
      thisPtr->_state.store(Connection::State::Connected);
      thisPtr->startWriting();
    }
  };

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
  auto cb = [this, self, item](asio_ns::error_code const& ec,
                               std::size_t nsend) {
    if (ec) {
      this->_state.store(Connection::State::Failed);
      this->shutdownConnection(Error::CouldNotConnect,
                               "authorization message failed");
      this->drainQueue(Error::CouldNotConnect);
    } else {
      _writing.store(true);
      asyncWriteCallback(ec, item, nsend);
    }
  };
  std::vector<asio_ns::const_buffer> buffers;
  vst::message::prepareForNetwork(_vstVersion, item->_messageID, item->_buffer,
                                  /*payload*/ asio_ns::const_buffer(), buffers);
  asio_ns::async_write(this->_proto.socket, buffers, std::move(cb));

  setTimeout();
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: activate the writer loop (if off and items are queud)
template <SocketType ST>
void VstConnection<ST>::startWriting() {
  FUERTE_ASSERT(this->_state.load(std::memory_order_acquire) ==
                Connection::State::Connected);
  FUERTE_LOG_VSTTRACE << "startWriting: this=" << this << "\n";

  if (_writing.load()) {
    return;  // There is already a write loop, do nothing
  }

  // we are the only ones here now

  FUERTE_LOG_HTTPTRACE << "startWriting: active=true, this=" << this << "\n";

  this->_io_context->post([self = Connection::shared_from_this(), this] {
    if (_writing.exchange(true)) {
      return;
    }
    
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

template <SocketType ST>
void VstConnection<ST>::terminateActivity() {
  // TODO: fill in the necessary stuff to fix bugs in this class
}

// writes data from task queue to network using asio_ns::async_write
template <SocketType ST>
void VstConnection<ST>::asyncWriteNextRequest() {
  FUERTE_LOG_VSTTRACE << "asyncWrite: preparing to send next\n";
  FUERTE_ASSERT(_writing.load());

  RequestItem* ptr = nullptr;
  if (!_writeQueue.pop(ptr)) {
    FUERTE_LOG_VSTTRACE << "asyncWriteNextRequest (vst): write queue empty\n";

    // careful now, we need to consider that someone queues
    // a new request item
    _writing.store(false);
    if (_writeQueue.empty()) {
      FUERTE_LOG_VSTTRACE << "asyncWriteNextRequest (vst): write stopped\n";
      return;  // done, someone else may restart
    }

    if (_writing.exchange(true)) {
      return;  // someone else restarted writing
    }
    bool success = _writeQueue.pop(ptr);
    FUERTE_ASSERT(success);
  }
  uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
  FUERTE_ASSERT(q > 0);
  
  std::shared_ptr<RequestItem> item(ptr);
  _messages.emplace(item->_messageID, item);  // add message to store
  _numMessages.fetch_add(1, std::memory_order_relaxed);

  setTimeout();  // prepare request / connection timeouts

  auto buffers = item->prepareForNetwork(_vstVersion);
  asio_ns::async_write(
      this->_proto.socket, std::move(buffers),
      [self = Connection::shared_from_this(), req(std::move(item))](
          auto const& ec, std::size_t nwrite) mutable {
        auto& me = static_cast<VstConnection<ST>&>(*self);
        me.asyncWriteCallback(ec, std::move(req), nwrite);
      });

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
    _messages.erase(item->_messageID);
    uint32_t q = _numMessages.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);

    auto err = translateError(ec, Error::WriteError);
    try {
      // let user know that this request caused the error
      item->_callback(err, std::move(item->_request), nullptr);
    } catch (...) {
    }
    
    _writing.store(false);
    // Stop current connection and try to restart a new one.
    this->restartConnection(err);
    return;
  }
  FUERTE_ASSERT(_writing.load());
  // Send succeeded
  FUERTE_LOG_VSTTRACE << "asyncWriteCallback: send succeeded, " << nwrite
                      << " bytes send\n";

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
    return;  // There is already a read loop, do nothing
  }
  FUERTE_ASSERT(requestsLeft() > 0);

  FUERTE_LOG_VSTTRACE << "startReading: active=true, this=" << this << "\n";
  this->asyncReadSome();
}

// asyncReadCallback is called when asyncReadSome is resulting in some data.
template <SocketType ST>
void VstConnection<ST>::asyncReadCallback(asio_ns::error_code const& ec) {
  if (ec) {
    FUERTE_LOG_VSTTRACE
        << "asyncReadCallback: Error while reading from socket: "
        << ec.message();
    _reading.store(false);
    this->restartConnection(translateError(ec, Error::ReadError));
    return;
  }
  FUERTE_ASSERT(_reading.load());

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
  if (_messages.empty() && !_writing.load()) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read\n";
    _reading.store(false);
    return;  // write-loop restarts read-loop if necessary
  }

  FUERTE_ASSERT(_reading.load());
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
      item->_callback(err, std::move(item->_request), std::move(resp));
    } catch (...) {
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

// adjust the timeouts (only call from IO-Thread)
template <SocketType ST>
void VstConnection<ST>::setTimeout() {
  // set to smallest point in time
  auto expires = std::chrono::steady_clock::time_point::max();
  if (_messages.empty()) {  // use default connection timeout
    expires = std::chrono::steady_clock::now() + this->_config._idleTimeout;
  } else {
    for (auto const& pair : _messages) {
      expires = std::max(expires, pair.second->_expires);
    }
  }

  this->_proto.timer.expires_at(expires);
  this->_proto.timer.async_wait(
      [self = Connection::weak_from_this()](asio_ns::error_code const& ec) {
        std::shared_ptr<Connection> s;
        if (ec || !(s = self.lock())) {  // was canceled / deallocated
          return;
        }

        auto& me = static_cast<VstConnection<ST>&>(*s);
        // cancel expired requests
        auto now = std::chrono::steady_clock::now();
        auto it = me._messages.begin();
        while (it != me._messages.end()) {
          if (it->second->_expires < now) {
            FUERTE_LOG_DEBUG << "VST-Request timeout\n";
            it->second->invokeOnError(Error::Timeout);
            it = me._messages.erase(it);
            uint32_t q = me._numMessages.fetch_sub(1, std::memory_order_relaxed);
            FUERTE_ASSERT(q > 0);
          } else {
            it++;
          }
        }

        if (me._messages.empty()) {  // no more messages to wait on
          FUERTE_LOG_DEBUG << "VST-Connection timeout\n";
          me.shutdownConnection(Error::Timeout);
        } else {
          me.setTimeout();
        }
      });
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

/// abort all requests lingering in the queue
template <SocketType ST>
void VstConnection<ST>::drainQueue(const fuerte::Error ec) {
  RequestItem* item = nullptr;
  while (_writeQueue.pop(item)) {
    std::unique_ptr<RequestItem> guard(item);
    uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);
    guard->invokeOnError(ec);
  }
}

template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
