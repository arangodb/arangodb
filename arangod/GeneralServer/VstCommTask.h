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

#ifndef ARANGOD_GENERAL_SERVER_VST_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_VST_COMM_TASK_H 1

#include "Basics/Common.h"

#include "GeneralServer/GeneralCommTask.h"
#include "lib/Rest/VstRequest.h"
#include "lib/Rest/VstResponse.h"

#include <boost/lockfree/queue.hpp>
#include <fuerte/detail/vst.h>

namespace arangodb {
namespace rest {
  
template <SocketType T>
class VstCommTask final : public GeneralCommTask<T> {
 public:
  VstCommTask(GeneralServer& server,
              ConnectionInfo,
              std::unique_ptr<AsioSocket<T>> socket,
              fuerte::vst::VSTVersion v);

  bool allowDirectHandling() const override { return false; }

 protected:

  std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                  uint64_t messageId) override final;

  // @brief send simple response including response body
  void addSimpleResponse(rest::ResponseCode, rest::ContentType, uint64_t messageId,
                         velocypack::Buffer<uint8_t>&&) override;

  // convert from GeneralResponse to VstResponse ad dispatch request to class
  // internal addResponse
  void sendResponse(std::unique_ptr<GeneralResponse>, RequestStatistics*) override;

  bool readCallback(asio_ns::error_code ec) override;
  

 private:
  
  // Process the given incoming chunk.
  bool processChunk(fuerte::vst::Chunk const& chunk);
  /// process a VST message
  bool processMessage(velocypack::Buffer<uint8_t>, uint64_t messageId);
  
  void doWrite();
  
  // process the VST 1000 request type
  void handleAuthHeader(velocypack::Slice header, uint64_t messageId);

 private:
  using MessageID = uint64_t;

  struct Message {
    /// used to index chunks in _buffer
    struct ChunkInfo {
      uint32_t index; /// chunk index
      size_t offset;  /// offset into buffer
      size_t size;  /// content length
    };
    
    velocypack::Buffer<uint8_t> buffer;
    /// @brief List of chunks that have been received.
    std::vector<ChunkInfo> chunks;
    std::size_t expectedChunks = 0;
    
    /// add chunk to this message
    void addChunk(fuerte::vst::Chunk const& chunk);
    /// assemble message, if true result is in _buffer
    bool assemble();
  };
  
  struct ResponseItem {
    velocypack::Buffer<uint8_t> metadata;
    std::unique_ptr<GeneralResponse> response;
    std::vector<asio_ns::const_buffer> buffers;
  };
  static constexpr size_t maxChunkSize = 30 * 1024;
  
 private:
  
  std::map<uint64_t, std::unique_ptr<Message>> _messages;
  boost::lockfree::queue<ResponseItem*, boost::lockfree::capacity<4096>> _writeQueue;
  std::atomic<bool> _writing; /// is writing
  
  /// Is the current user authorized
  bool _authorized;
  rest::AuthenticationMethod _authMethod;
  fuerte::vst::VSTVersion _vstVersion;
};
}  // namespace rest
}  // namespace arangodb

#endif
