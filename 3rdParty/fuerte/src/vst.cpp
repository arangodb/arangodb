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

#include "Basics/Format.h"
#include "vst.h"

#include <boost/algorithm/string.hpp>
#include <fuerte/helper.h>
#include <fuerte/types.h>
#include <fuerte/FuerteLogger.h>
#include <fuerte/detail/vst.h>

#include <velocypack/HexDump.h>
#include <velocypack/Iterator.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

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
     << std::endl;
  return ss.str();
}*/

// writeHeaderToVST1_0 write the chunk to the given buffer in VST 1.0 format.
// The length of the buffer is returned.
size_t ChunkHeader::writeHeaderToVST1_0(size_t chunkDataLen,
                                        VPackBuffer<uint8_t>& buffer) const {
  size_t hdrLength;
  uint8_t hdr[maxChunkHeaderSize];
  if (isFirst() && numberOfChunks() > 1) {
    // Use extended header
    hdrLength = maxChunkHeaderSize;
    basics::uintToPersistentLittleEndian<uint64_t>(hdr + 16, _messageLength);  // total message length
  } else {
    // Use minimal header
    hdrLength = minChunkHeaderSize;
  }
  basics::uintToPersistentLittleEndian<uint32_t>(hdr + 0, hdrLength + chunkDataLen);  // chunk length (header+data)
  basics::uintToPersistentLittleEndian<uint32_t>(hdr + 4, _chunkX);  // chunkX
  basics::uintToPersistentLittleEndian<uint64_t>(hdr + 8, _messageID);  // messageID
  
  // Now add hdr to buffer
  buffer.append(hdr, hdrLength);
  return hdrLength;
}

// writeHeaderToVST1_1 write the chunk to the given buffer in VST 1.1 format.
// The length of the buffer is returned.
size_t ChunkHeader::writeHeaderToVST1_1(size_t chunkDataLen,
                                        VPackBuffer<uint8_t>& buffer) const {
  buffer.reserve(maxChunkHeaderSize);
  uint8_t* hdr = buffer.data() + buffer.size();
  basics::uintToPersistentLittleEndian<uint32_t>(hdr + 0, maxChunkHeaderSize + chunkDataLen);
  basics::uintToPersistentLittleEndian<uint32_t>(hdr + 4, _chunkX);  // chunkX
  basics::uintToPersistentLittleEndian<uint64_t>(hdr + 8, _messageID);  // messageID
  basics::uintToPersistentLittleEndian<uint64_t>(hdr + 16, _messageLength);  // total message length

  buffer.advance(maxChunkHeaderSize);
  return maxChunkHeaderSize;
}

// section - VstMessageHeaders

/// @brief creates a slice containing a VST request-message header.
void message::requestHeader(RequestHeader const& header,
                            VPackBuffer<uint8_t>& buffer) {
  static std::string const message = " for message not set";
  
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
    throw std::runtime_error("database" + message);
  }
  builder.add(VPackValue(header.database));
  FUERTE_LOG_DEBUG << "MessageHeader.database=" << header.database
                   << std::endl;

  // 3 - RestVerb
  if (header.restVerb == RestVerb::Illegal) {
    throw std::runtime_error("rest verb" + message);
  }
  builder.add(VPackValue(static_cast<int>(header.restVerb)));
  FUERTE_LOG_DEBUG << "MessageHeader.restVerb="
                   << static_cast<int>(header.restVerb) << std::endl;

  // 4 - path
  // if(!header.path){ throw std::runtime_error("path" + message); }
  if (header.path.empty() || header.path[0] != '/') {
    builder.add(VPackValue("/" + header.path));
  } else {
    builder.add(VPackValue(header.path));
  }
  FUERTE_LOG_DEBUG << "MessageHeader.path=" << header.path << std::endl;

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
  if (!header.meta.empty()) {
    VPackObjectBuilder guard(&builder);
    for (auto const& item : header.meta) {
      builder.add(item.first, VPackValue(item.second));
    }
  } else {
    builder.add(VPackSlice::emptyObjectSlice());
  }

  builder.close(); // </array>
}
  
