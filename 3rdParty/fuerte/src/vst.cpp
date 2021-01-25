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

#include "vst.h"

#include <fuerte/FuerteLogger.h>
#include <fuerte/detail/vst.h>
#include <fuerte/helper.h>
#include <fuerte/types.h>
#include <velocypack/HexDump.h>
#include <velocypack/Iterator.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <boost/algorithm/string.hpp>

#include "Basics/Format.h"
#include "debugging.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

///////////////////////////////////////////////////////////////////////////////////
// sending vst
///////////////////////////////////////////////////////////////////////////////////

// ### not exported
// ###############################################################
// sending vst
/*static std::string chunkHeaderToString(ChunkHeader const& header){
  std::stringstream ss;
  ss << "### ChunkHeader ###"
     << "\nchunk length:         " << header.chunkLength()
     << "\nnumber of chunks:     " << header.numberOfChunks()
     << "\nmessage id:           " << header.messageID()
     << "\nis first:             " << header.isFirst()
     << "\nindex:                " << header.index()
     << "\ntotal message length: " << header.messageLength()
     << "\n";
  return ss.str();
}*/

// writeHeaderToVST1_0 write the chunk to the given buffer in VST 1.0 format.
// The length of the buffer is returned.
size_t ChunkHeader::writeHeaderToVST1_0(size_t chunkDataLen,
                                        VPackBuffer<uint8_t>& buffer) const {
  size_t hdrLength = minChunkHeaderSize;
  uint8_t hdr[maxChunkHeaderSize];
  if (isFirst() && numberOfChunks() > 1) {
    // Use extended header
    hdrLength = maxChunkHeaderSize;
    basics::uintToPersistentLE<uint64_t>(
        hdr + 16, _messageLength);  // total message length
  }                                 // else Use minimal header
  basics::uintToPersistentLE<uint32_t>(
      hdr + 0, hdrLength + chunkDataLen);  // chunk length (header+data)
  basics::uintToPersistentLE<uint32_t>(hdr + 4, _chunkX);     // chunkX
  basics::uintToPersistentLE<uint64_t>(hdr + 8, _messageID);  // messageID

  // Now add hdr to buffer
  buffer.append(hdr, hdrLength);
  return hdrLength;
}

// writeHeaderToVST1_1 write the chunk to the given buffer in VST 1.1 format.
// The length of the buffer is returned.
size_t ChunkHeader::writeHeaderToVST1_1(size_t chunkDataLen,
                                        VPackBuffer<uint8_t>& buffer) const {
  if (buffer.capacity() < maxChunkHeaderSize) {
    buffer.reserve(maxChunkHeaderSize);
  }
  uint8_t* hdr = buffer.data() + buffer.size();
  basics::uintToPersistentLE<uint32_t>(hdr + 0,
                                       maxChunkHeaderSize + chunkDataLen);
  basics::uintToPersistentLE<uint32_t>(hdr + 4, _chunkX);     // chunkX
  basics::uintToPersistentLE<uint64_t>(hdr + 8, _messageID);  // messageID
  basics::uintToPersistentLE<uint64_t>(hdr + 16,
                                       _messageLength);  // total message length

  buffer.advance(maxChunkHeaderSize);
  return maxChunkHeaderSize;
}

// section - VstMessageHeaders

