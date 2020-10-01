////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "VstCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/Result.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/dtrace-wrapper.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "Meta/conversion.h"
#include "RestServer/ServerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"

#include <velocypack/Options.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

template <SocketType T>
VstCommTask<T>::VstCommTask(GeneralServer& server, ConnectionInfo info,
                            std::unique_ptr<AsioSocket<T>> so, fuerte::vst::VSTVersion v)
    : GeneralCommTask<T>(server, std::move(info), std::move(so)),
      _writeLoopActive(false),
      _numProcessing(0),
      _authToken("", false, 0),
      _authenticated(!this->_auth->isActive()),
      _authMethod(rest::AuthenticationMethod::NONE),
      _vstVersion(v) {
  // arbitrary initial reserve value to save a few memory allocations
  // in the most common cases.
  _writeQueue.reserve(32);
}

template <SocketType T>
VstCommTask<T>::~VstCommTask() {
  try {
    ResponseItem* tmp = nullptr;
    while (_writeQueue.pop(tmp)) {
      delete tmp;
    }
  } catch (...) {
  }
}

template <SocketType T>
void VstCommTask<T>::start() {
  LOG_TOPIC("7215f", DEBUG, Logger::REQUESTS)
      << "<vst> opened connection \"" << (void*)this << "\"";
  asio_ns::dispatch(this->_protocol->context.io_context, [self = this->shared_from_this()] {
    static_cast<VstCommTask<T>&>(*self).asyncReadSome();
  });
}

template <SocketType T>
bool VstCommTask<T>::readCallback(asio_ns::error_code ec) {
  using namespace fuerte;
  if (ec) {
    this->close(ec);
    return false;
  }

  // Inspect the data we've received so far.
  auto recvBuffs = this->_protocol->buffer.data();  // no copy
  auto cursor = asio_ns::buffer_cast<const uint8_t*>(recvBuffs);
  auto available = asio_ns::buffer_size(recvBuffs);
  // TODO technically buffer_cast is deprecated

  size_t parsedBytes = 0;
  while (true) {
    vst::Chunk chunk;
    vst::parser::ChunkState state = vst::parser::ChunkState::Invalid;
    if (_vstVersion == vst::VST1_1) {
      state = vst::parser::readChunkVST1_1(chunk, cursor, available);
    } else if (_vstVersion == vst::VST1_0) {
      state = vst::parser::readChunkVST1_0(chunk, cursor, available);
    }

    if (vst::parser::ChunkState::Incomplete == state) {
      break;
    } else if (vst::parser::ChunkState::Invalid == state) {  // actually should never happen
      this->close();
      return false;  // stop read loop
    }
    TRI_ASSERT(vst::parser::ChunkState::Complete == state);

    // move cursors
    cursor += chunk.header.chunkLength();
    available -= chunk.header.chunkLength();
    parsedBytes += chunk.header.chunkLength();

    // Process chunk
    if (!processChunk(chunk)) {
      this->close();
      return false;  // stop read loop
    }
  }

  // Remove consumed data from receive buffer.
  this->_protocol->buffer.consume(parsedBytes);

  return true;  // continue read loop
}

template <SocketType T>
void VstCommTask<T>::setIOTimeout() {
  double secs = GeneralServerFeature::keepAliveTimeout();
  if (secs <= 0) {
    return;
  }

  const bool wasReading = this->_reading;
  const bool wasWriting = this->_writing;
  TRI_ASSERT(wasReading || wasWriting);
  if (wasWriting) {
    secs = std::max(this->WriteTimeout, secs);
  }

  auto millis = std::chrono::milliseconds(static_cast<int64_t>(secs * 1000));
  this->_protocol->timer.expires_after(millis);  // cancels old waiters
  this->_protocol->timer.async_wait(
      [=, self = CommTask::weak_from_this()](asio_ns::error_code const& ec) {
        std::shared_ptr<CommTask> s;
        if (ec || !(s = self.lock())) {  // was canceled / deallocated
          return;
        }

        auto& me = static_cast<VstCommTask<T>&>(*s);

        bool idle = wasReading && me._reading && !me._writing;
        bool writeTimeout = wasWriting && me._writing;
        if (idle || writeTimeout) {
          // _numProcessing == 0 also if responses wait for writing
          if (me._numProcessing.load(std::memory_order_relaxed) == 0) {
            LOG_TOPIC("6a7ad", INFO, Logger::REQUESTS)
                << "keep alive timeout, closing stream!";
            static_cast<GeneralCommTask<T>&>(*s).close(ec);
          } else {
            setIOTimeout();
          }
        }
      });
}

