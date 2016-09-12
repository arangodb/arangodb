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
///     vpp://www.apache.org/licenses/LICENSE-2.0
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

#include "VppCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "GeneralServer/VppNetwork.h"
#include "Logger/LoggerFeature.h"
#include "Meta/conversion.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/ticks.h"

#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <boost/optional.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

VppCommTask::VppCommTask(GeneralServer* server, TRI_socket_t sock,
                         ConnectionInfo&& info, double timeout)
    : Task("VppCommTask"),
      GeneralCommTask(server, sock, std::move(info), timeout),
      _authenticatedUser(),
      _authenticationEnabled(
          application_features::ApplicationServer::getFeature<
              GeneralServerFeature>("GeneralServer")
              ->authenticationEnabled()) {
  _protocol = "vpp";
  _readBuffer.reserve(
      _bufferLength);  // ATTENTION <- this is required so we do not
                       // loose information during a resize
                       // connectionStatisticsAgentSetVpp();
  _agents.emplace(std::make_pair(0UL, RequestStatisticsAgent(true)));
  getAgent(0UL)->acquire();
}

void VppCommTask::addResponse(VppResponse* response) {
  VPackMessageNoOwnBuffer response_message = response->prepareForNetwork();
  uint64_t& id = response_message._id;

  std::vector<VPackSlice> slices;
  slices.push_back(response_message._header);

  if (response->generateBody()) {
    for (auto& payload : response_message._payloads) {
      slices.push_back(payload);
    }
  }

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                          << "created response:";
  for (auto const& slice : slices) {
    try {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << slice.toJson();
    } catch (arangodb::velocypack::Exception const& e) {
    }
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "--";
  }
  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "response -- end";

  // adds chunk header infromation and creates SingBuffer* that can be
  // used with _writeBuffers
  auto buffers = createChunkForNetwork(
      slices, id, (std::numeric_limits<std::size_t>::max)(),
      false);  // set some sensible maxchunk
               // size and compression

  double const totalTime = getAgent(id)->elapsedSinceReadStart();

  for (auto&& buffer : buffers) {
    // is the multiple getAgent ok? REVIEW (fc)
    addWriteBuffer(std::move(buffer), getAgent(id));
  }

  // and give some request information
  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"vpp-request-end\",\"" << (void*)this << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << VppRequest::translateVersion(_protocolVersion) << "\","
      << static_cast<int>(response->responseCode()) << ","
      << "\"," << Logger::FIXED(totalTime, 6);

  if (id) {
    _agents.erase(id);
  } else {
    getAgent(0UL)->acquire();
  }
}

VppCommTask::ChunkHeader VppCommTask::readChunkHeader() {
  VppCommTask::ChunkHeader header;

  auto cursor = _readBuffer.begin() + _processReadVariables._readBufferOffset;

  std::memcpy(&header._chunkLength, cursor, sizeof(header._chunkLength));
  cursor += sizeof(header._chunkLength);

  uint32_t chunkX;
  std::memcpy(&chunkX, cursor, sizeof(chunkX));
  cursor += sizeof(chunkX);

  header._isFirst = chunkX & 0x1;
  header._chunk = chunkX >> 1;

  std::memcpy(&header._messageID, cursor, sizeof(header._messageID));
  cursor += sizeof(header._messageID);

  // extract total len of message
  if (header._isFirst && header._chunk > 1) {
    std::memcpy(&header._messageLength, cursor, sizeof(header._messageLength));
    cursor += sizeof(header._messageLength);
  } else {
    header._messageLength = 0;  // not needed
  }

  header._headerLength = std::distance(
      _readBuffer.begin() + _processReadVariables._readBufferOffset, cursor);

  return header;
}