/// @brief creates a slice containing a VST request-message header.
void message::requestHeader(RequestHeader const& header,
                            VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);

  assert(builder.isClosed());
  builder.openArray();

  // 0 - version
  builder.add(VPackValue(header.version()));

  // 1 - type
  builder.add(VPackValue(static_cast<int>(MessageType::Request)));
  FUERTE_LOG_DEBUG << "MessageHeader.type=request\n";

  // 2 - database
  if (header.database.empty()) {
    throw std::runtime_error("database for message not set");
  }
  builder.add(VPackValue(header.database));
  FUERTE_LOG_DEBUG << "MessageHeader.database=" << header.database << "\n";

  // 3 - RestVerb
  if (header.restVerb == RestVerb::Illegal) {
    throw std::runtime_error("rest verb  for message not set");
  }
  builder.add(VPackValue(static_cast<int>(header.restVerb)));
  FUERTE_LOG_DEBUG << "MessageHeader.restVerb="
                   << static_cast<int>(header.restVerb) << "\n";

  // 4 - path
  // if(!header.path){ throw std::runtime_error("path" + message); }
  if (header.path.empty() || header.path[0] != '/') {
    builder.add(VPackValue("/" + header.path));
  } else {
    builder.add(VPackValue(header.path));
  }
  FUERTE_LOG_DEBUG << "MessageHeader.path=" << header.path << "\n";

  // 5 - parameters - not optional in current server
  if (!header.parameters.empty()) {
    VPackObjectBuilder guard(&builder);
    for (auto const& item : header.parameters) {
      builder.add(item.first, VPackValue(item.second));
    }
  } else {
    builder.add(VPackSlice::emptyObjectSlice());
  }

  // 6 - meta
  {
    VPackObjectBuilder guard(&builder);
    if (header.acceptType() != ContentType::Custom) {
      builder.add(fu_accept_key, VPackValue(to_string(header.acceptType())));
    }
    if (header.contentType() != ContentType::Custom) {
      builder.add(fu_content_type_key,
                  VPackValue(to_string(header.contentType())));
    }
    for (auto const& pair : header.meta()) {  // iequals for data from server
      builder.add(pair.first, VPackValue(pair.second));
    }
  }
  builder.close();  // </array>
}

/// @brief creates a slice containing a VST response-message header.
void message::responseHeader(ResponseHeader const& header,
                             VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);
  assert(builder.isClosed());
  builder.openArray();

  // 0 - version
  builder.add(VPackValue(header.version()));

  FUERTE_ASSERT(header.responseType() == MessageType::Response ||
                header.responseType() == MessageType::ResponseUnfinished);

  // 1 - type
  builder.add(VPackValue(static_cast<int>(header.responseType())));
  FUERTE_LOG_DEBUG << "MessageHeader.type=response\n";

  // 2 - responseCode

  if (!header.responseCode) {
    throw std::runtime_error("response code for message not set");
  }
  builder.add(VPackValue(header.responseCode));

  // 3 - meta (not optional even if empty)
  builder.openObject();
  builder.add(fu_content_type_key, VPackValue(to_string(header.contentType())));
  if (!header.meta().empty()) {
    for (auto const& pair : header.meta()) {
      if (boost::iequals(fu_content_type_key, pair.first)) {
        continue;
      }
      builder.add(pair.first, VPackValue(pair.second));
    }
  }
  builder.close();
}

/// @brief creates a slice containing a VST auth message with JWT encryption
void message::authJWT(std::string const& token, VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);
  builder.openArray();
  builder.add(VPackValue(1));  // version
  builder.add(
      VPackValue(static_cast<int>(MessageType::Authentication)));  // type
  builder.add(VPackValue("jwt"));                                  // encryption
  builder.add(VPackValue(token));                                  // token
  builder.close();
}

/// @brief creates a slice containing a VST auth message with plain enctyption
void message::authBasic(std::string const& username,
                        std::string const& password,
                        VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);
  builder.openArray();
  builder.add(VPackValue(1));  // version
  builder.add(
      VPackValue(static_cast<int>(MessageType::Authentication)));  // type
  builder.add(VPackValue("plain"));                                // encryption
  builder.add(VPackValue(username));                               // user
  builder.add(VPackValue(password));                               // password
  builder.close();
}

