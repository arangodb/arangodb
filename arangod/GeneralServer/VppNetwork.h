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

#ifndef ARANGOD_VPP_NETWORK_H
#define ARANGOD_VPP_NETWORK_H 1

#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LoggerFeature.h"

#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>
#include <stdexcept>

using namespace arangodb;

inline std::size_t validateAndCount(char const* vpHeaderStart,
                                    char const* vpEnd) {
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
std::size_t appendToBuffer(basics::StringBuffer* buffer, T& value) {
  constexpr std::size_t len = sizeof(T);
  char charArray[len];
  char const* charPtr = charArray;
  std::memcpy(&charArray, &value, len);
  buffer->appendText(charPtr, len);
  return len;
}

inline constexpr std::size_t chunkHeaderLength(bool firstOfMany) {
  // chunkLength uint32 , chunkX uint32 , id uint64 , messageLength unit64
  return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) +
         (firstOfMany ? sizeof(uint64_t) : 0);
}

// Send Message Created from Slices

// working version of single chunk message creation
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
    std::vector<VPackSlice> const& slices, bool isFirstChunk, uint32_t chunk,
    uint64_t id, uint64_t totalMessageLength = 0) {
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
  uint32_t chunkLength =
      dataLength + static_cast<uint32_t>(chunkHeaderLength(firstOfMany));

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
    buffer->appendText(slice.startAs<char>(), slice.byteSize());
  }

  return buffer;
}

//  slices, isFirstChunk, chunk, id, totalMessageLength
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkSingle(
    std::vector<VPackSlice> const& slices, uint64_t id) {
  return createChunkForNetworkDetail(slices, true, 1, id, 0 /*unused*/);
}

// This method does not respect the max chunksize instead it avoids copying
// by moving slices into the createion functions - This is not acceptable for
// big slices
//
// inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkMultiFirst(
//    std::vector<VPackSlice> const& slices, uint64_t id, uint32_t
//    numberOfChunks,
//    uint32_t totalMessageLength) {
//  return createChunkForNetworkDetail(slices, true, numberOfChunks, id,
//                                     totalMessageLength);
//}
//
// inline std::unique_ptr<basics::StringBuffer>
// createChunkForNetworkMultiFollow(
//    std::vector<VPackSlice> const& slices, uint64_t id, uint32_t chunkNumber,
//    uint32_t totalMessageLength) {
//  return createChunkForNetworkDetail(slices, false, chunkNumber, id, 0);
//}

// helper functions for sending chunks when given a string buffer as input
// working version of single chunk message creation
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
    char const* data, std::size_t begin, std::size_t end, bool isFirstChunk,
    uint32_t chunk, uint64_t id, uint64_t totalMessageLength = 0) {
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
  uint32_t dataLength = static_cast<uint32_t>(end - begin);

  // calculate length of current chunk
  uint32_t chunkLength =
      dataLength + static_cast<uint32_t>(chunkHeaderLength(firstOfMany));

  auto buffer =
      std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE, chunkLength, false);

  appendToBuffer(buffer.get(), chunkLength);
  appendToBuffer(buffer.get(), chunk);
  appendToBuffer(buffer.get(), id);

  if (firstOfMany) {
    appendToBuffer(buffer.get(), totalMessageLength);
  }

  buffer->appendText(data + begin, dataLength);

  return buffer;
}

inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkMultiFirst(
    char const* data, std::size_t begin, std::size_t end, uint64_t id,
    uint32_t numberOfChunks, uint64_t totalMessageLength) {
  return createChunkForNetworkDetail(data, begin, end, true, numberOfChunks, id,
                                     totalMessageLength);
}

inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkMultiFollow(
    char const* data, std::size_t begin, std::size_t end, uint64_t id,
    uint32_t chunkNumber) {
  return createChunkForNetworkDetail(data, begin, end, false, chunkNumber, id,
                                     0);
}

