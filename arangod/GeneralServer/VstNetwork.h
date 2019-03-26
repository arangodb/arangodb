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

#ifndef ARANGOD_VST_NETWORK_H
#define ARANGOD_VST_NETWORK_H 1

#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LoggerFeature.h"

#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>
#include <stdexcept>

namespace arangodb {

template <typename T>
void appendLittleEndian(basics::StringBuffer* buffer, T v) {
  for (size_t i = 0; i < sizeof(T); ++i) {
    buffer->appendChar(static_cast<char>(v & 0xffu));
    v >>= 8;
  }
}

inline constexpr std::size_t chunkHeaderLength(bool sendTotalLen) {
  // chunkLength uint32 , chunkX uint32 , id uint64 , messageLength uint64
  return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) +
         (sendTotalLen ? sizeof(uint64_t) : 0);
}

// Send Message Created from Slices

// working version of single chunk message creation
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
    std::vector<VPackSlice> const& slices, bool isFirstChunk, uint32_t chunk,
    uint64_t id, ProtocolVersion protocolVersion, uint64_t totalMessageLength) {
  using basics::StringBuffer;

  bool sendTotalLen =
      protocolVersion != ProtocolVersion::VST_1_0 || (isFirstChunk && chunk > 1);
  // if we speak VST_1_0 and have more than one chunk and the chunk
  // is the first then we are sending the first in a series. if this
  // condition is true we also send extra 8 bytes for the messageLength
  // (length of all VPackData). In later versions we always send the
  // total length.

  // build chunkX -- see VelocyStream Documentaion
  chunk <<= 1;
  chunk |= isFirstChunk ? 0x1 : 0x0;

  // get the length of VPack data
  uint32_t dataLength = 0;
  for (auto& slice : slices) {
    // TODO: is a 32bit value sufficient for all Slices here?
    dataLength += static_cast<uint32_t>(slice.byteSize());
  }

  // calculate length of current chunk
  uint32_t chunkLength =
      dataLength + static_cast<uint32_t>(chunkHeaderLength(sendTotalLen));

  auto buffer = std::make_unique<StringBuffer>(chunkLength, false);

  LOG_TOPIC("5e49b", TRACE, Logger::COMMUNICATION) << "chunkLength: " << chunkLength;
  appendLittleEndian(buffer.get(), chunkLength);
  appendLittleEndian(buffer.get(), chunk);
  appendLittleEndian(buffer.get(), id);

  if (sendTotalLen) {
    appendLittleEndian(buffer.get(), totalMessageLength);
  }

  // append data in slices
  for (auto const& slice : slices) {
    try {
      LOG_TOPIC("68fc6", TRACE, Logger::COMMUNICATION)
          << slice.toJson() << " , " << slice.byteSize();
    } catch (...) {
    }
    buffer->appendText(slice.startAs<char>(), slice.byteSize());
  }

  return buffer;
}

// helper functions for sending chunks when given a string buffer as input
// working version of single chunk message creation
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
    char const* data, std::size_t begin, std::size_t end, bool isFirstChunk, uint32_t chunk,
    uint64_t id, ProtocolVersion protocolVersion, uint64_t totalMessageLength) {
  using basics::StringBuffer;

  bool sendTotalLen =
      protocolVersion != ProtocolVersion::VST_1_0 || (isFirstChunk && chunk > 1);
  // if we speak VST_1_0 and have more than one chunk and the chunk
  // is the first then we are sending the first in a series. if this
  // condition is true we also send extra 8 bytes for the messageLength
  // (length of all VPackData). In later versions we always send the
  // total length.

  // build chunkX -- see VelocyStream Documentaion
  chunk <<= 1;
  chunk |= isFirstChunk ? 0x1 : 0x0;

  // get the length of VPack data
  uint32_t dataLength = static_cast<uint32_t>(end - begin);

  // calculate length of current chunk
  uint32_t chunkLength =
      dataLength + static_cast<uint32_t>(chunkHeaderLength(sendTotalLen));

  auto buffer = std::make_unique<StringBuffer>(chunkLength, false);

  appendLittleEndian(buffer.get(), chunkLength);
  appendLittleEndian(buffer.get(), chunk);
  appendLittleEndian(buffer.get(), id);

  if (sendTotalLen) {
    appendLittleEndian(buffer.get(), totalMessageLength);
  }

  buffer->appendText(data + begin, dataLength);

  return buffer;
}

