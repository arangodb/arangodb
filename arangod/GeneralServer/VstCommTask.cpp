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
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "GeneralServer/VstNetwork.h"
#include "Logger/LoggerFeature.h"
#include "Meta/conversion.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"

#include <iostream>
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


VstCommTask::VstCommTask(EventLoop loop, GeneralServer* server,
                         std::unique_ptr<Socket> socket, ConnectionInfo&& info,
                         double timeout, ProtocolVersion protocolVersion,
                         bool skipInit)
    : Task(loop, "VstCommTask"),
      GeneralCommTask(loop, server, std::move(socket), std::move(info), timeout,
                      skipInit),
      _authorized(false),
      _authenticatedUser(),
      _protocolVersion(protocolVersion) {
  _protocol = "vst";

  // ATTENTION <- this is required so we do not lose information during a resize
  _readBuffer.reserve(_bufferLength);

  _maxChunkSize = arangodb::application_features::ApplicationServer::getFeature<
      ServerFeature>("Server")
      ->vstMaxSize();
}

void VstCommTask::addResponse(VstResponse* response, RequestStatistics* stat) {
  _lock.assertLockedByCurrentThread();

  VPackMessageNoOwnBuffer response_message = response->prepareForNetwork();
  uint64_t const id = response_message._id;

  std::vector<VPackSlice> slices;

  if (response->generateBody()) {
    slices.reserve(1 + response_message._payloads.size());
    slices.push_back(response_message._header);

    for (auto& payload : response_message._payloads) {
      LOG_TOPIC(DEBUG, Logger::REQUESTS) << "\"vst-request-result\",\""
                                         << (void*)this << "/" << id << "\","
                                         << payload.toJson() << "\"";

      slices.push_back(payload);
    }
  } else {
    // header only
    slices.push_back(response_message._header);
  }

  // set some sensible maxchunk size and compression
  auto buffers = createChunkForNetwork(slices, id, _maxChunkSize,
                                       _protocolVersion);
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  if (stat != nullptr && arangodb::Logger::isEnabled(arangodb::LogLevel::TRACE,
                                                     Logger::REQUESTS)) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "\"vst-request-statistics\",\"" << (void*)this << "\",\""
        << VstRequest::translateVersion(_protocolVersion) << "\","
        << static_cast<int>(response->responseCode()) << ","
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
        WriteBuffer b(buffer.release(), stat);
        addWriteBuffer(b);
      } else {
        WriteBuffer b(buffer.release(), nullptr);
        addWriteBuffer(b);
      }

      ++c;
    }
  }

  // and give some request information
  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"vst-request-end\",\"" << (void*)this << "/" << id << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << VstRequest::translateVersion(_protocolVersion) << "\","
      << static_cast<int>(response->responseCode()) << ","
      << "\"," << Logger::FIXED(totalTime, 6);
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
  _authorized = false;
  if (_authentication->isActive()) {
    std::string auth, user;
    AuthenticationMethod authType = AuthenticationMethod::NONE;

    std::string encryption = header.at(2).copyString();
    if (encryption == "jwt") {// doing JWT
      auth = header.at(3).copyString();
      authType = AuthenticationMethod::JWT;
    } else if (encryption == "plain") {
      user = header.at(3).copyString();
      std::string pass = header.at(4).copyString();
      auth = basics::StringUtils::encodeBase64(user + ":" + pass);
      authType = AuthenticationMethod::BASIC;
    } else {
      LOG_TOPIC(ERR, Logger::REQUESTS) << "Unknown VST encryption type";
    }
    AuthResult result = _authentication->authInfo()->checkAuthentication(
        authType, auth);

    _authorized = result._authorized;
    if (_authorized) {
      _authenticatedUser = std::move(result._username);
    }
  } else {
    _authorized = true;
  }
  
 VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0, true /*fakeRequest*/);
  if (_authorized) {
    // mop: hmmm...user should be completely ignored if there is no auth IMHO
    // obi: user who sends authentication expects a reply
    handleSimpleError(rest::ResponseCode::OK, fakeRequest, TRI_ERROR_NO_ERROR,
                      "authentication successful", messageId);
  } else {
    _authenticatedUser.clear();
    handleSimpleError(rest::ResponseCode::UNAUTHORIZED, fakeRequest,
                      TRI_ERROR_HTTP_UNAUTHORIZED, "authentication failed",
                      messageId);
  }
}