// this function will be called when we send multiple compressed
// or uncompressed chunks
inline void send_many(
    std::vector<std::unique_ptr<basics::StringBuffer>>& resultVecRef,
    uint64_t id, std::size_t maxChunkBytes,
    std::unique_ptr<basics::StringBuffer> completeMessage,
    std::size_t uncompressedCompleteMessageLength) {
  uint64_t totalLen = completeMessage->length();
  std::size_t offsetBegin = 0;
  std::size_t offsetEnd = maxChunkBytes - chunkHeaderLength(true);
  // maximum number of bytes for follow up chunks
  std::size_t maxBytes = maxChunkBytes - chunkHeaderLength(false);

  uint32_t numberOfChunks = 1;
  {  // calcuate the number of chunks taht will be send
    std::size_t bytesToSend = totalLen - maxChunkBytes +
                              chunkHeaderLength(true);  // data for first chunk
    while (bytesToSend >= maxBytes) {
      bytesToSend -= maxBytes;
      ++numberOfChunks;
    }
    if (bytesToSend) {
      ++numberOfChunks;
    }
  }

  // send first
  resultVecRef.push_back(
      createChunkForNetworkMultiFirst(completeMessage->c_str(), offsetBegin,
                                      offsetEnd, id, numberOfChunks, totalLen));

  std::uint32_t chunkNumber = 0;
  while (offsetEnd + maxBytes <= totalLen) {
    // send middle
    offsetBegin = offsetEnd;
    offsetEnd += maxBytes;
    chunkNumber++;
    resultVecRef.push_back(createChunkForNetworkMultiFollow(
        completeMessage->c_str(), offsetBegin, offsetEnd, id, chunkNumber));
  }

  if (offsetEnd < totalLen) {
    resultVecRef.push_back(createChunkForNetworkMultiFollow(
        completeMessage->c_str(), offsetEnd, totalLen, id, ++chunkNumber));
  }

  return;
}

// this function will be called by client code
inline std::vector<std::unique_ptr<basics::StringBuffer>> createChunkForNetwork(
    std::vector<VPackSlice> const& slices, uint64_t id,
    std::size_t maxChunkBytes, bool compress = false) {
  /// variables used in this function
  std::size_t uncompressedPayloadLength = 0;
  // worst case len in case of compression
  std::size_t preliminaryPayloadLength = 0;
  // std::size_t compressedPayloadLength = 0;
  std::size_t payloadLength = 0;  // compressed or uncompressed

  std::vector<std::unique_ptr<basics::StringBuffer>> rv;

  // find out the uncompressed payload length
  for (auto const& slice : slices) {
    uncompressedPayloadLength += slice.byteSize();
  }

  if (compress) {
    // use some function to calculate the worst case lenght
    preliminaryPayloadLength = uncompressedPayloadLength;
  } else {
    payloadLength = uncompressedPayloadLength;
  }

  if (!compress &&
      uncompressedPayloadLength < maxChunkBytes - chunkHeaderLength(false)) {
    // one chunk uncompressed
    rv.push_back(createChunkForNetworkSingle(slices, id));
    return rv;
  } else if (compress &&
             preliminaryPayloadLength <
                 maxChunkBytes - chunkHeaderLength(false)) {
    throw std::logic_error("no implemented");
    // one chunk compressed
  } else {
    //// here we enter the domain of multichunck
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VppCommTask: sending multichunk message";

    // test if we have smaller slices that fit into chunks when there is
    // no compression - optimization

    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "VppCommTask: there are slices that do not fit into a single "
           "totalMessageLength or compression is enabled";
    // we have big slices that do not fit into single chunks
    // now we will build one big buffer ans split it into pieces

    // reseve buffer
    auto vppPayload = std::make_unique<basics::StringBuffer>(
        TRI_UNKNOWN_MEM_ZONE, uncompressedPayloadLength, false);

    // fill buffer
    for (auto const& slice : slices) {
      vppPayload->appendText(slice.startAs<char>(), slice.byteSize());
    }

    if (compress) {
      // compress uncompressedVppPayload -> vppPayload
      auto uncommpressedVppPayload = std::move(vppPayload);
      vppPayload = std::make_unique<basics::StringBuffer>(
          TRI_UNKNOWN_MEM_ZONE, preliminaryPayloadLength, false);
      // do compression
      throw std::logic_error("no implemented");
      // payloadLength = compressedPayloadLength;
    }

    // create chunks
    (void)payloadLength;
    send_many(rv, id, maxChunkBytes, std::move(vppPayload),
              uncompressedPayloadLength);
  }
  return rv;
}

#endif