// Process the given incoming chunk.
template <SocketType T>
bool VstCommTask<T>::processChunk(fuerte::vst::Chunk const& chunk) {
  if (chunk.body.size() > CommTask::MaximalBodySize) {
    LOG_TOPIC("695ef", WARN, Logger::REQUESTS)
        << "\"vst-request\"; chunk is too big for server, \""
        << chunk.body.size() << "\", this=" << this;
    return false;  // close connection
  } else if (chunk.body.size() == 0) {
    LOG_TOPIC("695ff", WARN, Logger::REQUESTS)
        << "\"vst-request\"; chunk was empty, this=" << this;
    return false;  // close connection
  }

  if (chunk.header.isFirst()) {
    this->acquireStatistics(chunk.header.messageID()).SET_READ_START(TRI_microtime());

    // single chunk optimization
    if (chunk.header.numberOfChunks() == 1) {
      VPackBuffer<uint8_t> buffer;  // TODO lease buffers ?
      buffer.append(reinterpret_cast<uint8_t const*>(chunk.body.data()),
                    chunk.body.size());
      TRI_ASSERT(_messages.find(chunk.header.messageID()) == _messages.end());
      processMessage(std::move(buffer), chunk.header.messageID());
      return true;
    }
  }

  // Find stored message for this chunk.
  Message& msg = _messages[chunk.header.messageID()];

  // returns false if message gets too big
  if (!msg.addChunk(chunk)) {
    LOG_TOPIC("695fd", WARN, Logger::REQUESTS)
        << "\"vst-request\"; chunk contents have become larger than "
        << "allowed, this=" << this;
    return false;  // close connection
  }

  if (!msg.assemble()) {
    return true;  // wait for more chunks
  }

  // this->_proto->timer.cancel();
  auto guard = scopeGuard([&] { _messages.erase(chunk.header.messageID()); });
  processMessage(std::move(msg.buffer), chunk.header.messageID());
  return true;
}

#ifdef USE_DTRACE
// Moved here to prevent multiplicity by template
static void __attribute__((noinline)) DTraceVstCommTaskProcessMessage(size_t th) {
  DTRACE_PROBE1(arangod, VstCommTaskProcessMessage, th);
}
#else
static void DTraceVstCommTaskProcessMessage(size_t) {}
#endif

