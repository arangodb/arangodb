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

#ifndef ARANGOD_GENERAL_SERVER_VST_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_VST_COMM_TASK_H 1

#include "Basics/Common.h"
#include "GeneralServer/GeneralCommTask.h"
#include "lib/Rest/VstMessage.h"
#include "lib/Rest/VstRequest.h"
#include "lib/Rest/VstResponse.h"

#include <stdexcept>

namespace arangodb {

namespace rest {

class VstCommTask final : public GeneralCommTask {
 public:
  VstCommTask(EventLoop, GeneralServer*, std::unique_ptr<Socket> socket,
              ConnectionInfo&&, double timeout, ProtocolVersion protocolVersion,
              bool skipSocketInit = false);

  // convert from GeneralResponse to VstResponse ad dispatch request to class
  // internal addResponse
  void addResponse(GeneralResponse* response, RequestStatistics* stat) override {
    VstResponse* vstResponse = dynamic_cast<VstResponse*>(response);

    if (vstResponse == nullptr) {
      throw std::logic_error("invalid response or response Type");
    }

    addResponse(vstResponse, stat);
  }

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VST;
  }

 protected:
  // read data check if chunk and message are complete
  // if message is complete execute a request
  bool processRead(double startTime) override;

  std::unique_ptr<GeneralResponse> createResponse(
      rest::ResponseCode, uint64_t messageId) override final;

  void handleAuthHeader(VPackSlice const& header, uint64_t messageId);

  void handleSimpleError(rest::ResponseCode code, GeneralRequest const& req, uint64_t id) override {
    VstResponse response(code, id);
    response.setContentType(req.contentTypeResponse());
    addResponse(&response, nullptr);
  }

  void handleSimpleError(rest::ResponseCode, GeneralRequest const&, int code,
                         std::string const& errorMessage,
                         uint64_t messageId) override;

  bool allowDirectHandling() const override final { return false; }

 private:
  // reets the internal state this method can be called to clean up when the
  // request handling aborts prematurely
  void closeTask(rest::ResponseCode code = rest::ResponseCode::SERVER_ERROR);

  void addResponse(VstResponse*, RequestStatistics* stat);

 private:
  using MessageID = uint64_t;

  struct IncompleteVPackMessage {
    IncompleteVPackMessage(uint32_t length, std::size_t numberOfChunks)
        : _length(length),
          _buffer(_length),
          _numberOfChunks(numberOfChunks),
          _currentChunk(0) {}
    uint32_t _length;  // length of total message in bytes
    VPackBuffer<uint8_t> _buffer;
    std::size_t _numberOfChunks;
    std::size_t _currentChunk;
  };
  std::unordered_map<MessageID, IncompleteVPackMessage> _incompleteMessages;

  static size_t const _bufferLength = 4096UL;
  static size_t const _chunkMaxBytes = 1000UL;
  struct ProcessReadVariables {
    ProcessReadVariables()
        : _currentChunkLength(0),
          _readBufferOffset(0),
          _cleanupLength(_bufferLength - _chunkMaxBytes - 1) {}
    uint32_t
        _currentChunkLength;     // size of chunk processed or 0 when expecting
                                 // new chunk
    size_t _readBufferOffset;    // data up to this position has been processed
    std::size_t _cleanupLength;  // length of data after that the read buffer
                                 // will be cleaned
  };
  ProcessReadVariables _processReadVariables;

  struct ChunkHeader {
    std::size_t _headerLength;
    uint32_t _chunkLength;
    uint32_t _chunk;
    uint64_t _messageID;
    uint64_t _messageLength;
    bool _isFirst;
  };
  bool isChunkComplete(char*);    // sub-function of processRead
  ChunkHeader readChunkHeader();  // sub-function of processRead
  void replyToIncompleteMessages();

  // Returns true if and only if there was no error, if false is returned,
  // the connection is closed
  bool getMessageFromSingleChunk(
      ChunkHeader const& chunkHeader, VstInputMessage& message, bool& doExecute,
      char const* vpackBegin, char const* chunkEnd);

  // Returns true if and only if there was no error, if false is returned,
  // the connection is closed
  bool getMessageFromMultiChunks(
      ChunkHeader const& chunkHeader, VstInputMessage& message, bool& doExecute,
      char const* vpackBegin, char const* chunkEnd);

  /// Is the current user authorized
  bool _authorized;
  std::string _authenticatedUser;
  ProtocolVersion _protocolVersion;
  uint32_t _maxChunkSize;
};
}
}

#endif