/// @brief creates a slice containing a VST response-message header.
void message::responseHeader(ResponseHeader const& header,
                             VPackBuffer<uint8_t>& buffer) {
  static std::string const message = " for message not set";

  VPackBuilder builder(buffer);
  assert(builder.isClosed());
  builder.openArray();
  
  // 0 - version
  builder.add(VPackValue(header.version()));
  
  assert(header.responseType() == MessageType::Response ||
         header.responseType() == MessageType::ResponseUnfinished);
  
  // 1 - type
  builder.add(VPackValue(static_cast<int>(header.responseType())));
  FUERTE_LOG_DEBUG << "MessageHeader.type=response\n";
  
  // 2 - responseCode
  
  if (!header.responseCode) {
    throw std::runtime_error("response code" + message);
  }
  builder.add(VPackValue(header.responseCode));
  
  // 3 - meta (not optional even if empty)
  builder.openObject();
  if (!header.meta.empty()) {
    for (auto const& item : header.meta) {
      builder.add(item.first, VPackValue(item.second));
    }
  }
  builder.close();
}

/// @brief creates a slice containing a VST auth message with JWT encryption
void message::authJWT(std::string const& token,
                      VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);
  builder.openArray();
  builder.add(VPackValue(1)); // version
  builder.add(VPackValue(static_cast<int>(MessageType::Authentication)));  // type
  builder.add(VPackValue("jwt")); // encryption
  builder.add(VPackValue(token)); // token
  builder.close();
}

/// @brief creates a slice containing a VST auth message with plain enctyption
void message::authBasic(std::string const& username,
                                         std::string const& password,
                                         VPackBuffer<uint8_t>& buffer) {
  VPackBuilder builder(buffer);
  builder.openArray();
  builder.add(VPackValue(1)); // version
  builder.add(VPackValue(static_cast<int>(MessageType::Authentication))); // type
  builder.add(VPackValue("plain")); // encryption
  builder.add(VPackValue(username)); // user
  builder.add(VPackValue(password)); // password
  builder.close();
}
  
/// @brief take existing buffers and partitions into chunks
/// @param buffer is containing the metadata. If non empty this is going to be
///        used as message header
/// @param payload the payload that is going to be partitioned
void message::prepareForNetwork(VSTVersion vstVersion,
                              MessageID messageId,
                              VPackBuffer<uint8_t>& buffer,
                              asio_ns::const_buffer payload,
                              std::vector<asio_ns::const_buffer>& result) {
  
  // Split message into chunks
  // we assume that the message header is already in the buffer
  size_t msgLength = buffer.size() + payload.size();
  assert(msgLength > 0);
  
  // builds a list of chunks that are ready to be send to the server.
  // The resulting set of chunks are added to the given result vector.
  
  // calculate intended number of chunks
  const size_t maxDataLength = defaultMaxChunkSize - maxChunkHeaderSize;
  const size_t numChunks =  (msgLength + maxDataLength - 1) / maxDataLength;
  assert(maxDataLength > 0);
  assert(header.size() < maxDataLength);
  
  // we allocte enough space so that pointers into it stay valid
  buffer.reserve(numChunks * maxChunkHeaderSize);
  asio_ns::const_buffer header(buffer.data(), buffer.size());
  result.reserve(numChunks * maxChunkHeaderSize + 1);
  
  uint32_t chunkIndex = 0;
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(payload.data());
#ifndef NDEBUG
  uint8_t const* end = reinterpret_cast<uint8_t const*>(payload.data()) + payload.size();
#endif
  
  size_t remaining = msgLength;
  while (remaining > 0) {
    
    // begin writing a new chunk
    ChunkHeader chunk;
    chunk._chunkX = (chunkIndex == 0) ? ((numChunks << 1) | 1) : (chunkIndex << 1);
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
    if (chunkIndex == 0) { // stuff in message header
      assert(header.size() <= chunkDataLen);
      result.emplace_back(header);
      chunkDataLen -= header.size();
    }
    assert(begin < end);
    // Add chunk data buffer
    result.emplace_back(begin, chunkDataLen);
    begin += chunkDataLen;
    
    chunkIndex++;
    assert(chunkIndex <= numChunks);
  }
  assert(chunkIndex == numChunks);
}

// ################################################################################

// prepareForNetwork prepares the internal structures for
// writing the request to the network.
std::vector<asio_ns::const_buffer> RequestItem::prepareForNetwork(VSTVersion vstVersion) {
  // setting defaults
  _request->header.setVersion(1); // always set to 1
  if (_request->header.database.empty()) {
    _request->header.database = "_system";
  }

  // Create the message header and store it in the metadata buffer
  _buffer.clear();
  message::requestHeader(_request->header, _buffer);
  assert(header.size() > 0);
  // message header has to go into the first chunk
  asio_ns::const_buffer payload = _request->payload();
  
  // _buffer content will be used as message header
  std::vector<asio_ns::const_buffer> result;
  message::prepareForNetwork(vstVersion, _messageID, _buffer, payload, result);
  return result;
}

