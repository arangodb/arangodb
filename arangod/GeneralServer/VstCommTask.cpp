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
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/LoggerFeature.h"
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

// throws on error
//namespace {
//
//inline void validateMessage(char const* vpStart, char const* vpEnd) {
//  VPackOptions validationOptions = VPackOptions::Defaults;
//  validationOptions.validateUtf8Strings = true;
//  validationOptions.checkAttributeUniqueness = true;
//  validationOptions.disallowExternals = true;
//  validationOptions.disallowCustom = true;
//  VPackValidator validator(&validationOptions);
//
//  // isSubPart allows the slice to be shorter than the checked buffer.
//  validator.validate(vpStart, std::distance(vpStart, vpEnd),
//                     /*isSubPart =*/true);
//
//  VPackSlice slice = VPackSlice(reinterpret_cast<uint8_t const*>(vpStart));
//  if (!slice.isArray() || slice.length() < 2) {
//    throw std::runtime_error(
//        "VST message does not contain a valid request header");
//  }
//
//  VPackSlice vSlice = slice.at(0);
//  if (!vSlice.isNumber<short>() || vSlice.getNumber<int>() != 1) {
//    throw std::runtime_error("VST message header has an unsupported version");
//  }
//  VPackSlice typeSlice = slice.at(1);
//  if (!typeSlice.isNumber<int>()) {
//    throw std::runtime_error("VST message is not of type request");
//  }
//}
//
//} // namespace


template<SocketType T>
VstCommTask<T>::VstCommTask(GeneralServer& server,
                            ConnectionInfo info,
                            std::unique_ptr<AsioSocket<T>> so,
                            fuerte::vst::VSTVersion v)
: GeneralCommTask<T>(server, "HttpCommTask", std::move(info), std::move(so)),
  _writing(false),
  _authorized(!this->_auth->isActive()),
  _authMethod(rest::AuthenticationMethod::NONE),
  _vstVersion(v) {}

