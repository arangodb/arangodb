////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
#ifndef ARANGO_CXX_DRIVER_VST
#define ARANGO_CXX_DRIVER_VST

#include <chrono>
#include <string>

#include <fuerte/asio_ns.h>
#include <fuerte/message.h>
#include <fuerte/types.h>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

using MessageID = uint64_t;

static size_t const bufferLength = 4096UL;
// static size_t const chunkMaxBytes = 1000UL;
static size_t const minChunkHeaderSize = 16;
static size_t const maxChunkHeaderSize = 24;
static size_t const defaultMaxChunkSize = 1024 * 30;

/////////////////////////////////////////////////////////////////////////////////////
// DataStructures
/////////////////////////////////////////////////////////////////////////////////////

// Velocystream Chunk Header
struct ChunkHeader {
  
  // data used in the specification
  uint32_t _chunkLength;    // length of chunk content (including chunkHeader)
  uint32_t _chunkX;         // number of chunks or chunk number
  uint64_t _messageID;      // messageid
  uint64_t _messageLength;  // length of total payload
  
  // Return length of this chunk (in host byte order)
  inline uint32_t chunkLength() const { return _chunkLength; }
  // Return message ID of this chunk (in host byte order)
  inline uint64_t messageID() const { return _messageID; }
  // Return total message length (in host byte order)
  // for VST1.0 only known in first chunk, 0 otherwise
  inline uint64_t messageLength() const { return _messageLength; }
  // isFirst returns true when the "first chunk" flag has been set.
  inline bool isFirst() const { return ((_chunkX & 0x01) == 1); }
  // index returns the index of this chunk in the message.
  inline uint32_t index() const { return isFirst() ? 0 : _chunkX >> 1; }
  // numberOfChunks return the number of chunks that make up the entire message.
  // This function is only valid for first chunks.
  inline uint32_t numberOfChunks() const {
    if (isFirst()) {
      return _chunkX >> 1;
    }
    assert(false);  // illegal call
    return 0;       // Not known
  }

  // writeHeaderToVST1_0 writes the chunk to the given buffer in VST 1.0 format.
  // The length of the buffer is returned.
  size_t writeHeaderToVST1_0(size_t chunkDataLen, velocypack::Buffer<uint8_t>&) const;

  // writeHeaderToVST1_1 writes the chunk to the given buffer in VST 1.1 format.
  // The length of the buffer is returned.
  size_t writeHeaderToVST1_1(size_t chunkDataLen, velocypack::Buffer<uint8_t>& buffer) const;
};
  
struct Chunk {
  ChunkHeader header;
  asio_ns::const_buffer body;
};
  
namespace message {
  
/// @brief creates a slice containing a VST request-message header.
void requestHeader(RequestHeader const&, velocypack::Buffer<uint8_t>&);
/// @brief creates a slice containing a VST response-message header.
void responseHeader(ResponseHeader const&, velocypack::Buffer<uint8_t>&);
/// @brief creates a slice containing a VST auth message with JWT encryption
void authJWT(std::string const& token, velocypack::Buffer<uint8_t>&);
/// @brief creates a slice containing a VST auth message with plain encryption
void authBasic(std::string const& username,
               std::string const& password,
               velocypack::Buffer<uint8_t>&);

/// @brief take existing buffers and partitions into chunks
/// @param buffer is containing the metadata. If non-empty this will be used
///        as a prefix to the payload.
/// @param payload the payload that is going to be partitioned
void prepareForNetwork(VSTVersion vstVersion,
                       MessageID messageId,
                       velocypack::Buffer<uint8_t>& buffer,
                       asio_ns::const_buffer payload,
                       std::vector<asio_ns::const_buffer>& result);
}

/////////////////////////////////////////////////////////////////////////////////////
// parse vst
/////////////////////////////////////////////////////////////////////////////////////

namespace parser {
  
enum class ChunkState : char{
  Invalid = 0,
  Incomplete = 1,
  Complete = 2,
};

/// @brief readChunkVST1_0 reads a chunk header in VST1.0 format.
ChunkState readChunkVST1_0(Chunk& chunk, uint8_t const*, std::size_t avail);

/// @brief readChunkVST1_1 reads a chunk header in VST1.1 format.
/// @return true if chunk is complete
ChunkState readChunkVST1_1(Chunk& chunk, uint8_t const*, std::size_t avail);

/// @brief verifies header input and checks correct length
/// @return message type or MessageType::Undefined on an error
MessageType validateAndExtractMessageType(uint8_t const* const vpStart,
                                          size_t length, size_t& hLength);

/// creates a RequestHeader from a given slice
RequestHeader requestHeaderFromSlice(velocypack::Slice const& header);
/// creates a RequestHeader from a given slice
ResponseHeader responseHeaderFromSlice(velocypack::Slice const& header);

// Validates if payload consists of valid velocypack slices
std::size_t validateAndCount(uint8_t const* vpHeaderStart, std::size_t len);

}  // namespace parser
}}}}  // namespace arangodb::fuerte::v1::vst
#endif
