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
#include "Meta/conversion.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Rest/CommonDefines.h"
#include "Logger/LoggerFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/ticks.h"

#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::size_t validateAndCount(char const* vpHeaderStart, char const* vpEnd) {
  try {
    VPackValidator validator;
    // check for slice start to the end of Chunk
    // isSubPart allows the slice to be shorter than the checked buffer.
    validator.validate(vpHeaderStart, std::distance(vpHeaderStart, vpEnd),
                       /*isSubPart =*/true);

    VPackSlice vpHeader(vpHeaderStart);
    auto vpPayloadStart = vpHeaderStart + vpHeader.byteSize();

    std::size_t numPayloads = 0;
    while (vpPayloadStart != vpEnd) {
      // validate
      validator.validate(vpPayloadStart, std::distance(vpPayloadStart, vpEnd),
                         true);
      // get offset to next
      VPackSlice tmp(vpPayloadStart);
      vpPayloadStart += tmp.byteSize();
      numPayloads++;
    }
    return numPayloads;
  } catch (std::exception const& e) {
    throw std::runtime_error(
        std::string("error during validation of incoming VPack") + e.what());
  }
}

template <typename T>
std::size_t appendToBuffer(StringBuffer* buffer, T& value) {
  constexpr std::size_t len = sizeof(T);
  char charArray[len];
  char const* charPtr = charArray;
  std::memcpy(&charArray, &value, len);
  buffer->appendText(charPtr, len);
  return len;
}

std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
    std::vector<VPackSlice> const& slices, bool isFirstChunk, uint32_t chunk,
    uint64_t id, uint32_t totalMessageLength = 0) {
  using basics::StringBuffer;
  bool firstOfMany = false;

  // if we have more than one chunk and the chunk is the first
  // then we are sending the first in a series. if this condition
  // is true we also send extra 8 bytes for the messageLength
  // (length of all VPackData)
  if (isFirstChunk && chunk > 1) {
    firstOfMany = true;
  }

  // build chunkX -- see VelocyStream Documentaion
  chunk <<= 1;
  chunk |= isFirstChunk ? 0x1 : 0x0;

  // get the lenght of VPack data
  uint32_t dataLength = 0;
  for (auto& slice : slices) {
    // TODO: is a 32bit value sufficient for all Slices here?
    dataLength += static_cast<uint32_t>(slice.byteSize());
  }

  // calculate length of current chunk
  uint32_t chunkLength = dataLength;
  chunkLength += (sizeof(chunkLength) + sizeof(chunk) + sizeof(id));
  if (firstOfMany) {
    chunkLength += sizeof(totalMessageLength);
  }

  auto buffer =
      std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE, chunkLength, false);

  appendToBuffer(buffer.get(), chunkLength);
  appendToBuffer(buffer.get(), chunk);
  appendToBuffer(buffer.get(), id);

  if (firstOfMany) {
    appendToBuffer(buffer.get(), totalMessageLength);
  }

  // append data in slices
  for (auto const& slice : slices) {
    buffer->appendText(std::string(slice.startAs<char>(), slice.byteSize()));
  }

  return buffer;
}

std::unique_ptr<basics::StringBuffer> createChunkForNetworkSingle(
    std::vector<VPackSlice> const& slices, uint64_t id) {
  return createChunkForNetworkDetail(slices, true, 1, id, 0 /*unused*/);
}

// TODO FIXME make use of these functions
// std::unique_ptr<basics::StringBuffer> createChunkForNetworkMultiFirst(
//     std::vector<VPackSlice> const& slices, uint64_t id, uint32_t
//     numberOfChunks,
//     uint32_t totalMessageLength) {
//   return createChunkForNetworkDetail(slices, true, numberOfChunks, id,
//                                      totalMessageLength);
// }
//
// std::unique_ptr<basics::StringBuffer> createChunkForNetworkMultiFollow(
//     std::vector<VPackSlice> const& slices, uint64_t id, uint32_t chunkNumber,
//     uint32_t totalMessageLength) {
//   return createChunkForNetworkDetail(slices, false, chunkNumber, id, 0);
// }
}