/// @brief send simple response including response body
template<SocketType T>
void VstCommTask<T>::addSimpleResponse(rest::ResponseCode code,
                                       rest::ContentType respType, uint64_t messageId,
                                       velocypack::Buffer<uint8_t>&& buffer) {
  auto resp = std::make_unique<VstResponse>(code, messageId);
  TRI_ASSERT(respType == rest::ContentType::VPACK);  // or not ?
  resp->setContentType(respType);

  try {
    if (!buffer.empty()) {
      resp->setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    sendResponse(std::move(resp), this->stealStatistics(messageId));
  } catch (...) {
    this->close();
  }
}

template <SocketType T>
bool VstCommTask<T>::readCallback(asio_ns::error_code ec) {
  using namespace fuerte;
  if (ec) {
    if (ec != asio_ns::error::misc_errors::eof) {
      LOG_TOPIC("395fe", DEBUG, Logger::REQUESTS)
      << "Error while reading from socket: '" << ec.message() << "'";
    }
    this->close();
    return false;
  }
  
  // Inspect the data we've received so far.
  auto recvBuffs = this->_protocol->buffer.data();  // no copy
  auto cursor = asio_ns::buffer_cast<const uint8_t*>(recvBuffs);
  auto available = asio_ns::buffer_size(recvBuffs);
  // TODO technically buffer_cast is deprecated
  
  size_t parsedBytes = 0;
  while (vst::parser::isChunkComplete(cursor, available)) {
    // Read chunk
    vst::Chunk chunk;
    switch (_vstVersion) {
      case vst::VST1_1:
        chunk = vst::parser::readChunkHeaderVST1_1(cursor);
        break;
      case vst::VST1_0:
        chunk = vst::parser::readChunkHeaderVST1_0(cursor);
        break;
      default:
        TRI_ASSERT(false);
        this->close();
        return false;
    }
    
    if (available < chunk.header.chunkLength()) { // prevent reading beyond buffer
      LOG_TOPIC("2c6b4", DEBUG, arangodb::Logger::REQUESTS)
        << "invalid chunk header";
      this->close();
      return false;
    }
    
    // move cursors
    cursor += chunk.header.chunkLength();
    available -= chunk.header.chunkLength();
    parsedBytes += chunk.header.chunkLength();
    
    // Process chunk
    if (!processChunk(chunk)) {
      this->close();
      return false;
    }
  }
  
  // Remove consumed data from receive buffer.
  this->_protocol->buffer.consume(parsedBytes);
  
  return true; // continue read loop
}

// Process the given incoming chunk.
template<SocketType T>
bool VstCommTask<T>::processChunk(fuerte::vst::Chunk const& chunk) {
    
  if (chunk.header.isFirst()) {
    RequestStatistics* stat = this->acquireStatistics(chunk.header.messageID());
    RequestStatistics::SET_READ_START(stat, TRI_microtime());
    
    if (chunk.header.numberOfChunks() == 1) {
      VPackBuffer<uint8_t> buffer; // TODO lease buffers ?
      buffer.append(reinterpret_cast<uint8_t const*>(chunk.body.data()),
                    chunk.body.size());
      return processMessage(std::move(buffer), chunk.header.messageID());
    }
  }
  
  Message* msg;
  // Find stored message for this chunk.
  auto it = _messages.find(chunk.header.messageID());
  if (it == _messages.end()) {
    auto tmp = std::make_unique<Message>();
    msg = tmp.get();
    _messages.emplace(chunk.header.messageID(), std::move(tmp));
  } else {
    msg = it->second.get();
  }
  
  msg->addChunk(chunk);
  if (!msg->assemble()) { // not done yet
    return true;
  }
  
  //this->_proto->timer.cancel();
  auto guard = scopeGuard([&] {
    _messages.erase(chunk.header.messageID());
  });
  return processMessage(std::move(msg->buffer), chunk.header.messageID());
}

/// process a VST message
template<SocketType T>
bool VstCommTask<T>::processMessage(velocypack::Buffer<uint8_t> buffer,
                                    uint64_t messageId) {
  using namespace fuerte;

  auto ptr = buffer.data();
  auto len = buffer.byteSize();
  
  // first part of the buffer contains the response buffer
  std::size_t headerLength;
  MessageType mt = vst::parser::validateAndExtractMessageType(ptr, len, headerLength);

  RequestStatistics::SET_READ_END(this->statistics(messageId));
  
  VPackSlice header(buffer.data());
  
  //  if (Logger::logRequestParameters()) {
  //    LOG_TOPIC("5479a", DEBUG, Logger::REQUESTS)
  //    << "\"vst-request-header\",\"" << (void*)this << "/"
  //    << chunkHeader._messageID << "\"," << message.header().toJson() << "\"";
  //
  //    /*LOG_TOPIC("4feb4", DEBUG, Logger::REQUESTS)
  //     << "\"vst-request-payload\",\"" << (void*)this << "/"
  //     << chunkHeader._messageID << "\"," <<
  //     VPackSlice(message.payload()).toJson()
  //     << "\"";
  //     */
  //  }
  
  // handle request types
  if (mt == MessageType::Authentication) {  // auth
    handleAuthHeader(header, messageId);
  } else if (mt == MessageType::Request) {  // request
    
    // the handler will take ownership of this pointer
    auto req = std::make_unique<VstRequest>(this->_connectionInfo,
                                            std::move(buffer),
                                            /*payloadOffset*/headerLength,
                                            messageId);
    req->setAuthenticated(_authorized);
    req->setUser(this->_authToken._username);
    req->setAuthenticationMethod(_authMethod);
    if (_authorized && this->_auth->userManager() != nullptr) {
      // if we don't call checkAuthentication we need to refresh
      this->_auth->userManager()->refreshUser(this->_authToken._username);
    }
    
    CommTask::Flow cont = this->prepareExecution(*req.get());
    if (cont == CommTask::Flow::Continue) {
      auto resp = std::make_unique<VstResponse>(rest::ResponseCode::SERVER_ERROR, messageId);
      resp->setContentTypeRequested(req->contentTypeResponse());
      this->executeRequest(std::move(req), std::move(resp));
    }
  } else {  // not supported on server
    LOG_TOPIC("b5073", ERR, Logger::REQUESTS)
    << "\"vst-request-header\",\"" << (void*)this << "/"
    << messageId << "\"," << header.toJson() << "\""
    << " is unsupported";
    addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                      messageId, VPackBuffer<uint8_t>());
  }
  return true;
}


template<SocketType T>
void VstCommTask<T>::sendResponse(std::unique_ptr<GeneralResponse> baseRes, RequestStatistics* stat) {
  using namespace fuerte;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VstResponse& response = dynamic_cast<VstResponse&>(*baseRes);
#else
  VstResponse& response = static_cast<VstResponse&>(*baseRes);
#endif

  this->finishExecution(*baseRes);
  
  auto resItem = std::make_unique<ResponseItem>();
  response.writeMessageHeader(resItem->metadata);
  resItem->response = std::move(baseRes);
  
  asio_ns::const_buffer payload;
  if (response.generateBody()) {
    payload = asio_ns::buffer(response.payload().data(),
                              response.payload().size());
  }
  vst::message::prepareForNetwork(_vstVersion, response.messageId(),
                                  resItem->metadata, payload,
                                  resItem->buffers);
  
  if (stat != nullptr &&
      arangodb::Logger::isEnabled(arangodb::LogLevel::TRACE, Logger::REQUESTS)) {
    LOG_TOPIC("cf80d", DEBUG, Logger::REQUESTS)
    << "\"vst-request-statistics\",\"" << (void*)this << "\",\""
    << static_cast<int>(response.responseCode()) << ","
    << this->_connectionInfo.clientAddress << "\"," << stat->timingsCsv();
  }
  
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  // and give some request information
  LOG_TOPIC("92fd7", INFO, Logger::REQUESTS)
  << "\"vst-request-end\",\"" << (void*)this << "/" << response.messageId() << "\",\""
  << this->_connectionInfo.clientAddress << "\",\""
  << static_cast<int>(response.responseCode()) << ","
  << "\"," << Logger::FIXED(totalTime, 6);
  
  while (true) {
    if (_writeQueue.push(resItem.get())) {
      break;
    }
    cpu_relax();
  }
  resItem.release();
  
  bool expected = _writing.load();
  if (false == expected) {
    if (_writing.compare_exchange_strong(expected, true)) {
      doWrite(); // we managed to start writing
    }
  }
}

