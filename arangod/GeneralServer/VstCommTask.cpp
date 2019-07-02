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
  VstResponse resp(code, messageId);
  TRI_ASSERT(respType == rest::ContentType::VPACK);  // or not ?
  resp.setContentType(respType);

  try {
    if (!buffer.empty()) {
      resp.setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    addResponse(resp, nullptr);
  } catch (...) {
    close();
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
    close();
    return false;
  }
  
  // Inspect the data we've received so far.
  auto recvBuffs = this->_readBuffer.data();  // no copy
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
        LOG_DEVEL << "Unknown VST version";
        close();
        return false;
    }
    
    if (available < chunk.header.chunkLength()) { // prevent reading beyond buffer
      LOG_DEVEL << "invalid chunk header";
      close();
      return false;
    }
    
    // move cursors
    cursor += chunk.header.chunkLength();
    available -= chunk.header.chunkLength();
    parsedBytes += chunk.header.chunkLength();
    
    // Process chunk
    if (!processChunk(chunk)) {
      close();
      return false;
    }
  }
  
  // Remove consumed data from receive buffer.
  this->_readBuffer.consume(parsedBytes);
  
  return true; // continue read loop
}

// Process the given incoming chunk.
template<SocketType T>
bool VstCommTask<T>::processChunk(fuerte::vst::Chunk const& chunk) {
  using namespace fuerte;
  
  auto msgID = chunk.header.messageID();
  LOG_DEVEL << "processChunk: messageID=" << msgID;
  
  // FIXME single chunk optimization
  Message* msg;
  // Find requestItem for this chunk.
  auto& it = _messages.find(chunk.header.messageID());
  if (it == _messages.end()) {
    auto tmp = std::make_unique<Message>();
    msg = tmp.get();
    _messages.emplace(chunk.header.messageID(), std::move(tmp));
  } else {
    msg = it.second.get();
  }
  
  // We've found the matching RequestItem.
  msg->addChunk(chunk);
  if (!msg->assemble()) { // not done yet
    return true;
  }
  
//  RequestStatistics::SET_READ_END(statistics(chunk.header.messageID()));
  
  //this->_proto->timer.cancel();
  auto guard = scopeGuard([&] {
    _messages.erase(chunk.header.messageID());
  });
  
  auto ptr = msg->_buffer.data();
  auto len = msg->_buffer.byteSize();
  
  // first part of the buffer contains the response buffer
  std::size_t headerLength;
  MessageType mt = vst::parser::validateAndExtractMessageType(ptr, len, headerLength);
  if (mt != MessageType::Request) {
    LOG_DEVEL << "received unsupported vst message from server";
    return false;
  }
  
  VPackSlice header(msg->_buffer.data());
  
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
  
  // get type of request, message header is validated earlier
  TRI_ASSERT(header.isArray() && header.length() >= 2);
  TRI_ASSERT(header.at(1).isNumber<int>());  // va
  
  int type = header.at(1).getNumber<int>();
  // handle request types
  if (type == 1000) {  // auth
    handleAuthHeader(header, chunk.header.messageID());
  } else if (type == 1) {  // request
    
    // the handler will take ownership of this pointer
    auto req = std::make_unique<VstRequest>(this->_connectionInfo,
                                            std::move(msg->_buffer),
                                            /*payloadOffset*/headerLength,
                                            it.first);
    req->setAuthenticated(_authorized);
    req->setUser(this->_authToken._username);
    req->setAuthenticationMethod(_authMethod);
    if (_authorized && this->_auth->userManager() != nullptr) {
      // if we don't call checkAuthentication we need to refresh
      this->_auth->userManager()->refreshUser(this->_authToken._username);
    }
    
    CommTask::RequestFlow cont = prepareExecution(*req.get());
    if (cont == CommTask::RequestFlow::Continue) {
      auto resp = std::make_unique<VstResponse>(rest::ResponseCode::SERVER_ERROR,
                                                chunk.header.messageID());
      resp->setContentTypeRequested(req->contentTypeResponse());
      executeRequest(std::move(req), std::move(resp));
    }
  } else {  // not supported on server
    LOG_TOPIC("b5073", ERR, Logger::REQUESTS)
    << "\"vst-request-header\",\"" << (void*)this << "/"
    << chunk.header.messageID() << "\"," << header.toJson() << "\""
    << " is unsupported";
    addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                      chunk.header.messageID(), VPackBuffer<uint8_t>());
    return false;
  }
}