/// @brief take existing buffers and partitions into chunks
/// @param buffer is containing the metadata. If non empty this is going to be
///        used as message header
/// @param payload the payload that is going to be partitioned
void message::prepareForNetwork(VSTVersion vstVersion, MessageID messageId,
                                VPackBuffer<uint8_t>& buffer,
                                asio_ns::const_buffer payload,
                                std::vector<asio_ns::const_buffer>& result) {
  // Split message into chunks
  // we assume that the message header is already in the buffer
  size_t msgLength = buffer.size() + payload.size();
  assert(msgLength > 0);

  // builds a list of chunks that are ready to be sent to the server.
  // The resulting set of chunks are added to the given result vector.

  // calculate intended number of chunks
  const size_t maxDataLength = defaultMaxChunkSize - maxChunkHeaderSize;
  const size_t numChunks = (msgLength + maxDataLength - 1) / maxDataLength;
  assert(maxDataLength > 0);
  assert(numChunks > 0);

  // we allocate enough space so that pointers into it stay valid
  const size_t spaceNeeded = numChunks * maxChunkHeaderSize;
  buffer.reserve(spaceNeeded);

  asio_ns::const_buffer header(buffer.data(), buffer.size());
  result.reserve((2 * numChunks) + 1);

  uint32_t chunkIndex = 0;
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(payload.data());
#ifdef FUERTE_DEBUG
  uint8_t const* end =
      reinterpret_cast<uint8_t const*>(payload.data()) + payload.size();
#endif

  size_t remaining = msgLength;
  while (remaining > 0) {
    // begin writing a new chunk
    ChunkHeader chunk;
    chunk._chunkX =
        (chunkIndex == 0) ? ((numChunks << 1) | 1) : (chunkIndex << 1);
    chunk._messageID = messageId;
    chunk._messageLength = msgLength;

    // put data into the chunk
    size_t chunkDataLen = std::min(maxDataLength, remaining);
    remaining -= chunkDataLen;
    assert(chunkDataLen > 0);

    size_t chunkHdrLen = 0;
    size_t chunkOffset = buffer.byteSize();
    switch (vstVersion) {
      case VST1_0:
        chunkHdrLen = chunk.writeHeaderToVST1_0(chunkDataLen, buffer);
        break;
      case VST1_1:
        chunkHdrLen = chunk.writeHeaderToVST1_1(chunkDataLen, buffer);
        break;
      default:
        throw std::logic_error("Unknown VST version");
    }
    assert(chunkHdrLen > 0 && chunkHdrLen <= maxChunkHeaderSize);

    // Add chunk buffer
    result.emplace_back(buffer.data() + chunkOffset, chunkHdrLen);
    if (chunkIndex == 0) {  // stuff in message header
      assert(header.size() <= chunkDataLen);
      result.emplace_back(header);
      chunkDataLen -= header.size();
    }
    if (chunkDataLen > 0) {
      FUERTE_ASSERT(payload.size() > 0);
#ifdef FUERTE_DEBUG
      FUERTE_ASSERT(begin < end);
#endif
      // Add chunk data buffer
      result.emplace_back(begin, chunkDataLen);
      begin += chunkDataLen;
    }

    chunkIndex++;
    assert(chunkIndex <= numChunks);
  }
  assert(chunkIndex == numChunks);
}

// ################################################################################

// prepareForNetwork prepares the internal structures for
// writing the request to the network.
std::vector<asio_ns::const_buffer> RequestItem::prepareForNetwork(
    VSTVersion vstVersion) {
  // setting defaults
  request->header.setVersion(1);  // always set to 1
  if (request->header.database.empty()) {
    request->header.database = "_system";
  }

  // Create the message header and store it in the metadata buffer
  _buffer.clear();
  message::requestHeader(request->header, _buffer);
  assert(_buffer.size() > 0);
  // message header has to go into the first chunk
  asio_ns::const_buffer payload = request->payload();

  // _buffer content will be used as message header
  std::vector<asio_ns::const_buffer> result;
  message::prepareForNetwork(vstVersion, _messageID, _buffer, payload, result);
  return result;
}

