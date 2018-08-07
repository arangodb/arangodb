////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "VstCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexUnlocker.h"
#include "Basics/Result.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/VstNetwork.h"
#include "Logger/LoggerFeature.h"
#include "Meta/conversion.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"

#include <limits>
#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

inline std::size_t validateAndCount(char const* vpStart,
                                    char const* vpEnd) {
  VPackOptions validationOptions = VPackOptions::Defaults;
  validationOptions.validateUtf8Strings = true;
  VPackValidator validator(&validationOptions);

  try {
    std::size_t numPayloads = 0;
    // check for slice start to the end of Chunk
    // isSubPart allows the slice to be shorter than the checked buffer.
    do {
      validator.validate(vpStart, std::distance(vpStart, vpEnd),
                         /*isSubPart =*/true);

      // get offset to next
      VPackSlice tmp(vpStart);
      vpStart += tmp.byteSize();
      numPayloads++;
    } while (vpStart != vpEnd);
    return numPayloads - 1;
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "len: " << std::distance(vpStart, vpEnd) << " - " << VPackSlice(vpStart).toHex();
    throw std::runtime_error(
        std::string("error during validation of incoming VPack: ") + e.what());
  }
}


VstCommTask::VstCommTask(Scheduler* scheduler, GeneralServer* server,
                         std::unique_ptr<Socket> socket, ConnectionInfo&& info,
                         double timeout, ProtocolVersion protocolVersion,
                         bool skipInit)
    : Task(scheduler, "VstCommTask"),
      GeneralCommTask(scheduler, server, std::move(socket), std::move(info), timeout,
                      skipInit),
      _authorized(!_auth->isActive()),
      _authMethod(rest::AuthenticationMethod::NONE),
      _authenticatedUser(),
      _protocolVersion(protocolVersion) {
  _protocol = "vst";

  // ATTENTION <- this is required so we do not lose information during a resize
  _readBuffer.reserve(_bufferLength);

  _maxChunkSize = arangodb::application_features::ApplicationServer::getFeature<
      ServerFeature>("Server")
      ->vstMaxSize();
}

/// @brief send simple response including response body
void VstCommTask::addSimpleResponse(rest::ResponseCode code, rest::ContentType respType,
                                    uint64_t messageId, velocypack::Buffer<uint8_t>&& buffer) {
  VstResponse resp(code, messageId);
  TRI_ASSERT(respType == rest::ContentType::VPACK); // or not ?
  resp.setContentType(respType);
  
  try {
    if (!buffer.empty()) {
      resp.setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    addResponse(resp, nullptr);
  } catch (...) {
    closeStream();
  }
}

void VstCommTask::addResponse(GeneralResponse& baseResponse,
                              RequestStatistics* stat) {
  TRI_ASSERT(_peer->runningInThisThread());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    VstResponse& response = dynamic_cast<VstResponse&>(baseResponse);
#else
    VstResponse& response = static_cast<VstResponse&>(baseResponse);
#endif

  finishExecution(baseResponse);
  VPackMessageNoOwnBuffer response_message = response.prepareForNetwork();
  uint64_t const mid = response_message._id;

  std::vector<VPackSlice> slices;

  if (response.generateBody()) {
    slices.reserve(1 + response_message._payloads.size());
    slices.push_back(response_message._header);

    for (auto& payload : response_message._payloads) {
      LOG_TOPIC(TRACE, Logger::REQUESTS) << "\"vst-request-result\",\""
                                         << (void*)this << "/" << mid << "\","
                                         << payload.toJson() << "\"";

      slices.push_back(payload);
    }
  } else {
    // header only
    slices.push_back(response_message._header);
  }

  // set some sensible maxchunk size and compression
  auto buffers = createChunkForNetwork(slices, mid, _maxChunkSize,
                                       _protocolVersion);
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  if (stat != nullptr && arangodb::Logger::isEnabled(arangodb::LogLevel::TRACE,
                                                     Logger::REQUESTS)) {
    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"vst-request-statistics\",\"" << (void*)this << "\",\""
        << VstRequest::translateVersion(_protocolVersion) << "\","
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
  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"vst-request-end\",\"" << (void*)this << "/" << mid << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << VstRequest::translateVersion(_protocolVersion) << "\","
      << static_cast<int>(response.responseCode()) << ","
      << "\"," << Logger::FIXED(totalTime, 6);
  
  // process remaining requests ?
  //processAll();
}

static uint32_t readLittleEndian32bit(char const* p) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(p[0])));
}

static uint64_t readLittleEndian64bit(char const* p) {
  return (static_cast<uint64_t>(static_cast<uint8_t>(p[7])) << 56) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[6])) << 48) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[5])) << 40) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[4])) << 32) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[3])) << 24) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[2])) << 16) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[1])) << 8) |
         (static_cast<uint64_t>(static_cast<uint8_t>(p[0])));
}

