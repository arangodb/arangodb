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

// TODO -- error handling

#ifndef ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"
#include "lib/Rest/VppMessage.h"
#include "lib/Rest/VppResponse.h"
#include "lib/Rest/VppRequest.h"

#include <stdexcept>

namespace arangodb {
namespace rest {

class VppCommTask : public GeneralCommTask {
 public:
  VppCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&, double timeout);

  // read data check if chunk and message are complete
  // if message is complete execute a request
  bool processRead() override;

  // convert from GeneralResponse to vppResponse ad dispatch request to class
  // internal addResponse
  void addResponse(GeneralResponse* response, bool isError) override {
    VppResponse* vppResponse = dynamic_cast<VppResponse*>(response);
    if (vppResponse == nullptr) {
      throw std::logic_error("invalid response or response Type");
    }
    addResponse(vppResponse, isError);
  };

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VPP;
  };

 protected:
  void completedWriteBuffer() override final;
  virtual void handleChunk(char const*, size_t) {}

  void handleSimpleError(GeneralResponse::ResponseCode code,
                         uint64_t id) override {
    VppResponse response(code, id);
    addResponse(&response, true);
  }
  void handleSimpleError(GeneralResponse::ResponseCode, int code,
                         std::string const& errorMessage) override;

 private:
  // resets the internal state this method can be called to clean up when the
  // request handling aborts prematurely
  void resetState(bool close);
  void replyToIncompleteMessages() {}

  void addResponse(VppResponse*, bool isError);

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
          _readBufferCursor(nullptr),
          _cleanupLength(_bufferLength - _chunkMaxBytes - 1) {}
    uint32_t
        _currentChunkLength;     // size of chunk processed or 0 when expecting
                                 // new chunk
    char* _readBufferCursor;     // data up to this position has been processed
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

  // user
  // authenticated or not
  // database aus url
};
}
}

#endif