/// process a VST message
template <SocketType T>
void VstCommTask<T>::processMessage(velocypack::Buffer<uint8_t> buffer, uint64_t messageId) {
  namespace fu = ::arangodb::fuerte::v1;

  DTraceVstCommTaskProcessMessage((size_t)this);

  if (this->stopped()) {
    return;  // we have to ignore this request because the connection has already been closed
  }

  // from here on we will send a response, the connection is not IDLE
  _numProcessing.fetch_add(1, std::memory_order_relaxed);

  auto ptr = buffer.data();
  auto len = buffer.byteSize();

  // first part of the buffer contains the response buffer
  std::size_t headerLength = 0;
  fu::MessageType mt = fu::MessageType::Undefined;
  try {
    mt = fu::vst::parser::validateAndExtractMessageType(ptr, len, headerLength);
  } catch (std::exception const& e) {
    LOG_TOPIC("6479a", ERR, Logger::REQUESTS)
        << "\"vst-request\"; invalid message: '" << e.what() << "'";
    // error is handled below
  }

  RequestStatistics::Item const& stat = this->statistics(messageId);
  stat.SET_READ_END();
  stat.ADD_RECEIVED_BYTES(buffer.size());

  // handle request types
  if (mt == fu::MessageType::Authentication) {  // auth
    handleVstAuthRequest(VPackSlice(buffer.data()), messageId);
    // Separate superuser traffic:
    // Note that currently, velocystream traffic will never come from
    // a forwarding, since we always forward with HTTP.
    if (_authMethod != AuthenticationMethod::NONE && _authenticated &&
        _authToken.username().empty()) {
      stat.SET_SUPERUSER();
    }
  } else if (mt == fu::MessageType::Request) {  // request

    // the handler will take ownership of this pointer
    auto req = std::make_unique<VstRequest>(this->_connectionInfo, std::move(buffer),
                                            /*payloadOffset*/ headerLength, messageId);
    req->setAuthenticated(_authenticated);
    req->setUser(_authToken.username());
    req->setAuthenticationMethod(_authMethod);
    if (_authenticated && this->_auth->userManager() != nullptr) {
      // if we don't call checkAuthentication we need to refresh
      this->_auth->userManager()->refreshUser(this->_authToken.username());
    }
    stat.SET_REQUEST_TYPE(req->requestType());

    // Separate superuser traffic:
    // Note that currently, velocystream traffic will never come from
    // a forwarding, since we always forward with HTTP.
    if (_authMethod != AuthenticationMethod::NONE && _authenticated &&
        this->_authToken.username().empty()) {
      stat.SET_SUPERUSER();
    }

    LOG_TOPIC("92fd6", INFO, Logger::REQUESTS)
        << "\"vst-request-begin\",\"" << (void*)this << "\",\""
        << this->_connectionInfo.clientAddress << "\",\""
        << VstRequest::translateMethod(req->requestType()) << "\",\""
        << (req->databaseName().empty() ? "" : "/_db/" + req->databaseName())
        << (Logger::logRequestParameters() ? req->fullUrl() : req->requestPath())
        << "\"";

    // TODO use different token if authentication header is present
    CommTask::Flow cont = this->prepareExecution(_authToken, *req.get());
    if (cont == CommTask::Flow::Continue) {
      auto resp = std::make_unique<VstResponse>(rest::ResponseCode::SERVER_ERROR, messageId);
      this->executeRequest(std::move(req), std::move(resp));
    }       // abort is handled in prepareExecution
  } else {  // not supported on server
    LOG_TOPIC("b5073", ERR, Logger::REQUESTS)
        << "\"vst-request-header\",\"" << (void*)this << "/" << messageId << "\""
        << " is unsupported";
    this->sendSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                             messageId, VPackBuffer<uint8_t>());
  }
}

#ifdef USE_DTRACE
// Moved here to prevent multiplicity by template
static void __attribute__((noinline)) DTraceVstCommTaskSendResponse(size_t th) {
  DTRACE_PROBE1(arangod, VstCommTaskSendResponse, th);
}
#else
static void DTraceVstCommTaskSendResponse(size_t) {}
#endif