VppCommTask::VppCommTask(GeneralServer* server, TRI_socket_t sock,
                         ConnectionInfo&& info, double timeout)
    : Task("VppCommTask"),
      GeneralCommTask(server, sock, std::move(info), timeout),
      _headerOptions(VPackOptions::Defaults) {
  _protocol = "vpp";
  _readBuffer->reserve(
      _bufferLength);  // ATTENTION <- this is required so we do not
                       // loose information during a resize
                       // connectionStatisticsAgentSetVpp();
  _headerOptions.attributeTranslator =
      basics::VelocyPackHelper::getHeaderTranslator();
}

void VppCommTask::addResponse(VppResponse* response) {
  VPackMessageNoOwnBuffer response_message = response->prepareForNetwork();
  uint64_t& id = response_message._id;

  std::vector<VPackSlice> slices;
  slices.push_back(response_message._header);

  VPackBuilder builder;
  if (response_message._generateBody) {
    builder = basics::VelocyPackHelper::sanitizeExternalsChecked(
        response_message._payload);
    slices.push_back(builder.slice());
  }

  // FIXME (obi)
  // If the message is big we will create many small chunks in a loop.
  // For the first tests we just send single Messages

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "got response:";
  for (auto const& slice : slices) {
    try {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << slice.toJson();
    } catch (arangodb::velocypack::Exception const& e) {
      std::cout << e.what() << std::endl;
    }
  }

  // adds chunk header infromation and creates SingBuffer* that can be
  // used with _writeBuffers
  auto buffer = createChunkForNetworkSingle(slices, id);

  addWriteBuffer(std::move(buffer));
}

VppCommTask::ChunkHeader VppCommTask::readChunkHeader() {
  VppCommTask::ChunkHeader header;

  auto cursor = _processReadVariables._readBufferCursor;

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

  header._headerLength =
      std::distance(_processReadVariables._readBufferCursor, cursor);

  return header;
}