// reads data from the socket
bool VstCommTask::processRead(double startTime) {
  _lock.assertLockedByCurrentThread();

  auto& prv = _processReadVariables;

  auto chunkBegin = _readBuffer.begin() + prv._readBufferOffset;
  if (chunkBegin == nullptr || !isChunkComplete(chunkBegin)) {
    return false;  // no data or incomplete
  }

  ChunkHeader chunkHeader = readChunkHeader();
  auto chunkEnd = chunkBegin + chunkHeader._chunkLength;
  auto vpackBegin = chunkBegin + chunkHeader._headerLength;
  bool doExecute = false;
  bool read_maybe_only_part_of_buffer = false;
  VstInputMessage message;  // filled in CASE 1 or CASE 2b

  if (chunkHeader._isFirst) {
    // create agent for new messages
    RequestStatistics* stat = acquireStatistics(chunkHeader._messageID);
    RequestStatistics::SET_READ_START(stat, startTime);
  }

  if (chunkHeader._isFirst && chunkHeader._chunk == 1) {
    // CASE 1: message is in one chunk
    if (!getMessageFromSingleChunk(chunkHeader, message, doExecute,
                                   vpackBegin, chunkEnd)) {
      return false;
    }
  } else {
    if (!getMessageFromMultiChunks(chunkHeader, message, doExecute,
                                   vpackBegin, chunkEnd)) {
      return false;
    }
  }

  read_maybe_only_part_of_buffer = true;
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
      VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0);
      handleSimpleError(rest::ResponseCode::BAD, fakeRequest, chunkHeader._messageID);
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
      request->setAuthorized(_authorized);
      request->setUser(_authenticatedUser);
      GeneralServerFeature::HANDLER_FACTORY->setRequestContext(request.get());
      
      if (request->requestContext() == nullptr) {
        handleSimpleError(
                          rest::ResponseCode::NOT_FOUND, *request,
                          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                          TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND),
                          chunkHeader._messageID);
      } else {
        request->setClientTaskId(_taskId);

        // will determine if the user can access this path.
        // checks db permissions and contains exceptions for the
        // users API to allow logins
        rest::ResponseCode code = GeneralCommTask::canAccessPath(request.get());
        if (code != rest::ResponseCode::OK) {
          events::NotAuthorized(request.get());
          handleSimpleError(rest::ResponseCode::UNAUTHORIZED, *request, TRI_ERROR_FORBIDDEN,
                            "not authorized to execute this request",
                            chunkHeader._messageID);
        } else {
          // now that we are authorized we do the request
          // make sure we have a database
          if (request->requestContext() == nullptr) {
            handleSimpleError(
                              rest::ResponseCode::NOT_FOUND, *request,
                              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                              TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND),
                              chunkHeader._messageID);
          } else {
            request->setClientTaskId(_taskId);
            
            // temporarily release the mutex
            MUTEX_UNLOCKER(locker, _lock);
            
            std::unique_ptr<VstResponse> response(new VstResponse(
                 rest::ResponseCode::SERVER_ERROR, chunkHeader._messageID));
            response->setContentTypeRequested(request->contentTypeResponse());
            executeRequest(std::move(request), std::move(response));
          }
        }
      }
    }
  }

  if (read_maybe_only_part_of_buffer) {
    if (prv._readBufferOffset == _readBuffer.length()) {
      return false;
    }
    return true;
  }
  return doExecute;
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

void VstCommTask::handleSimpleError(rest::ResponseCode responseCode,
                                    GeneralRequest const& req,
                                    int errorNum,
                                    std::string const& errorMessage,
                                    uint64_t messageId) {
  VstResponse response(responseCode, messageId);
  response.setContentType(req.contentTypeResponse());

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(true));
  builder.add(StaticStrings::ErrorNum, VPackValue(errorNum));
  builder.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
  builder.add(StaticStrings::Code, VPackValue((int)responseCode));
  builder.close();

  try {
    response.setPayload(builder.slice(), true, VPackOptions::Defaults);
    processResponse(&response);
  } catch (...) {
    closeStream();
  }
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
    VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0, true /*isFake*/);
    handleSimpleError(rest::ResponseCode::BAD, fakeRequest,
                      TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, e.what(),
                      chunkHeader._messageID);
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VstCommTask: "
        << "VPack Validation failed: " << e.what();
    closeTask(rest::ResponseCode::BAD);
    return false;
  } catch (...) {
    VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0, true /*isFake*/);
    handleSimpleError(rest::ResponseCode::BAD, fakeRequest, chunkHeader._messageID);
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
        VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0, true /*isFake*/);
        handleSimpleError(rest::ResponseCode::BAD, fakeRequest,
                          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, e.what(),
                          chunkHeader._messageID);
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "VstCommTask: "
            << "VPack Validation failed: " << e.what();
        closeTask(rest::ResponseCode::BAD);
        return false;
      } catch (...) {
        VstRequest fakeRequest( _connectionInfo, VstInputMessage{}, 0, true /*isFake*/);
        handleSimpleError(rest::ResponseCode::BAD, fakeRequest, chunkHeader._messageID);
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