template <SocketType T>
void VstCommTask<T>::sendResponse(std::unique_ptr<GeneralResponse> baseRes,
                                  RequestStatistics::Item stat) {
  using namespace fuerte;

  DTraceVstCommTaskSendResponse((size_t)this);

  unsigned n = _numProcessing.fetch_sub(1, std::memory_order_relaxed);
  TRI_ASSERT(n > 0);

  if (this->stopped()) {
    return;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VstResponse& response = dynamic_cast<VstResponse&>(*baseRes);
#else
  VstResponse& response = static_cast<VstResponse&>(*baseRes);
#endif

  this->finishExecution(*baseRes, /*origin*/ StaticStrings::Empty);

  auto resItem = std::make_unique<ResponseItem>();
  response.writeMessageHeader(resItem->metadata);
  resItem->response = std::move(baseRes);

  stat.SET_WRITE_START();

  asio_ns::const_buffer payload;
  if (response.generateBody()) {
    payload = asio_ns::buffer(response.payload().data(), response.payload().size());
  }
  vst::message::prepareForNetwork(_vstVersion, response.messageId(),
                                  resItem->metadata, payload, resItem->buffers);

  if (stat) {
    LOG_TOPIC("cf80d", TRACE, Logger::REQUESTS)
        << "\"vst-request-statistics\",\"" << (void*)this << "\",\""
        << static_cast<int>(response.responseCode()) << ","
        << this->_connectionInfo.clientAddress << "\"," << stat.timingsCsv();
  }

  double const totalTime = stat.ELAPSED_SINCE_READ_START();

  // and give some request information
  LOG_TOPIC("92fd7", DEBUG, Logger::REQUESTS)
      << "\"vst-request-end\",\"" << (void*)this << "/" << response.messageId()
      << "\",\"" << this->_connectionInfo.clientAddress << "\",\""
      << static_cast<int>(response.responseCode()) << ","
      << "\"," << Logger::FIXED(totalTime, 6);

  resItem->stat = std::move(stat);

  // this uses a fixed capacity queue, push might fail (unlikely, we limit max streams)
  unsigned retries = 512;
  try {
    while (ADB_UNLIKELY(!_writeQueue.push(resItem.get()) && --retries > 0)) {
      std::this_thread::yield();
      --retries;
    }
  } catch (...) {
    retries = 0;
  }
  if (retries == 0) {
    LOG_TOPIC("a3bfc", WARN, Logger::REQUESTS)
        << "was not able to queue response this=" << (void*)this;
    this->stop();  // stop is thread-safe
    return;
  }
  resItem.release();

  // start writing if necessary
  if (_writeLoopActive.load()) {
    return;
  }
  this->_protocol->context.io_context.post([self = this->shared_from_this()]() {
    auto& me = static_cast<VstCommTask<T>&>(*self);
    if (!me._writeLoopActive.exchange(true)) {
      me.doWrite();
    }
  });
}

#ifdef USE_DTRACE
// Moved out to avoid duplication by templates.
static void __attribute__((noinline)) DTraceVstCommTaskBeforeAsyncWrite(size_t th) {
  DTRACE_PROBE1(arangod, VstCommTaskBeforeAsyncWrite, th);
}
static void __attribute__((noinline)) DTraceVstCommTaskAfterAsyncWrite(size_t th) {
  DTRACE_PROBE1(arangod, VstCommTaskAfterAsyncWrite, th);
}
#else
static void DTraceVstCommTaskBeforeAsyncWrite(size_t) {}
static void DTraceVstCommTaskAfterAsyncWrite(size_t) {}
#endif

template <SocketType T>
void VstCommTask<T>::doWrite() {
  ResponseItem* tmp = nullptr;
  if (!_writeQueue.pop(tmp)) {
    // careful now, we need to consider that someone queues
    // a new request item
    _writeLoopActive.store(false);
    if (_writeQueue.empty()) {
      return;  // done, someone else may restart
    }

    if (_writeLoopActive.exchange(true)) {
      return;  // someone else restarted writing
    }
    bool success = _writeQueue.pop(tmp);
    TRI_ASSERT(success);
  }
  TRI_ASSERT(tmp != nullptr);
  std::unique_ptr<ResponseItem> item(tmp);

  DTraceVstCommTaskBeforeAsyncWrite((size_t)this);

  this->_writing = true;
  this->setIOTimeout();

  auto& buffers = item->buffers;
  asio_ns::async_write(this->_protocol->socket, buffers,
                       [self(CommTask::shared_from_this()),
                        rsp(std::move(item))](asio_ns::error_code const& ec, size_t) {
                         DTraceVstCommTaskAfterAsyncWrite((size_t)self.get());

                         auto& me = static_cast<VstCommTask<T>&>(*self);
                         me._writing = false;

                         rsp->stat.SET_WRITE_END();
                         rsp->stat.ADD_SENT_BYTES(rsp->buffers[0].size() +
                                                  rsp->buffers[1].size());
                         if (ec) {
                           me.close(ec);
                         } else {
                           me.doWrite();  // write next one
                         }
                       });
}

template <SocketType T>
void VstCommTask<T>::handleVstAuthRequest(VPackSlice header, uint64_t mId) {
  std::string authString;
  std::string user = "";
  _authenticated = false;
  _authMethod = AuthenticationMethod::NONE;

  std::string encryption = header.at(2).copyString();
  if (encryption == "jwt") {  // doing JWT
    authString = header.at(3).copyString();
    _authMethod = AuthenticationMethod::JWT;
  } else if (encryption == "plain") {
    user = header.at(3).copyString();
    std::string pass = header.at(4).copyString();
    authString = basics::StringUtils::encodeBase64(user + ":" + pass);
    _authMethod = AuthenticationMethod::BASIC;
  } else {
    LOG_TOPIC("01f44", WARN, Logger::REQUESTS) << "Unknown VST encryption type";
  }

  _authToken = this->_auth->tokenCache().checkAuthentication(_authMethod, authString);
  _authenticated = _authToken.authenticated();

  if (_authenticated || !this->_auth->isActive()) {
    // simon: drivers expect a response for their auth request
    this->sendErrorResponse(ResponseCode::OK, rest::ContentType::VPACK, mId,
                            TRI_ERROR_NO_ERROR, "auth successful");
  } else {
    _authToken = auth::TokenCache::Entry::Unauthenticated();
    this->sendErrorResponse(rest::ResponseCode::UNAUTHORIZED, rest::ContentType::VPACK,
                            mId, TRI_ERROR_HTTP_UNAUTHORIZED);
  }
}

template <SocketType T>
std::unique_ptr<GeneralResponse> VstCommTask<T>::createResponse(rest::ResponseCode responseCode,
                                                                uint64_t messageId) {
  return std::make_unique<VstResponse>(responseCode, messageId);
}

/// @brief add chunk to this message
/// @return false if the message size is too big
template <SocketType T>
bool VstCommTask<T>::Message::addChunk(fuerte::vst::Chunk const& chunk) {
  if (chunk.header.isFirst()) {
    // only the first chunk has the message length
    // and number of chunks (in VST/1.0)
    expectedChunks = chunk.header.numberOfChunks();
    expectedMsgSize = chunk.header.messageLength();
    chunks.reserve(expectedChunks);

    TRI_ASSERT(buffer.empty());
    if (buffer.capacity() < chunk.header.messageLength()) {
      buffer.reserve(chunk.header.messageLength() - buffer.capacity());
    }
  }

  // verify total message body size limit
  size_t newSize = buffer.size() + chunk.body.size();
  if (newSize > CommTask::MaximalBodySize ||
      (expectedMsgSize != 0 && expectedMsgSize < newSize)) {
    return false;  // error
  }

  uint8_t const* begin = reinterpret_cast<uint8_t const*>(chunk.body.data());
  size_t offset = buffer.size();
  buffer.append(begin, chunk.body.size());
  // Add chunk to index list
  chunks.push_back(ChunkInfo{chunk.header.index(), offset, chunk.body.size()});

  return true;
}

/// assemble message, if true result is in _buffer
template <SocketType T>
bool VstCommTask<T>::Message::assemble() {
  if (expectedChunks == 0 || chunks.size() < expectedChunks) {
    return false;
  }

  // fast-path: chunks received in-order
  bool reject = false;
  for (size_t i = 0; i < expectedChunks; i++) {
    if (chunks[i].index != i) {
      reject = true;
      break;
    }
  }
  if (!reject) {  //  fast-path, chunks are in order
    return true;
  }

  // We now have all chunks. Sort them by index.
  std::sort(chunks.begin(), chunks.end(),
            [](auto const& a, auto const& b) { return a.index < b.index; });

  // Combine chunk content
  VPackBuffer<uint8_t> cp(std::move(buffer));
  buffer.clear();
  for (ChunkInfo const& info : chunks) {
    buffer.append(cp.data() + info.offset, info.size);
  }
  return true;
}

template class arangodb::rest::VstCommTask<SocketType::Tcp>;
template class arangodb::rest::VstCommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::VstCommTask<SocketType::Unix>;
#endif
