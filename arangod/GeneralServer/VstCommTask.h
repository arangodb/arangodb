////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Rest/VstRequest.h"
#include "Rest/VstResponse.h"

#include <fuerte/detail/vst.h>
#include <boost/lockfree/queue.hpp>

namespace arangodb {
namespace rest {

template <SocketType T>
class VstCommTask final : public GeneralCommTask<T> {
 public:
  VstCommTask(GeneralServer& server, ConnectionInfo,
              std::unique_ptr<AsioSocket<T>> socket, fuerte::vst::VSTVersion v);
  ~VstCommTask();

 protected:
  virtual void start() override;

  virtual bool readCallback(asio_ns::error_code ec) override;

  /// set / reset connection timeout
  virtual void setIOTimeout() override;

  // convert from GeneralResponse to VstResponse ad dispatch request to class
  // internal addResponse
  virtual void sendResponse(std::unique_ptr<GeneralResponse>, RequestStatistics::Item) override;

  virtual std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                          uint64_t messageId) override;

 private:
  // Process the given incoming chunk.
  bool processChunk(fuerte::vst::Chunk const& chunk);
  /// process a VST message
  void processMessage(velocypack::Buffer<uint8_t>, uint64_t messageId);

  void doWrite();

  // process the VST 1000 request type
  void handleVstAuthRequest(velocypack::Slice header, uint64_t messageId);

 private:
  using MessageID = uint64_t;

  struct Message {
    /// used to index chunks in _buffer
    struct ChunkInfo {
      uint32_t index;  /// chunk index
      size_t offset;   /// offset into buffer
      size_t size;     /// content length
    };

    velocypack::Buffer<uint8_t> buffer;
    /// @brief List of chunks that have been received.
    std::vector<ChunkInfo> chunks;
    std::size_t expectedChunks = 0;
    std::size_t expectedMsgSize = 0;

    /// @brief add chunk to this message
    /// @return false if the message size is too big
    bool addChunk(fuerte::vst::Chunk const& chunk);
    /// assemble message, if true result is in _buffer
    bool assemble();
  };

  struct ResponseItem {
    velocypack::Buffer<uint8_t> metadata;
    std::vector<asio_ns::const_buffer> buffers;
    std::unique_ptr<GeneralResponse> response;
    RequestStatistics::Item stat;
  };
  /// default max chunksize is 30kb in arangodb in all versions
  static constexpr size_t maxChunkSize = 30 * 1024;

 private:

  std::string url(VstRequest const* req) const;

  std::map<uint64_t, Message> _messages;

  // the queue is dynamically sized because we can't guarantee that
  // only a fixed number of responses are active at the same time.
  // the reason is that producing responses may happen faster than
  // fetching responses from the queue. this is done by different threads
  // and depends on thread scheduling, so it is somewhat random how
  // fast producing and consuming happen.
  // effectively the length of the queue is bounded by the fact that the
  // scheduler queue length is also bounded, so that we will not see
  // an endless growth of the queue in a single connection.
  boost::lockfree::queue<ResponseItem*> _writeQueue;

  std::atomic<bool> _writeLoopActive;  /// is writing
  std::atomic<unsigned> _numProcessing;

  /// Is the current user authenticated (not authorized)
  auth::TokenCache::Entry _authToken;
  rest::AuthenticationMethod _authMethod;
  fuerte::vst::VSTVersion _vstVersion;
};
}  // namespace rest
}  // namespace arangodb

#endif