#if 0
// reads data from the socket
bool VstCommTask::processRead(double startTime) {
  TRI_ASSERT(_peer->runningInThisThread());
  
  auto& prv = _processReadVariables;
  auto chunkBegin = _readBuffer.begin() + prv._readBufferOffset;
  if (chunkBegin == nullptr || !isChunkComplete(chunkBegin)) {
    return false;  // no data or incomplete
  }
  
  ChunkHeader chunkHeader = readChunkHeader();
  auto chunkEnd = chunkBegin + chunkHeader._chunkLength;
  auto vpackBegin = chunkBegin + chunkHeader._headerLength;
  bool doExecute = false;
  VstInputMessage message;  // filled in CASE 1 or CASE 2b
  
  if (chunkHeader._isFirst) {
    // create agent for new messages
    RequestStatistics* stat = acquireStatistics(chunkHeader._messageID);
    RequestStatistics::SET_READ_START(stat, startTime);
  }
  
  RequestStatistics::SET_READ_END(statistics(chunkHeader._messageID));
  
  if (chunkHeader._isFirst && chunkHeader._chunk == 1) {
    // CASE 1: message is in one chunk
    if (!getMessageFromSingleChunk(chunkHeader, message, doExecute, vpackBegin, chunkEnd)) {
      return false;  // error, closeTask was called
    }
  } else {
    if (!getMessageFromMultiChunks(chunkHeader, message, doExecute, vpackBegin, chunkEnd)) {
      return false;  // error, closeTask was called
    }
  }
  
  prv._currentChunkLength = 0;  // we have read a complete chunk
  prv._readBufferOffset = std::distance(_readBuffer.begin(), chunkEnd);
  
  // clean buffer up to length of chunk
  if (prv._readBufferOffset > prv._cleanupLength) {
    _readBuffer.move_front(prv._readBufferOffset);
    prv._readBufferOffset = 0;  // the positon will be set at the
    // begin of this function
  }
  
  if (doExecute) {
    VPackSlice header = message.header();
    
    if (Logger::logRequestParameters()) {
      LOG_TOPIC("5479a", DEBUG, Logger::REQUESTS)
      << "\"vst-request-header\",\"" << (void*)this << "/"
      << chunkHeader._messageID << "\"," << message.header().toJson() << "\"";
      
      /*LOG_TOPIC("4feb4", DEBUG, Logger::REQUESTS)
       << "\"vst-request-payload\",\"" << (void*)this << "/"
       << chunkHeader._messageID << "\"," <<
       VPackSlice(message.payload()).toJson()
       << "\"";
       */
    }
    
    // get type of request, message header is validated earlier
    TRI_ASSERT(header.isArray() && header.length() >= 2);
    TRI_ASSERT(header.at(1).isNumber<int>());  // va
    
    int type = header.at(1).getNumber<int>();
    
    // handle request types
    if (type == 1000) {  // auth
      handleAuthHeader(header, chunkHeader._messageID);
    } else if (type == 1) {  // request
      
      // the handler will take ownership of this pointer
      auto req = std::make_unique<VstRequest>(_connectionInfo, std::move(message),
                                              chunkHeader._messageID);
      req->setAuthenticated(_authorized);
      req->setUser(_authToken._username);
      req->setAuthenticationMethod(_authMethod);
      if (_authorized && _auth->userManager() != nullptr) {
        // if we don't call checkAuthentication we need to refresh
        _auth->userManager()->refreshUser(_authToken._username);
      }
      
      RequestFlow cont = prepareExecution(*req.get());
      
      if (cont == RequestFlow::Continue) {
        auto resp = std::make_unique<VstResponse>(rest::ResponseCode::SERVER_ERROR,
                                                  chunkHeader._messageID);
        resp->setContentTypeRequested(req->contentTypeResponse());
        executeRequest(std::move(req), std::move(resp));
      }
    } else {  // not supported on server
      LOG_TOPIC("b5073", ERR, Logger::REQUESTS)
      << "\"vst-request-header\",\"" << (void*)this << "/"
      << chunkHeader._messageID << "\"," << message.header().toJson() << "\""
      << " is unsupported";
      addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                        chunkHeader._messageID, VPackBuffer<uint8_t>());
      return false;
    }
  }
  
  if (prv._readBufferOffset == _readBuffer.length()) {
    return false;
  }
  return true;
}
#endif