namespace parser {

///////////////////////////////////////////////////////////////////////////////////
// receiving vst
///////////////////////////////////////////////////////////////////////////////////

/// @brief readChunkVST1_0 reads a chunk header in VST1.0 format.
/// @return true if chunk is complete
ChunkState readChunkVST1_0(Chunk& chunk, uint8_t const* hdr,
                           std::size_t avail) {
  if (avail < minChunkHeaderSize) {
    return ChunkState::Incomplete;
  }

  chunk.header._chunkLength = basics::uintFromPersistentLE<uint32_t>(hdr + 0);
  chunk.header._chunkX = basics::uintFromPersistentLE<uint32_t>(hdr + 4);
  chunk.header._messageID = basics::uintFromPersistentLE<uint64_t>(hdr + 8);

  if (avail < chunk.header._chunkLength) {
    return ChunkState::Incomplete;
  }

  size_t hdrLen = minChunkHeaderSize;
  if (chunk.header.isFirst()) {
    if (chunk.header.numberOfChunks() > 1) {
      if (avail < maxChunkHeaderSize) {
        return ChunkState::Incomplete;
      }

      hdrLen = maxChunkHeaderSize;  // first chunk header is bigger
      // First chunk, numberOfChunks>1 -> read messageLength
      chunk.header._messageLength =
          basics::uintFromPersistentLE<uint64_t>(hdr + 16);
    } else {
      chunk.header._messageLength = chunk.header._chunkLength - hdrLen;
    }
  } else {  // not needed / known otherwise
    chunk.header._messageLength = 0;
  }

  FUERTE_ASSERT(avail >= chunk.header._chunkLength);
  if (hdrLen > chunk.header._chunkLength) {
    return ChunkState::Invalid;  // chunk incomplete / invalid chunk length
  }

  size_t contentLength = chunk.header._chunkLength - hdrLen;

  FUERTE_LOG_VSTCHUNKTRACE << "readChunkHeaderVST1_0: got " << contentLength
                           << " data bytes after " << hdrLen
                           << " header bytes\n";
  chunk.body = asio_ns::const_buffer(hdr + hdrLen, contentLength);

  return ChunkState::Complete;
}

/// @brief readChunkVST1_1 reads a chunk header in VST1.1 format.
ChunkState readChunkVST1_1(Chunk& chunk, uint8_t const* hdr,
                           std::size_t avail) {
  if (avail < maxChunkHeaderSize) {
    return ChunkState::Incomplete;
  }

  chunk.header._chunkLength = basics::uintFromPersistentLE<uint32_t>(hdr + 0);
  chunk.header._chunkX = basics::uintFromPersistentLE<uint32_t>(hdr + 4);
  chunk.header._messageID = basics::uintFromPersistentLE<uint64_t>(hdr + 8);
  chunk.header._messageLength =
      basics::uintFromPersistentLE<uint64_t>(hdr + 16);

  if (avail < chunk.header._chunkLength) {
    return ChunkState::Incomplete;
  }

  if (maxChunkHeaderSize > chunk.header._chunkLength) {
    return ChunkState::Invalid;  // invalid chunk length
  }

  size_t contentLength = chunk.header._chunkLength - maxChunkHeaderSize;
  FUERTE_LOG_VSTCHUNKTRACE << "readChunkHeaderVST1_1: got " << contentLength
                           << " data bytes after " << maxChunkHeaderSize
                           << " bytes\n";
  chunk.body = asio_ns::const_buffer(hdr + maxChunkHeaderSize, contentLength);

  return ChunkState::Complete;
}

/// @brief verifies header input and checks correct length
/// @return message type or MessageType::Undefined on an error
MessageType validateAndExtractMessageType(uint8_t const* const vpStart,
                                          std::size_t length, size_t& hLength) {
  // there must be at least one velocypack for the header
  VPackOptions validationOptions = VPackOptions::Defaults;
  validationOptions.validateUtf8Strings = true;
  VPackValidator validator(&validationOptions);

  try {
    // isSubPart allows the slice to be shorter than the checked buffer.
    validator.validate(vpStart, length, /*isSubPart*/ true);
    FUERTE_LOG_VSTTRACE << "validation done\n";
  } catch (std::exception const& e) {
    FUERTE_LOG_VSTTRACE << "len: " << length
                        << std::string(reinterpret_cast<char const*>(vpStart),
                                       length);
    throw std::runtime_error(
        std::string("error during validation of incoming VPack (HEADER): ") +
        e.what());
  }
  VPackSlice slice = VPackSlice(vpStart);
  if (!slice.isArray()) {
    return MessageType::Undefined;
  }
  hLength = slice.byteSize();

  VPackSlice vSlice = slice.at(0);
  if (!vSlice.isNumber<short>() || vSlice.getNumber<int>() != 1) {
    FUERTE_LOG_ERROR << "VST message header has an unsupported version";
    return MessageType::Undefined;
  }
  VPackSlice typeSlice = slice.at(1);
  if (!typeSlice.isNumber<int>()) {
    return MessageType::Undefined;
  }
  return intToMessageType(typeSlice.getNumber<int>());
}

RequestHeader requestHeaderFromSlice(VPackSlice const& headerSlice) {
  assert(headerSlice.isArray());
  RequestHeader header;

  header.setVersion(headerSlice.at(0).getNumber<short>());  // version
  assert(headerSlice.at(1).getNumber<int>() ==
         static_cast<int>(MessageType::Request));
  header.database = headerSlice.at(2).copyString();  // databse
  header.restVerb =
      static_cast<RestVerb>(headerSlice.at(3).getInt());    // rest verb
  header.path = headerSlice.at(4).copyString();             // request (path)
  for (auto it : VPackObjectIterator(headerSlice.at(5))) {  // query params
    header.parameters.emplace(it.key.copyString(), it.value.copyString());
  }
  for (auto it : VPackObjectIterator(headerSlice.at(6))) {  // meta (headers)
    std::string key = it.key.copyString();
    toLowerInPlace(key);
    header.addMeta(std::move(key), it.value.copyString());
  }
  return header;
};

ResponseHeader responseHeaderFromSlice(VPackSlice const& headerSlice) {
  FUERTE_ASSERT(headerSlice.isArray());
  ResponseHeader header;

  header.setVersion(headerSlice.at(0).getNumber<short>());  // version
  FUERTE_ASSERT(headerSlice.at(1).getNumber<int>() ==
                static_cast<int>(MessageType::Response));
  header.responseCode = headerSlice.at(2).getNumber<StatusCode>();
  if (headerSlice.length() >= 4) {
    VPackSlice meta = headerSlice.at(3);
    assert(meta.isObject());
    if (meta.isObject()) {
      for (auto it : VPackObjectIterator(meta)) {
        std::string key = it.key.copyString();
        toLowerInPlace(key);
        header.addMeta(std::move(key), it.value.copyString());
      }
    }
  }
  if (header.contentType() == ContentType::Unset) {
    header.contentType(ContentType::VPack);
  }
  return header;
}

// Validates if payload consists of valid velocypack slices
std::size_t validateAndCount(uint8_t const* const vpStart, std::size_t length) {
  // start points to the begin of a velocypack
  uint8_t const* cursor = vpStart;
  // there must be at least one velocypack for the header
  VPackValidator validator;
  std::size_t numPayloads = 0;  // fist item is the header
  bool isSubPart = true;

  FUERTE_LOG_VSTTRACE << "buffer length to validate: " << length << "\n";

  FUERTE_LOG_VSTTRACE << "sliceSizes: ";
  VPackSlice slice;
  while (length) {
    try {
      // isSubPart allows the slice to be shorter than the checked buffer.
      validator.validate(cursor, length, isSubPart);
      slice = VPackSlice(cursor);
      auto sliceSize = slice.byteSize();
      if (length < sliceSize) {
        throw std::length_error("slice is longer than buffer");
      }
      length -= sliceSize;
      cursor += sliceSize;
      numPayloads++;
      FUERTE_LOG_VSTTRACE << sliceSize << " ";
    } catch (std::exception const& e) {
      FUERTE_LOG_VSTTRACE << "len: " << length << velocypack::HexDump(slice);
      FUERTE_LOG_VSTTRACE << "len: " << length
                          << std::string(reinterpret_cast<char const*>(cursor),
                                         length);
      throw std::runtime_error(
          std::string("error during validation of incoming VPack (body)") +
          e.what());
    }
  }
  FUERTE_LOG_VSTTRACE << "\n";
  return numPayloads;
}

}  // namespace parser