bool VppCommTask::isChunkComplete(char* start) {
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

// reads data from the socket
bool VppCommTask::processRead() {
  RequestStatisticsAgent agent(true);

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
  VppInputMessage message;  // filled in CASE 1 or CASE 2b

  if (chunkHeader._isFirst && chunkHeader._chunk == 1) {
    // CASE 1: message is in one chunk
    if (auto rv = getMessageFromSingleChunk(chunkHeader, message, doExecute,
                                            vpackBegin, chunkEnd)) {
      return *rv;
    }
  } else {
    if (auto rv = getMessageFromMultiChunks(chunkHeader, message, doExecute,
                                            vpackBegin, chunkEnd)) {
      return *rv;
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

    LOG_TOPIC(DEBUG, Logger::REQUESTS) << "\"vpp-request-header\",\""
                                       << "\"," << message.header().toJson()
                                       << "\"";

    LOG_TOPIC(DEBUG, Logger::REQUESTS) << "\"vpp-request-payload\",\""
                                       << "\"," << message.payload().toJson()
                                       << "\"";

    // get type of request
    int type = meta::underlyingValue(rest::RequestType::ILLEGAL);
    try {
      type = header.at(1).getInt();
    } catch (std::exception const& e) {
      handleSimpleError(rest::ResponseCode::BAD, chunkHeader._messageID);
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VppCommTask: "
          << std::string("VPack Validation failed!") + e.what();
      closeTask(rest::ResponseCode::BAD);
      return false;
    }

    // handle request types
    if (type == 1000) {
      // do authentication
      // std::string encryption = header.at(2).copyString();
      std::string user = header.at(3).copyString();
      std::string pass = header.at(4).copyString();
      auto auth = basics::StringUtils::encodeBase64(user + ":" + pass);
      AuthResult result = GeneralServerFeature::AUTH_INFO.checkAuthentication(
          AuthInfo::AuthType::BASIC, auth);

      if (!_authenticationEnabled || result._authorized) {
        _authenticatedUser = std::move(user);
        handleSimpleError(rest::ResponseCode::OK, TRI_ERROR_NO_ERROR,
                          "authentication successful", chunkHeader._messageID);
      } else {
        _authenticatedUser.clear();
        handleSimpleError(rest::ResponseCode::UNAUTHORIZED,
                          TRI_ERROR_HTTP_UNAUTHORIZED, "authentication failed",
                          chunkHeader._messageID);
      }
    } else {
      // the handler will take ownersip of this pointer
      std::unique_ptr<VppRequest> request(new VppRequest(
          _connectionInfo, std::move(message), chunkHeader._messageID));
      GeneralServerFeature::HANDLER_FACTORY->setRequestContext(request.get());
      request->setUser(_authenticatedUser);

      // check authentication
      std::string const& dbname = request->databaseName();
      AuthLevel level = AuthLevel::RW;
      if (_authenticationEnabled &&
          (!_authenticatedUser.empty() || !dbname.empty())) {
        level = GeneralServerFeature::AUTH_INFO.canUseDatabase(
            _authenticatedUser, dbname);
      }

      if (level != AuthLevel::RW) {
        handleSimpleError(rest::ResponseCode::UNAUTHORIZED, TRI_ERROR_FORBIDDEN,
                          "not authorized to execute this request",
                          chunkHeader._messageID);
      } else {
        // now that we are authorized we do the request
        // make sure we have a database
        if (request->requestContext() == nullptr) {
          handleSimpleError(
              rest::ResponseCode::NOT_FOUND,
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND),
              chunkHeader._messageID);
        } else {
          request->setClientTaskId(_taskId);
          _protocolVersion = request->protocolVersion();

          std::unique_ptr<VppResponse> response(new VppResponse(
              rest::ResponseCode::SERVER_ERROR, chunkHeader._messageID));
          response->setContentTypeRequested(request->contentTypeResponse());
          executeRequest(std::move(request), std::move(response));
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

void VppCommTask::closeTask(rest::ResponseCode code) {
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
  _clientClosed = true;
}

rest::ResponseCode VppCommTask::authenticateRequest(GeneralRequest* request) {
  auto context = (request == nullptr) ? nullptr : request->requestContext();

  if (context == nullptr && request != nullptr) {
    bool res =
        GeneralServerFeature::HANDLER_FACTORY->setRequestContext(request);

    if (!res) {
      return rest::ResponseCode::NOT_FOUND;
    }
    context = request->requestContext();
  }

  if (context == nullptr) {
    return rest::ResponseCode::SERVER_ERROR;
  }
  return context->authenticate();
}

std::unique_ptr<GeneralResponse> VppCommTask::createResponse(
    rest::ResponseCode responseCode, uint64_t messageId) {
  return std::unique_ptr<GeneralResponse>(
      new VppResponse(responseCode, messageId));
}

void VppCommTask::handleSimpleError(rest::ResponseCode responseCode,
                                    int errorNum,
                                    std::string const& errorMessage,
                                    uint64_t messageId) {
  VppResponse response(responseCode, messageId);

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
    _clientClosed = true;
  }
}

boost::optional<bool> VppCommTask::getMessageFromSingleChunk(
    ChunkHeader const& chunkHeader, VppInputMessage& message, bool& doExecute,
    char const* vpackBegin, char const* chunkEnd) {
  // add agent for this new message
  _agents.emplace(
      std::make_pair(chunkHeader._messageID, RequestStatisticsAgent(true)));

  auto agent = getAgent(chunkHeader._messageID);
  agent->acquire();
  agent->requestStatisticsAgentSetReadStart();

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                          << "chunk contains single message";
  std::size_t payloads = 0;

  try {
    payloads = validateAndCount(vpackBegin, chunkEnd);
  } catch (std::exception const& e) {
    handleSimpleError(rest::ResponseCode::BAD,
                      TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, e.what(),
                      chunkHeader._messageID);
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                            << "VPack Validation failed!"
                                            << e.what();
    closeTask(rest::ResponseCode::BAD);
    return false;
  } catch (...) {
    handleSimpleError(rest::ResponseCode::BAD, chunkHeader._messageID);
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                            << "VPack Validation failed!";
    closeTask(rest::ResponseCode::BAD);
    return false;
  }

  VPackBuffer<uint8_t> buffer;
  buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
  message.set(chunkHeader._messageID, std::move(buffer), payloads);  // fixme

  doExecute = true;
  getAgent(chunkHeader._messageID)->requestStatisticsAgentSetReadEnd();
  return boost::none;
}

boost::optional<bool> VppCommTask::getMessageFromMultiChunks(
    ChunkHeader const& chunkHeader, VppInputMessage& message, bool& doExecute,
    char const* vpackBegin, char const* chunkEnd) {
  // CASE 2:  message is in multiple chunks
  auto incompleteMessageItr = _incompleteMessages.find(chunkHeader._messageID);

  // CASE 2a: chunk starts new message
  if (chunkHeader._isFirst) {  // first chunk of multi chunk message
    // add agent for this new message
    _agents.emplace(
        std::make_pair(chunkHeader._messageID, RequestStatisticsAgent(true)));

    auto agent = getAgent(chunkHeader._messageID);
    agent->acquire();
    agent->requestStatisticsAgentSetReadStart();

    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                            << "chunk starts a new message";
    if (incompleteMessageItr != _incompleteMessages.end()) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VppCommTask: "
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
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                              << "insert failed";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }

    // CASE 2b: chunk continues a message
  } else {  // followup chunk of some mesage
    // do not add agent for this continued message
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                            << "chunk continues a message";
    if (incompleteMessageItr == _incompleteMessages.end()) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "VppCommTask: "
          << "found message without previous part";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }
    auto& im = incompleteMessageItr->second;  // incomplete Message
    im._currentChunk++;
    assert(im._currentChunk == chunkHeader._chunk);
    im._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    // check buffer longer than length

    // MESSAGE COMPLETE
    if (im._currentChunk == im._numberOfChunks - 1 /* zero based counting */) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                              << "chunk completes a message";
      std::size_t payloads = 0;

      try {
        payloads =
            validateAndCount(reinterpret_cast<char const*>(im._buffer.data()),
                             reinterpret_cast<char const*>(
                                 im._buffer.data() + im._buffer.byteSize()));

      } catch (std::exception const& e) {
        handleSimpleError(rest::ResponseCode::BAD,
                          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, e.what(),
                          chunkHeader._messageID);
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                                << "VPack Validation failed!"
                                                << e.what();
        closeTask(rest::ResponseCode::BAD);
        return false;
      } catch (...) {
        handleSimpleError(rest::ResponseCode::BAD, chunkHeader._messageID);
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VppCommTask: "
                                                << "VPack Validation failed!";
        closeTask(rest::ResponseCode::BAD);
        return false;
      }

      message.set(chunkHeader._messageID, std::move(im._buffer), payloads);
      _incompleteMessages.erase(incompleteMessageItr);
      // check length

      doExecute = true;
      getAgent(chunkHeader._messageID)->requestStatisticsAgentSetReadEnd();
    }
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VppCommTask: "
        << "chunk does not complete a message";
  }
  return boost::none;
}