VstCommTask::ChunkHeader VstCommTask::readChunkHeader() {
  VstCommTask::ChunkHeader header;

  auto cursor = _readBuffer.begin() + _processReadVariables._readBufferOffset;

  header._chunkLength = readLittleEndian32bit(cursor);
  LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "chunkLength: "
                                          << header._chunkLength;
  cursor += sizeof(header._chunkLength);

  uint32_t chunkX = readLittleEndian32bit(cursor);
  cursor += sizeof(chunkX);

  header._isFirst = chunkX & 0x1;
  LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "is first: " << header._isFirst;
  header._chunk = chunkX >> 1;
  LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "chunk: " << header._chunk;

  header._messageID = readLittleEndian64bit(cursor);
  LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "message id: "
                                          << header._messageID;
  cursor += sizeof(header._messageID);

  // extract total len of message
  if (_protocolVersion == ProtocolVersion::VST_1_1 ||
      (header._isFirst && header._chunk > 1)) {
    header._messageLength = readLittleEndian64bit(cursor);
    cursor += sizeof(header._messageLength);
  } else {
    header._messageLength = 0;  // not needed for old protocol
  }

  header._headerLength = std::distance(
      _readBuffer.begin() + _processReadVariables._readBufferOffset, cursor);

  return header;
}

bool VstCommTask::isChunkComplete(char* start) {
  std::size_t length = std::distance(start, _readBuffer.end());
  auto& prv = _processReadVariables;

  if (!prv._currentChunkLength && length < sizeof(uint32_t)) {
    return false;
  }
  if (!prv._currentChunkLength) {
    // read chunk length
    std::memcpy(&prv._currentChunkLength, start, sizeof(uint32_t));
  }
  if (length < prv._currentChunkLength) {
    // chunk not complete
    return false;
  }

  return true;
}

void VstCommTask::handleAuthHeader(VPackSlice const& header,
                                   uint64_t messageId) {
  
  std::string authString;
  std::string user = "";
  _authorized = false;
  _authMethod = AuthenticationMethod::NONE;

  std::string encryption = header.at(2).copyString();
  if (encryption == "jwt") {// doing JWT
    authString = header.at(3).copyString();
    _authMethod = AuthenticationMethod::JWT;
  } else if (encryption == "plain") {
    user = header.at(3).copyString();
    std::string pass = header.at(4).copyString();
    authString = basics::StringUtils::encodeBase64(user + ":" + pass);
    _authMethod = AuthenticationMethod::BASIC;
  } else {
    LOG_TOPIC(ERR, Logger::REQUESTS) << "Unknown VST encryption type";
  }
  
  if (_auth->isActive()) { // will just fail if method is NONE
    auto entry = _auth->tokenCache()->checkAuthentication(_authMethod, authString);
    _authorized = entry.authenticated();
    if (_authorized) {
      _authenticatedUser = std::move(entry._username);
    } else {
      _authenticatedUser.clear();
    }
  } else {
    _authorized = true;
    _authenticatedUser = std::move(user); // may be empty
  }
  
  if (_authorized) {
    // mop: hmmm...user should be completely ignored if there is no auth IMHO
    // obi: user who sends authentication expects a reply
    addErrorResponse(ResponseCode::OK, rest::ContentType::VPACK, messageId, TRI_ERROR_NO_ERROR,
                     "auth successful");
  } else {
    addErrorResponse(rest::ResponseCode::UNAUTHORIZED, rest::ContentType::VPACK, messageId,
                     TRI_ERROR_HTTP_UNAUTHORIZED);
  }
}

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
    if (!getMessageFromSingleChunk(chunkHeader, message, doExecute,
                                   vpackBegin, chunkEnd)) {
      return false; // error, closeTask was called
    }
  } else {
    if (!getMessageFromMultiChunks(chunkHeader, message, doExecute,
                                   vpackBegin, chunkEnd)) {
      return false; // error, closeTask was called
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

    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"vst-request-header\",\"" << (void*)this << "/"
        << chunkHeader._messageID << "\"," << message.header().toJson() << "\"";

    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"vst-request-payload\",\"" << (void*)this << "/"
        << chunkHeader._messageID << "\"," << message.payload().toJson()
        << "\"";

    // get type of request
    int type = meta::underlyingValue(rest::RequestType::ILLEGAL);
    try {
      type = header.at(1).getNumber<int>();
    } catch (std::exception const& e) {
      addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                        chunkHeader._messageID, VPackBuffer<uint8_t>());
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VstCommTask: "
          << "VPack Validation failed: " << e.what();
      closeTask(rest::ResponseCode::BAD);
      return false;
    }
    
    // handle request types
    if (type == 1000) {
      handleAuthHeader(header, chunkHeader._messageID);
    } else {
      
      // the handler will take ownership of this pointer
      std::unique_ptr<VstRequest> request(new VstRequest(
          _connectionInfo, std::move(message), chunkHeader._messageID));
      request->setAuthenticated(_authorized);
      request->setUser(_authenticatedUser);
      request->setAuthenticationMethod(_authMethod);
      if (_authorized && _auth->userManager() != nullptr) {
        // if we don't call checkAuthentication we need to refresh
        _auth->userManager()->refreshUser(_authenticatedUser);
      }
      
      RequestFlow cont = prepareExecution(*request.get());
      if (cont == RequestFlow::Continue) {
        auto resp = std::make_unique<VstResponse>(rest::ResponseCode::SERVER_ERROR,
                                                  chunkHeader._messageID);
        resp->setContentTypeRequested(request->contentTypeResponse());
        executeRequest(std::move(request), std::move(resp));
      }
    }
  }

  if (prv._readBufferOffset == _readBuffer.length()) {
    return false;
  }
  return true;
}