namespace parser {

///////////////////////////////////////////////////////////////////////////////////
// receiving vst
///////////////////////////////////////////////////////////////////////////////////

// isChunkComplete returns the length of the chunk that starts at the given
// begin
// if the entire chunk is available.
// Otherwise 0 is returned.
std::size_t isChunkComplete(uint8_t const* const begin,
                            std::size_t const lengthAvailable) {
  // there is not enought to read the length of
  if (lengthAvailable < sizeof(uint32_t)) { // first header field
    return 0;
  }
  // read chunk length
  uint32_t lengthThisChunk = basics::uintFromPersistentLittleEndian<uint32_t>(begin);
  if (lengthAvailable < lengthThisChunk) {
    FUERTE_LOG_VSTCHUNKTRACE << "\nchunk incomplete: " << lengthAvailable << "/"
                             << lengthThisChunk << "(available/len)\n";
    return 0;
  }
  FUERTE_LOG_VSTCHUNKTRACE << "\nchunk complete: " << lengthThisChunk
                           << " bytes\n";
  return lengthThisChunk;
}

// readChunkHeaderVST1_0 reads a chunk header in VST1.0 format.
Chunk readChunkHeaderVST1_0(uint8_t const* bufferBegin) {
  Chunk chunk;

  uint8_t const* hdr = bufferBegin;
  chunk.header._chunkLength = basics::uintFromPersistentLittleEndian<uint32_t>(hdr + 0);
  chunk.header._chunkX = basics::uintFromPersistentLittleEndian<uint32_t>(hdr + 4);
  chunk.header._messageID = basics::uintFromPersistentLittleEndian<uint64_t>(hdr + 8);
  size_t hdrLen = minChunkHeaderSize;

  if (chunk.header.isFirst() && chunk.header.numberOfChunks() > 1) {
    // First chunk, numberOfChunks>1 -> read messageLength
    chunk.header._messageLength =
        basics::uintFromPersistentLittleEndian<uint64_t>(hdr + 16);
    hdrLen = maxChunkHeaderSize; // first chunk header is bigger
  }

  size_t contentLength = 0;
  if (chunk.header._chunkLength >= hdrLen) { // prevent underflow
    chunk.header._chunkLength = chunk.header._chunkLength - hdrLen;
  } else {
    FUERTE_LOG_ERROR << "received invalid chunk length";
  }
  FUERTE_LOG_VSTCHUNKTRACE << "readChunkHeaderVST1_0: got " << contentLength
                           << " data bytes after " << hdrLen << " header bytes\n";
  chunk.body = asio_ns::const_buffer(hdr + hdrLen, contentLength);
  return chunk;
}

// readChunkHeaderVST1_1 reads a chunk header in VST1.1 format.
Chunk readChunkHeaderVST1_1(uint8_t const* bufferBegin) {
  Chunk chunk;

  uint8_t const* hdr = bufferBegin;
  chunk.header._chunkLength = basics::uintFromPersistentLittleEndian<uint32_t>(hdr + 0);
  chunk.header._chunkX = basics::uintFromPersistentLittleEndian<uint32_t>(hdr + 4);
  chunk.header._messageID = basics::uintFromPersistentLittleEndian<uint64_t>(hdr + 8);
  chunk.header._messageLength = basics::uintFromPersistentLittleEndian<uint64_t>(hdr + 16);
  size_t contentLength = 0;
  if (chunk.header._chunkLength >= maxChunkHeaderSize) { // prevent underflow
    contentLength = chunk.header._chunkLength - maxChunkHeaderSize;
  } else {
    FUERTE_LOG_ERROR << "received invalid chunk length";
  }
  FUERTE_LOG_VSTCHUNKTRACE << "readChunkHeaderVST1_1: got " << contentLength
                           << " data bytes after " << maxChunkHeaderSize << " bytes\n";
  chunk.body = asio_ns::const_buffer(hdr + maxChunkHeaderSize, contentLength);
  
  return chunk;
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
    << std::string(reinterpret_cast<char const*>(vpStart), length);
    throw std::runtime_error(std::string("error during validation of incoming VPack (HEADER): ") +
                             e.what());
  }
  VPackSlice slice = VPackSlice(vpStart);
  if (!slice.isArray())  {
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
#ifndef NDEBUG
  header.byteSize = headerSlice.byteSize();  // for debugging
#endif

  header.setVersion( headerSlice.at(0).getNumber<short>()); // version
  assert(headerSlice.at(1).getNumber<int>() == static_cast<int>(MessageType::Request));
  header.database = headerSlice.at(2).copyString();  // databse
  header.restVerb = static_cast<RestVerb>(headerSlice.at(3).getInt());  // rest verb
  header.path = headerSlice.at(4).copyString();           // request (path)
  header.parameters = sliceToStringMap(headerSlice.at(5));  // query params
  header.meta = sliceToStringMap(headerSlice.at(6));        // meta

  return header;
};
  
  
ResponseHeader responseHeaderFromSlice(VPackSlice const& headerSlice) {
  assert(headerSlice.isArray());
  ResponseHeader header;
#ifndef NDEBUG
  header.byteSize = headerSlice.byteSize();  // for debugging
#endif
  
  header.setVersion(headerSlice.at(0).getNumber<short>()); // version
  assert(headerSlice.at(1).getNumber<int>() == static_cast<int>(MessageType::Response));
  header.responseCode = headerSlice.at(2).getNumber<StatusCode>();
  // header.contentType(ContentType::VPack); // empty meta
  if (headerSlice.length() >= 4) {
    VPackSlice meta = headerSlice.at(3);
    assert(meta.isObject());
    if (meta.isObject()) {
      for (auto it : VPackObjectIterator(meta)) {
        std::string key = it.key.copyString();
        boost::algorithm::to_lower(key); // in-place
        header.meta.emplace(std::move(key), it.value.copyString());
      }
    }
  }
  if (header.contentType() == ContentType::Unset) {
    header.contentType(ContentType::VPack);
  }
  return header;
};

// Validates if payload consitsts of valid velocypack slices
std::size_t validateAndCount(uint8_t const* const vpStart, std::size_t length) {
  // start points to the begin of a velocypack
  uint8_t const* cursor = vpStart;
  // there must be at least one velocypack for the header
  VPackValidator validator;
  std::size_t numPayloads = 0;  // fist item is the header
  bool isSubPart = true;

  FUERTE_LOG_VSTTRACE << "buffer length to validate: " << length << std::endl;

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
  FUERTE_LOG_VSTTRACE << std::endl;
  return numPayloads;
}

}  // namespace parser