// add the given chunk to the list of response chunks.
void RequestItem::addChunk(Chunk const& chunk) {
  // Copy _data to response buffer
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::addChunk: adding "
                           << chunk.body.size() << " bytes to buffer\n";
  if (_responseChunks.empty()) {  //
    resetSendData();
  }

  // Gather number of chunk info
  if (chunk.header.isFirst()) {
    _responseNumberOfChunks = chunk.header.numberOfChunks();
    _responseChunks.reserve(_responseNumberOfChunks);
    FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::addChunk: set #chunks to "
                             << _responseNumberOfChunks << "\n";
    FUERTE_ASSERT(_buffer.empty());
    if (_buffer.capacity() < chunk.header.messageLength()) {
      FUERTE_ASSERT(_buffer.capacity() >= _buffer.size());
      _buffer.reserve(chunk.header.messageLength() - _buffer.size());
      FUERTE_ASSERT(_buffer.capacity() >= chunk.header.messageLength());
    }
  }
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(chunk.body.data());
  size_t offset = _buffer.size();
  _buffer.append(begin, chunk.body.size());
  // Add chunk to index list
  _responseChunks.push_back(
      ChunkInfo{chunk.header.index(), offset, chunk.body.size()});
}

static bool chunkByIndex(const RequestItem::ChunkInfo& a,
                         const RequestItem::ChunkInfo& b) {
  return (a.index < b.index);
}

