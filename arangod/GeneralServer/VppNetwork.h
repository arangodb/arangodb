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
  // chunkLength uint32 , chunkX uint32 , id uint64 , messageLength unit32
  return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) +
         (firstOfMany ? sizeof(uint32_t) : 0);
}

// working version of single chunk message creation
inline std::unique_ptr<basics::StringBuffer> createChunkForNetworkDetail(
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
  uint32_t chunkLength = dataLength + chunkHeaderLength(firstOfMany);

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

inline std::vector<std::unique_ptr<basics::StringBuffer>> createChunkForNetwork(
    std::vector<VPackSlice> const& slices, uint64_t id,
    std::size_t maxChunkBytes, bool compress = false) {
  /// variables used in this function
  std::size_t uncompressedPayloadLength = 0;
  // worst case len in case of compression
  std::size_t preliminaryPayloadLength = 0;
  std::size_t compressedPayloadLength = 0;
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
  }

  if (compress &&
      preliminaryPayloadLength < maxChunkBytes - chunkHeaderLength(false)) {
    // one chunk compressed
  }

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "VppCommTask: sending multichunk message";
  //// here we enter the domain of multichunck

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
    vppPayload->appendText(
        std::string(slice.startAs<char>(), slice.byteSize()));
  }

  if (compress) {
    // compress uncompressedVppPayload -> vppPayload
    auto uncommpressedVppPayload = std::move(vppPayload);
    vppPayload = std::make_unique<basics::StringBuffer>(
        TRI_UNKNOWN_MEM_ZONE, preliminaryPayloadLength, false);
    // do compression
    payloadLength = compressedPayloadLength;
  }

  // create chunks
  (void)vppPayload;
  (void)payloadLength;

  return rv;
}

#endif
