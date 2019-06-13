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

// Item that represents a Request in flight
struct RequestItem {
  /// ID of this message
  MessageID _messageID;
  /// Reference to the request we're processing
  std::unique_ptr<Request> _request;
  /// Callback for when request is done (in error or succeeded)
  RequestCallback _callback;
  /// point in time when the message expires
  std::chrono::steady_clock::time_point _expires;
  
  // ======= Request variables =======
  
  /// Buffer used to hold chunk headers and message header
  velocypack::Buffer<uint8_t> _requestMetadata;
  
  /// Temporary list of buffers goin to be send by the socket.
  std::vector<asio_ns::const_buffer> _requestBuffers;
  
  // ======= Response variables =======
  
  /// @brief List of chunks that have been received.
  std::vector<ChunkHeader> _responseChunks;
  /// Buffer containing content of received chunks.
  /// Not necessarily in a sorted order!
  velocypack::Buffer<uint8_t> _responseChunkContent;
  /// The number of chunks we're expecting (0==not know yet).
  size_t _responseNumberOfChunks;
  
 public:
  
  inline MessageID messageID() { return _messageID; }
  inline void invokeOnError(Error e) {
    _callback(e, std::move(_request), nullptr);
  }

  /// prepareForNetwork prepares the internal structures for
  /// writing the request to the network.
  void prepareForNetwork(VSTVersion);
  
  // prepare structures with a given message header and payload
  void prepareForNetwork(VSTVersion,
                         asio_ns::const_buffer header,
                         asio_ns::const_buffer payload);

  // add the given chunk to the list of response chunks.
  void addChunk(ChunkHeader&& chunk,
                asio_ns::const_buffer const& data);
  // try to assembly the received chunks into a response.
  // returns NULL if not all chunks are available.
  std::unique_ptr<velocypack::Buffer<uint8_t>> assemble();

  // Flush all memory needed for sending this request.
  inline void resetSendData() {
    _requestMetadata.clear();
    _requestBuffers.clear();
  }
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