void VstCommTask::closeTask(rest::ResponseCode code) {
  _processReadVariables._readBufferOffset = 0;
  _processReadVariables._currentChunkLength = 0;
  _readBuffer.clear();  // check is this changes the reserved size

  // we close the connection hard and do not even try to
  // to answer with failed packages
  //
  // is there a case where we do not want to close the connection
  // for (auto const& message : _incompleteMessages) {
  //  handleSimpleError(code, message.first);
  //}

  _incompleteMessages.clear();
  closeStream();
}

std::unique_ptr<GeneralResponse> VstCommTask::createResponse(
    rest::ResponseCode responseCode, uint64_t messageId) {
  return std::unique_ptr<GeneralResponse>(
      new VstResponse(responseCode, messageId));
}

// Returns true if and only if there was no error, if false is returned,
// the connection is closed
bool VstCommTask::getMessageFromSingleChunk(
    ChunkHeader const& chunkHeader, VstInputMessage& message, bool& doExecute,
    char const* vpackBegin, char const* chunkEnd) {
  // add agent for this new message

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                          << "chunk contains single message";
  std::size_t payloads = 0;

  try {
    payloads = validateAndCount(vpackBegin, chunkEnd);
  } catch (std::exception const& e) {
    addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                      chunkHeader._messageID, VPackBuffer<uint8_t>());
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VstCommTask: "
        << "VPack Validation failed: " << e.what();
    closeTask(rest::ResponseCode::BAD);
    return false;
  } catch (...) {
    addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                      chunkHeader._messageID, VPackBuffer<uint8_t>());
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                            << "VPack Validation failed";
    closeTask(rest::ResponseCode::BAD);
    return false;
  }

  VPackBuffer<uint8_t> buffer;
  buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
  message.set(chunkHeader._messageID, std::move(buffer), payloads);  // fixme

  doExecute = true;
  return true;
}

// Returns true if and only if there was no error, if false is returned,
// the connection is closed
bool VstCommTask::getMessageFromMultiChunks(
    ChunkHeader const& chunkHeader, VstInputMessage& message, bool& doExecute,
    char const* vpackBegin, char const* chunkEnd) {
  // CASE 2:  message is in multiple chunks
  auto incompleteMessageItr = _incompleteMessages.find(chunkHeader._messageID);

  // CASE 2a: chunk starts new message
  if (chunkHeader._isFirst) {  // first chunk of multi chunk message
    // add agent for this new message
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                            << "chunk starts a new message";
    if (incompleteMessageItr != _incompleteMessages.end()) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VstCommTask: "
          << "Message should be first but is already in the Map of "
             "incomplete "
             "messages";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }

    // TODO: is a 32bit value sufficient for the messageLength here?
    IncompleteVPackMessage message(
        static_cast<uint32_t>(chunkHeader._messageLength),
        chunkHeader._chunk /*number of chunks*/);
    message._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    auto insertPair = _incompleteMessages.emplace(
        std::make_pair(chunkHeader._messageID, std::move(message)));
    if (!insertPair.second) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                              << "insert failed";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }

    // CASE 2b: chunk continues a message
  } else {  // followup chunk of some mesage
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                            << "chunk continues a message";
    if (incompleteMessageItr == _incompleteMessages.end()) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VstCommTask: "
          << "found message without previous part";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }
    auto& im = incompleteMessageItr->second;  // incomplete Message
    im._currentChunk++;
    TRI_ASSERT(im._currentChunk == chunkHeader._chunk);
    im._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    // check buffer longer than length

    // MESSAGE COMPLETE
    if (im._currentChunk == im._numberOfChunks - 1 /* zero based counting */) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                              << "chunk completes a message";
      std::size_t payloads = 0;

      try {
        payloads =
            validateAndCount(reinterpret_cast<char const*>(im._buffer.data()),
                             reinterpret_cast<char const*>(
                                 im._buffer.data() + im._buffer.byteSize()));

      } catch (std::exception const& e) {
        addErrorResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                         chunkHeader._messageID, TRI_ERROR_BAD_PARAMETER, e.what());
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "VstCommTask: "
            << "VPack Validation failed: " << e.what();
        closeTask(rest::ResponseCode::BAD);
        return false;
      } catch (...) {
        addSimpleResponse(rest::ResponseCode::BAD, rest::ContentType::VPACK,
                          chunkHeader._messageID, VPackBuffer<uint8_t>());
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VstCommTask: "
                                                << "VPack Validation failed!";
        closeTask(rest::ResponseCode::BAD);
        return false;
      }

      message.set(chunkHeader._messageID, std::move(im._buffer), payloads);
      _incompleteMessages.erase(incompleteMessageItr);
      // check length

      doExecute = true;
    }
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VstCommTask: "
        << "chunk does not complete a message";
  }
  return true;
}