template<SocketType T>
void VstCommTask<T>::doWrite() {
  
  ResponseItem* tmp = nullptr;
  if (!_writeQueue.pop(tmp)) {
    // careful now, we need to consider that someone queues
    // a new request item while we store
    
    _writing.store(false);
    if (!_writeQueue.empty()) {
      bool expected = false;
      if (_writing.compare_exchange_strong(expected, true)) {
        doWrite(); // we managed to re-start writing
      }
    }
    return;
  }
  TRI_ASSERT(tmp != nullptr);
  std::unique_ptr<ResponseItem> item(tmp);
  
  auto& buffers = item->buffers;
  auto cb = [self = CommTask::shared_from_this(),
             item = std::move(item)](asio_ns::error_code ec,
                                     size_t transferred) {
    auto* thisPtr = static_cast<VstCommTask<T>*>(self.get());
    if (ec) {
      LOG_DEVEL << "boost write error: " << ec.message();
      thisPtr->close();
    } else {
      thisPtr->doWrite(); // write next one
    }
  };
  asio_ns::async_write(this->_protocol->socket, buffers, std::move(cb));
}

template<SocketType T>
void VstCommTask<T>::handleAuthHeader(VPackSlice header, uint64_t mId) {
  std::string authString;
  std::string user = "";
  _authorized = false;
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
    LOG_TOPIC("01f44", ERR, Logger::REQUESTS) << "Unknown VST encryption type";
  }

  this->_authToken = this->_auth->tokenCache().checkAuthentication(_authMethod, authString);
  _authorized = this->_authToken.authenticated();

  if (this->_authorized || !this->_auth->isActive()) {
    // simon: drivers expect a response for their auth request
    this->addErrorResponse(ResponseCode::OK, rest::ContentType::VPACK, mId,
                           TRI_ERROR_NO_ERROR, "auth successful");
  } else {
    this->_authToken = auth::TokenCache::Entry::Unauthenticated();
    this->addErrorResponse(rest::ResponseCode::UNAUTHORIZED, rest::ContentType::VPACK,
                           mId, TRI_ERROR_HTTP_UNAUTHORIZED);
  }
}

template<SocketType T>
std::unique_ptr<GeneralResponse> VstCommTask<T>::createResponse(rest::ResponseCode responseCode,
                                                                uint64_t messageId) {
  return std::make_unique<VstResponse>(responseCode, messageId);
}

/// add chunk to this message
template<SocketType T>
void VstCommTask<T>::Message::addChunk(fuerte::vst::Chunk const& chunk) {
  
  // Gather number of chunk info
  if (chunk.header.isFirst()) {
    expectedChunks = chunk.header.numberOfChunks();
    chunks.reserve(expectedChunks);
    LOG_DEVEL << "RequestItem::addChunk: set #chunks to "
    << expectedChunks << "\n";
    TRI_ASSERT(buffer.empty());
    if (buffer.capacity() < chunk.header.messageLength()) {
      buffer.reserve(chunk.header.messageLength() - buffer.capacity());
    }
  }
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(chunk.body.data());
  size_t offset = buffer.size();
  buffer.append(begin, chunk.body.size());
  // Add chunk to index list
  chunks.push_back(ChunkInfo{chunk.header.index(), offset, chunk.body.size()});
}

/// assemble message, if true result is in _buffer
template<SocketType T>
bool VstCommTask<T>::Message::assemble() {
  if (expectedChunks == 0) {
    // We don't have the first chunk yet
    LOG_DEVEL << "RequestItem::assemble: don't have first chunk";
    return false;
  }
  if (chunks.size() < expectedChunks) {
    // Not all chunks have arrived yet
    LOG_DEVEL << "RequestItem::assemble: not all chunks have arrived";
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
  if (!reject) {
    LOG_DEVEL << "RequestItem::assemble: fast-path, chunks are in order";
    return true;
  }
  
  // We now have all chunks. Sort them by index.
  LOG_DEVEL << "RequestItem::assemble: sort chunks";
  std::sort(chunks.begin(), chunks.end(), [](auto const& a, auto const& b) {
    return a.index < b.index;
  });
  
  // Combine chunk content
  LOG_DEVEL << "RequestItem::assemble: build response buffer";
  
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