// this function will be called when we send multiple compressed
// or uncompressed chunks
inline void send_many(std::vector<std::unique_ptr<basics::StringBuffer>>& resultVecRef,
                      uint64_t id, std::size_t maxChunkBytes,
                      std::unique_ptr<basics::StringBuffer> completeMessage,
                      std::size_t uncompressedCompleteMessageLength,
                      ProtocolVersion protocolVersion) {
  uint64_t totalLen = completeMessage->length();
  std::size_t offsetBegin = 0;
  std::size_t offsetEnd = maxChunkBytes - chunkHeaderLength(true);
  // maximum number of bytes for follow up chunks
  std::size_t maxBytes = maxChunkBytes - chunkHeaderLength(false);

  uint32_t numberOfChunks = 1;
  {  // calculate the number of chunks that will be send
    std::size_t bytesToSend = totalLen - maxChunkBytes + chunkHeaderLength(true);  // data for first chunk
    while (bytesToSend >= maxBytes) {
      bytesToSend -= maxBytes;
      ++numberOfChunks;
    }
    if (bytesToSend) {
      ++numberOfChunks;
    }
  }

  resultVecRef.reserve(numberOfChunks);

  // send first
  resultVecRef.push_back(
      createChunkForNetworkDetail(completeMessage->c_str(), offsetBegin, offsetEnd, true,
                                  numberOfChunks, id, protocolVersion, totalLen));

  std::uint32_t chunkNumber = 0;
  while (offsetEnd + maxBytes <= totalLen) {
    // send middle
    offsetBegin = offsetEnd;
    offsetEnd += maxBytes;
    chunkNumber++;
    resultVecRef.push_back(
        createChunkForNetworkDetail(completeMessage->c_str(), offsetBegin, offsetEnd, false,
                                    chunkNumber, id, protocolVersion, totalLen));
  }

  if (offsetEnd < totalLen) {
    resultVecRef.push_back(
        createChunkForNetworkDetail(completeMessage->c_str(), offsetEnd, totalLen, false,
                                    ++chunkNumber, id, protocolVersion, totalLen));
  }
}

// this function will be called by client code
inline std::vector<std::unique_ptr<basics::StringBuffer>> createChunkForNetwork(
    std::vector<VPackSlice> const& slices, uint64_t messageid,
    std::size_t maxChunkBytes, ProtocolVersion protocolVersion) {
  /// variables used in this function
  std::size_t payloadLength = 0;

  std::vector<std::unique_ptr<basics::StringBuffer>> rv;

  // find out the uncompressed payload length
  for (auto const& slice : slices) {
    payloadLength += slice.byteSize();
  }

  bool sendTotalLen = protocolVersion != ProtocolVersion::VST_1_0;
  size_t chl = chunkHeaderLength(sendTotalLen);

  if (payloadLength < maxChunkBytes - chl) {
    // one chunk uncompressed
    rv.push_back(createChunkForNetworkDetail(slices, true, 1, messageid,
                                             protocolVersion, chl + payloadLength));
  } else {
    // here we enter the domain of multichunk
    LOG_TOPIC("07b6a", DEBUG, Logger::COMMUNICATION)
        << "VstCommTask: sending multichunk message";

    // TODO: test if we have smaller slices that fit into chunks

    LOG_TOPIC("8007d", DEBUG, Logger::COMMUNICATION)
        << "VstCommTask: there are slices that do not fit into a single "
           "totalMessageLength or compression is enabled";
    // we have big slices that do not fit into single chunks
    // now we will build one big buffer and split it into pieces

    // reserve buffer
    auto vstPayload = std::make_unique<basics::StringBuffer>(payloadLength, false);

    // fill buffer
    for (auto const& slice : slices) {
      try {
        LOG_TOPIC("1d90f", TRACE, Logger::COMMUNICATION)
            << slice.toJson() << " , " << slice.byteSize();
      } catch (...) {
      }
      vstPayload->appendText(slice.startAs<char>(), slice.byteSize());
    }

    // create chunks
    send_many(rv, messageid, maxChunkBytes, std::move(vstPayload), payloadLength, protocolVersion);
  }
  return rv;
}

}  // namespace arangodb

#endif