// add the given chunk to the list of response chunks.
void RequestItem::addChunk(Chunk const& chunk) {
  // Copy _data to response buffer
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::addChunk: adding "
                           << chunk.body.size() << " bytes to buffer\n";
  
  // Gather number of chunk info
  if (chunk.header.isFirst()) {
    _responseNumberOfChunks = chunk.header.numberOfChunks();
    _responseChunks.reserve(_responseNumberOfChunks);
    FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::addChunk: set #chunks to "
    << _responseNumberOfChunks << "\n";
    assert(_buffer.empty());
    if (_buffer.capacity() < chunk.header.messageLength()) {
      _buffer.reserve(chunk.header.messageLength() - _buffer.capacity());
    }
  }
  uint8_t const* begin = reinterpret_cast<uint8_t const*>(chunk.body.data());
  size_t offset = _buffer.size();
  _buffer.append(begin, chunk.body.size());
  // Add chunk to index list
  _responseChunks.push_back(ChunkInfo{chunk.header.index(), offset, chunk.body.size()});
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
                             << std::endl;
    return nullptr;
  }
  if (_responseChunks.size() < _responseNumberOfChunks) {
    // Not all chunks have arrived yet
    FUERTE_LOG_VSTCHUNKTRACE
        << "RequestItem::assemble: not all chunks have arrived" << std::endl;
    return nullptr;
  }
  assert(_responseChunks.size() == _responseNumberOfChunks);

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
        << "RequestItem::assemble: fast-path, chunks are in order" << std::endl;
    return std::unique_ptr<VPackBuffer<uint8_t>>(
        new VPackBuffer<uint8_t>(std::move(_buffer)));
  }

  // We now have all chunks. Sort them by index.
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::assemble: sort chunks" << std::endl;
  std::sort(_responseChunks.begin(), _responseChunks.end(), chunkByIndex);

  // Combine chunk content
  FUERTE_LOG_VSTCHUNKTRACE << "RequestItem::assemble: build response buffer"
                           << std::endl;

  auto buffer = std::unique_ptr<VPackBuffer<uint8_t>>(new VPackBuffer<uint8_t>());
  for (ChunkInfo const& info : _responseChunks) {
    buffer->append(_buffer.data() + info.offset, info.size);
  }

  return buffer;
}
}}}}  // namespace arangodb::fuerte::v1::vst
