////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2019 ArangoDB GmbH, Cologne, Germany
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
#pragma once
#ifndef ARANGO_CXX_DRIVER_VST_REQUEST_ITEM
#define ARANGO_CXX_DRIVER_VST_REQUEST_ITEM

#include <fuerte/detail/vst.h>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

static std::atomic<MessageID> vstMessageId(1);

// Item that represents a Request in flight
struct RequestItem {
  /// Buffer used to store data for request and response
  /// For request holds chunk headers and message header
  /// For responses contains contents of received chunks.
  /// Not necessarily in a sorted order!
  velocypack::Buffer<uint8_t> _buffer;

  /// used to index chunks in _buffer
  struct ChunkInfo {
    uint32_t index;  /// chunk index
    size_t offset;   /// offset into buffer
    size_t size;     /// content length
  };
  /// @brief List of chunks that have been received.
  std::vector<ChunkInfo> _responseChunks;

  /// Callback for when request is done (in error or succeeded)
  RequestCallback _callback;

  /// The number of chunks we're expecting (0==not know yet).
  size_t _responseNumberOfChunks = 0;

  /// ID of this message
  const MessageID _messageID;
  /// Reference to the request we're processing
  std::unique_ptr<Request> request;

  /// point in time when the message expires
  std::chrono::steady_clock::time_point expires;

 public:
  RequestItem(std::unique_ptr<Request>&& req, RequestCallback&& cb)
      : _callback(std::move(cb)),
        _messageID(vstMessageId.fetch_add(1, std::memory_order_relaxed)),
        request(std::move(req)) {}

  inline void invokeOnError(Error e) {
    _callback(e, std::move(request), nullptr);
  }

  /// prepareForNetwork prepares the internal structures for
  /// writing the request to the network.
  std::vector<asio_ns::const_buffer> prepareForNetwork(VSTVersion);

  // add the given chunk to the list of response chunks.
  void addChunk(Chunk const& chunk);
  // try to assembly the received chunks into a response.
  // returns NULL if not all chunks are available.
  std::unique_ptr<velocypack::Buffer<uint8_t>> assemble();

  // Flush all memory needed for sending this request.
  inline void resetSendData() { _buffer.clear(); }
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
