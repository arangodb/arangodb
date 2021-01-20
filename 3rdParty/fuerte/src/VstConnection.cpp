////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2020 ArangoDB GmbH, Cologne, Germany
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
    : fuerte::MultiConnection<ST, vst::RequestItem>(loop, config),
      _vstVersion(config._vstVersion) {}

template <SocketType ST>
VstConnection<ST>::~VstConnection() try {
  abortRequests(Error::ConnectionCanceled,
                /*now*/ std::chrono::steady_clock::time_point::max());
} catch (...) {
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
  auto item = std::make_unique<RequestItem>(
      nullptr, [self](Error error, std::unique_ptr<Request>,
                      std::unique_ptr<Response> resp) {
        auto& me = static_cast<VstConnection<ST>&>(*self);
        if (error != Error::NoError || resp->statusCode() != StatusOK) {
          me.shutdownConnection(Error::VstUnauthorized,
                                "could not authenticate");
        } else {
          me._state.store(Connection::State::Connected);
          if (!me._writing) {
            me.asyncWriteNextRequest();
          }
        }
      });
  item->expires = std::chrono::steady_clock::now() + Request::defaultTimeout;

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

  this->_streams.emplace(item->_messageID,
                         std::move(item));  // add message to store
  this->_streamCount.fetch_add(1, std::memory_order_relaxed);

  this->_writing = true;
  this->setIOTimeout();  // has to be after setting _writing
  asio_ns::async_write(
      this->_proto.socket, buffers, [self](auto const& ec, std::size_t nsend) {
        auto& me = static_cast<VstConnection<ST>&>(*self);
        me._writing = false;
        if (ec) {
          me.shutdownConnection(Error::CouldNotConnect, "authorization failed");
        } else {
          me.asyncReadSome();  // read the response
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
  std::unique_ptr<RequestItem> item(ptr);
  FUERTE_ASSERT(ptr != nullptr);

  uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
  FUERTE_ASSERT(q > 0);

  auto buffers = item->prepareForNetwork(_vstVersion);

  this->_streams.emplace(item->_messageID,
                         std::move(item));  // add message to store
  this->_streamCount.fetch_add(1, std::memory_order_relaxed);

  this->_writing = true;
  this->setIOTimeout();  // prepare request / connection timeouts
  asio_ns::async_write(this->_proto.socket, std::move(buffers),
                       [self(Connection::shared_from_this())](
                           auto const& ec, std::size_t nwrite) mutable {
                         auto& me = static_cast<VstConnection<ST>&>(*self);
                         me._writing = false;
                         me.asyncWriteCallback(ec);
                       });

  FUERTE_LOG_VSTTRACE << "asyncWrite: done\n";
}

// callback of async_write function that is called in sendNextRequest.
template <SocketType ST>
void VstConnection<ST>::asyncWriteCallback(asio_ns::error_code const& ec) {
  // auto pendingAsyncCalls = --_connection->_async_calls;
  if (ec) {
    // Send failed
    FUERTE_LOG_VSTTRACE << "asyncWriteCallback: error " << ec.message() << "\n";

    // Stop current connection and try to restart a new one.
    this->shutdownConnection(this->translateError(ec, Error::WriteError),
                             "write error");
    return;
  }

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
    this->shutdownConnection(this->translateError(ec, Error::ReadError));
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
  if (this->_streams.empty() && !(this->_writing)) {
    FUERTE_LOG_VSTTRACE << "shouldStopReading: no more pending "
                           "messages/requests, stopping read\n";
    FUERTE_ASSERT(!this->_reading);
    this->setIOTimeout();  // may set idle timeout
    return;                // write-loop restarts read-loop if necessary
  }

  this->asyncReadSome();  // Continue read loop
}

// Process the given incoming chunk.
template <SocketType ST>
void VstConnection<ST>::processChunk(Chunk const& chunk) {
  auto msgID = chunk.header.messageID();
  FUERTE_LOG_VSTTRACE << "processChunk: messageID=" << msgID << "\n";

  // Find requestItem for this chunk.
  auto it = this->_streams.find(chunk.header.messageID());
  if (it == this->_streams.end()) {
    FUERTE_LOG_ERROR << "got chunk with unknown message ID: " << msgID << "\n";
    return;
  }

  // We've found the matching RequestItem.
  std::unique_ptr<vst::RequestItem>& item = it->second;
  item->addChunk(chunk);

  // Try to assembly chunks in RequestItem to complete response.
  auto completeBuffer = item->assemble();
  if (completeBuffer) {
    FUERTE_LOG_VSTTRACE << "processChunk: complete response received\n";
    this->_proto.timer.cancel();

    // Message is complete, create response
    try {
      auto resp = createResponse(*item, completeBuffer);
      auto err = resp != nullptr ? Error::NoError : Error::ProtocolError;
      item->_callback(err, std::move(item->request), std::move(resp));
    } catch (...) {
      FUERTE_LOG_ERROR << "unhandled exception in fuerte callback\n";
    }

    // Remove message from store
    this->_streams.erase(item->_messageID);
    uint32_t q = this->_streamCount.fetch_sub(1, std::memory_order_relaxed);
    FUERTE_ASSERT(q > 0);

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
    type = parser::validateAndExtractMessageType(itemCursor, itemLength,
                                                 headerLength);
  } catch (std::exception const& e) {
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
/// abort ongoing / unfinished requests expiring before given timepoint
template <SocketType ST>
void VstConnection<ST>::abortRequests(
    fuerte::Error err, std::chrono::steady_clock::time_point now) {
  if (err == Error::VstUnauthorized) {
    return;  // prevents stack overflow
  }

  auto it = this->_streams.begin();
  while (it != this->_streams.end()) {
    if (it->second->expires < now) {
      FUERTE_LOG_DEBUG << "VST-Request timeout\n";
      it->second->invokeOnError(err);
      it = this->_streams.erase(it);
      uint32_t q = this->_streamCount.fetch_sub(1, std::memory_order_relaxed);
      FUERTE_ASSERT(q > 0);
    } else {
      it++;
    }
  }
}

template struct arangodb::fuerte::v1::vst::VstConnection<SocketType::Tcp>;
template struct arangodb::fuerte::v1::vst::VstConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template struct arangodb::fuerte::v1::vst::VstConnection<SocketType::Unix>;
#endif

}}}}  // namespace arangodb::fuerte::v1::vst