template<SocketType T>
void VstCommTask<T>::addResponse(GeneralResponse& baseResponse, RequestStatistics* stat) {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VstResponse& response = dynamic_cast<VstResponse&>(baseResponse);
#else
  VstResponse& response = static_cast<VstResponse&>(baseResponse);
#endif

  this->finishExecution(baseResponse);
  
#if 0
  VPackMessageNoOwnBuffer response_message = response.prepareForNetwork();
  uint64_t const mid = response_message._id;

  std::vector<VPackSlice> slices;

  if (response.generateBody()) {
    slices.reserve(1 + response_message._payloads.size());
    slices.push_back(response_message._header);

    for (auto& payload : response_message._payloads) {
      LOG_TOPIC("ac411", TRACE, Logger::REQUESTS)
          << "\"vst-request-result\",\"" << (void*)this << "/" << mid << "\","
          << payload.toJson() << "\"";

      slices.push_back(payload);
    }
  } else {
    // header only
    slices.push_back(response_message._header);
  }

  // set some sensible maxchunk size and compression
  auto buffers = createChunkForNetwork(slices, mid, maxChunkSize, _protocolVersion);
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  if (stat != nullptr &&
      arangodb::Logger::isEnabled(arangodb::LogLevel::TRACE, Logger::REQUESTS)) {
    LOG_TOPIC("cf80d", DEBUG, Logger::REQUESTS)
        << "\"vst-request-statistics\",\"" << (void*)this << "\",\""
        << GeneralRequest::translateVersion(_protocolVersion) << "\","
        << static_cast<int>(response.responseCode()) << ","
        << _connectionInfo.clientAddress << "\"," << stat->timingsCsv();
  }

  if (buffers.empty()) {
    if (stat != nullptr) {
      stat->release();
    }
  } else {
    size_t n = buffers.size() - 1;
    size_t c = 0;

    for (auto&& buffer : buffers) {
      if (c == n) {
        addWriteBuffer(WriteBuffer(buffer.release(), stat));
      } else {
        addWriteBuffer(WriteBuffer(buffer.release(), nullptr));
      }

      ++c;
    }
  }

  // and give some request information
  LOG_TOPIC("92fd7", INFO, Logger::REQUESTS)
      << "\"vst-request-end\",\"" << (void*)this << "/" << mid << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << static_cast<int>(response.responseCode()) << ","
      << "\"," << Logger::FIXED(totalTime, 6);

  // process remaining requests ?
  // processAll();
#endif
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
    _expectedChunks = chunk.header.numberOfChunks();
    _chunks.reserve(_expectedChunks);
    LOG_DEVEL << "RequestItem::addChunk: set #chunks to "
    << _expectedChunks << "\n";
    assert(_buffer.empty());
    if (_buffer.capacity() < chunk.header.messageLength()) {
      _buffer.reserve(chunk.header.messageLength() - _buffer.capacity());
    }
  }
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(chunk.body.data());
  size_t offset = _buffer.size();
  _buffer.append(begin, chunk.body.size());
  // Add chunk to index list
  _chunks.push_back(ChunkInfo{chunk.header.index(), offset, chunk.body.size()});
}

/// assemble message, if true result is in _buffer
template<SocketType T>
bool VstCommTask<T>::Message::assemble() {
  if (_expectedChunks == 0) {
    // We don't have the first chunk yet
    LOG_DEVEL << "RequestItem::assemble: don't have first chunk";
    return false;
  }
  if (_chunks.size() < _expectedChunks) {
    // Not all chunks have arrived yet
    LOG_DEVEL << "RequestItem::assemble: not all chunks have arrived";
    return false;
  }
  
  // fast-path: chunks received in-order
  bool reject = false;
  for (size_t i = 0; i < _expectedChunks; i++) {
    if (_chunks[i].index != i) {
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
  std::sort(_chunks.begin(), _chunks.end(), [](auto const& a, auto const& b) {
    return a.index < b.index;
  });
  
  // Combine chunk content
  LOG_DEVEL << "RequestItem::assemble: build response buffer";
  
  VPackBuffer<uint8_t> cp(std::move(_buffer));
  _buffer.clear();
  for (ChunkInfo const& info : _chunks) {
    _buffer.append(cp.data() + info.offset, info.size);
  }
  return true;
}