// try to assembly the received chunks into a buffer.
// returns NULL if not all chunks are available.
std::unique_ptr<VPackBuffer<uint8_t>> RequestItem::assemble() {
  if (_responseNumberOfChunks == 0) {
    // We don't have the first chunk yet
    FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::assemble: don't have first chunk"
                             << "\n";
    return nullptr;
  }
  if (_responseChunks.size() < _responseNumberOfChunks) {
    // Not all chunks have arrived yet
    FUERTE_LOG_VSTCHUNKTRACE
        << "RequestItem::assemble: not all chunks have arrived"
        << "\n";
    return nullptr;
  }
  FUERTE_ASSERT(_responseChunks.size() == _responseNumberOfChunks);

  // fast-path: chunks received in-order
  bool reject = false;
  for (size_t i = 0; i < _responseNumberOfChunks; i++) {
    if (_responseChunks[i].index != i) {
      reject = true;
      break;
    }
  }
  if (!reject) {
    FUERTE_LOG_VSTCHUNKTRACE
        << "RequestItem::assemble: fast-path, chunks are in order"
        << "\n";
    return std::make_unique<VPackBuffer<uint8_t>>(std::move(_buffer));
  }

  // We now have all chunks. Sort them by index.
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::assemble: sort chunks"
                           << "\n";
  std::sort(_responseChunks.begin(), _responseChunks.end(), chunkByIndex);

  // Combine chunk content
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::assemble: build response buffer"
                           << "\n";

  auto buffer = std::make_unique<VPackBuffer<uint8_t>>();
  for (ChunkInfo const& info : _responseChunks) {
    buffer->append(_buffer.data() + info.offset, info.size);
  }

  return buffer;
}
}}}}  // namespace arangodb::fuerte::v1::vst