bool VppCommTask::isChunkComplete(char* start) {
  std::size_t length = std::distance(start, _readBuffer->end());
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
  // TODO FIXME
  // - in case of error send an operation failed to all incomplete messages /
  //   operation and close connection (implement resetState/resetCommtask)

  auto& prv = _processReadVariables;
  if (!prv._readBufferCursor) {
    prv._readBufferCursor = _readBuffer->begin();
  }

  auto chunkBegin = prv._readBufferCursor;
  if (chunkBegin == nullptr || !isChunkComplete(chunkBegin)) {
    return false;  // no data or incomplete
  }

  ChunkHeader chunkHeader = readChunkHeader();
  auto chunkEnd = chunkBegin + chunkHeader._chunkLength;
  auto vpackBegin = chunkBegin + chunkHeader._headerLength;
  bool doExecute = false;
  bool read_maybe_only_part_of_buffer = false;
  VppInputMessage message;  // filled in CASE 1 or CASE 2b

  // CASE 1: message is in one chunk
  if (chunkHeader._isFirst && chunkHeader._chunk == 1) {
    std::size_t payloads = 0;

    try {
      payloads = validateAndCount(vpackBegin, chunkEnd);
    } catch (std::exception const& e) {
      handleSimpleError(rest::ResponseCode::BAD,
                        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, e.what(),
                        chunkHeader._messageID);
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VPack Validation failed!";
      closeTask(rest::ResponseCode::BAD);
      return false;
    } catch (...) {
      handleSimpleError(rest::ResponseCode::BAD, chunkHeader._messageID);
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VPack Validation failed!";
      closeTask(rest::ResponseCode::BAD);
      return false;
    }

    VPackBuffer<uint8_t> buffer;
    buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    message.set(chunkHeader._messageID, std::move(buffer), payloads);  // fixme

    // message._header = VPackSlice(message._buffer.data());
    // if (payloadOffset) {
    //  message._payload = VPackSlice(message._buffer.data() + payloadOffset);
    // }

    doExecute = true;
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "CASE 1";
  }
  // CASE 2:  message is in multiple chunks
  auto incompleteMessageItr = _incompleteMessages.find(chunkHeader._messageID);

  // CASE 2a: chunk starts new message
  if (chunkHeader._isFirst) {  // first chunk of multi chunk message
    if (incompleteMessageItr != _incompleteMessages.end()) {
      throw std::logic_error(
          "Message should be first but is already in the Map of incomplete "
          "messages");
    }

    // TODO: is a 32bit value sufficient for the messageLength here?
    IncompleteVPackMessage message(
        static_cast<uint32_t>(chunkHeader._messageLength),
        chunkHeader._chunk /*number of chunks*/);
    message._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    auto insertPair = _incompleteMessages.emplace(
        std::make_pair(chunkHeader._messageID, std::move(message)));
    if (!insertPair.second) {
      throw std::logic_error("insert failed");
      closeTask();
    }

    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "CASE 2a";

    // CASE 2b: chunk continues a message
  } else {  // followup chunk of some mesage
    if (incompleteMessageItr == _incompleteMessages.end()) {
      throw std::logic_error("found message without previous part");
      closeTask();
    }
    auto& im = incompleteMessageItr->second;  // incomplete Message
    im._currentChunk++;
    assert(im._currentChunk == chunkHeader._chunk);
    im._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    // check buffer longer than length

    // MESSAGE COMPLETE
    if (im._currentChunk == im._numberOfChunks - 1 /* zero based counting */) {
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
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VPack Validation failed!";
        closeTask(rest::ResponseCode::BAD);
        return false;
      } catch (...) {
        handleSimpleError(rest::ResponseCode::BAD, chunkHeader._messageID);
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "VPack Validation failed!";
        closeTask(rest::ResponseCode::BAD);
        return false;
      }

      message.set(chunkHeader._messageID, std::move(im._buffer), payloads);
      _incompleteMessages.erase(incompleteMessageItr);
      // check length

      doExecute = true;
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "CASE 2b - complete";
    }
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "CASE 2b - still incomplete";
  }

  read_maybe_only_part_of_buffer = true;
  prv._currentChunkLength =
      0;  // if we end up here we have read a complete chunk
  prv._readBufferCursor = chunkEnd;
  std::size_t processedDataLen =
      std::distance(_readBuffer->begin(), prv._readBufferCursor);
  // clean buffer up to length of chunk
  if (processedDataLen > prv._cleanupLength) {
    _readBuffer->move_front(processedDataLen);
    prv._readBufferCursor = nullptr;  // the positon will be set at the
                                      // begin of this function
  }

  if (doExecute) {
    VPackSlice header = message.header();
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "got request:" << header.toJson(&_headerOptions);
    int type = meta::underlyingValue(rest::RequestType::ILLEGAL);
    try {
      type = header.get("type", &_headerOptions).getInt();
    } catch (std::exception const& e) {
      throw std::runtime_error(
          std::string("Error during Parsing of VppHeader: ") + e.what());
    }
    if (type == 1000) {
      // do auth
    } else {
      // check auth
      // the handler will take ownersip of this pointer
      std::unique_ptr<VppRequest> request(new VppRequest(
          _connectionInfo, std::move(message), chunkHeader._messageID));
      request->setHeaderOptions(&_headerOptions);
      GeneralServerFeature::HANDLER_FACTORY->setRequestContext(request.get());
      // make sure we have a database
      if (request->requestContext() == nullptr) {
        handleSimpleError(rest::ResponseCode::NOT_FOUND,
                          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                          TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND),
                          chunkHeader._messageID);
      } else {
        request->setClientTaskId(_taskId);
        _protocolVersion = request->protocolVersion();

        std::unique_ptr<VppResponse> response(new VppResponse(
            rest::ResponseCode::SERVER_ERROR, chunkHeader._messageID));
        response->setHeaderOptions(&_headerOptions);
        executeRequest(std::move(request), std::move(response));
      }
    }
  }

  if (read_maybe_only_part_of_buffer) {
    if (prv._readBufferCursor == _readBuffer->end()) {
      return false;
    }
    return true;
  }
  return doExecute;
}

void VppCommTask::closeTask(rest::ResponseCode code) {
  _processReadVariables._readBufferCursor = nullptr;
  _processReadVariables._currentChunkLength = 0;
  _readBuffer->clear();  // check is this changes the reserved size

  // is there a case where we do not want to close the connection
  for (auto const& message : _incompleteMessages) {
    handleSimpleError(code, message.first);
  }
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
