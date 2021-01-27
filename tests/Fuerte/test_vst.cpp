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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"

#include "vst.h"
#include "Basics/Format.h"
#include <velocypack/velocypack-aliases.h>

namespace fu = ::arangodb::fuerte;

// testsuite for VST 1.1

TEST(VelocyStream_11, ChunkHeader) {
  std::string const length("\x1C\0\0\0", 4); // 24 + 4 bytes length
  std::string const chunkX("\x03\0\0\0", 4); // 1 chunk
  std::string const mid("\x01\0\0\0\0\0\0\0", 8); // messageId 1
  std::string const mLength("\x04\0\0\0\0\0\0\0", 8); // messageLength
  std::string const data("\x0a\x0b\x0c\x0d", 4); // messageId 1

  std::string chunkData = length + chunkX + mid + mLength + data;
  ASSERT_EQ(chunkData.size(), 28);
  
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(chunkData.c_str());
  
  fu::vst::Chunk chunk;
  fu::vst::parser::ChunkState state = fu::vst::parser::readChunkVST1_1(chunk, ptr, chunkData.size());

  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  
  ASSERT_EQ(chunk.header._chunkLength, 28);
  
//  auto resultChunk = fu::vst::parser::readChunkHeaderVST1_1(ptr);
  ASSERT_EQ(chunk.header.chunkLength(), 28);
  ASSERT_EQ(chunk.header.messageID(), 1);
  ASSERT_EQ(chunk.header.messageLength(), 4);
  ASSERT_TRUE(chunk.header.isFirst());
  ASSERT_EQ(chunk.header.index(), 0);
  ASSERT_EQ(chunk.header.numberOfChunks(), 1);
  ASSERT_EQ(chunk.body.size(), 4);
  
  ptr = reinterpret_cast<uint8_t const*>(chunk.body.data());
  uint32_t val = fu::basics::uintFromPersistentLE<uint32_t>(ptr);
  ASSERT_EQ(val, static_cast<uint32_t>(0x0d0c0b0a));

  arangodb::velocypack::Buffer<uint8_t> tmp;
  size_t t = chunk.header.writeHeaderToVST1_1(4, tmp);
  ASSERT_EQ(tmp.size(), fu::vst::maxChunkHeaderSize);
  ASSERT_EQ(t, fu::vst::maxChunkHeaderSize);
  ASSERT_LE(fu::vst::maxChunkHeaderSize, chunk.header._chunkLength);
  ASSERT_TRUE(memcmp(tmp.data(), chunkData.data(), fu::vst::maxChunkHeaderSize) == 0);
}

TEST(VelocyStream_11, MultiChunk) {
  std::string const length("\x1C\0\0\0", 4); // 24 + 4 bytes length
  std::string const chunkX_0("\x07\0\0\0", 4); // 3 chunks = ((0b11 << 1) | 1)
  std::string const chunkX_1("\x02\0\0\0", 4); // chunk 1 ((0b01 << 1) | 0)
  std::string const chunkX_2("\x04\0\0\0", 4); // chunk 2 ((0b10 << 1) | 0)
  std::string const mid("\x01\0\0\0\0\0\0\x01", 8); // messageId
  std::string const mLength("\x0C\0\0\0\0\0\0\0", 8); // messageLength
  std::string const data("\x0a\x0b\x0c\x0d", 4); // messageId 1

  std::string chunk0 = length + chunkX_0 + mid + mLength + data;
  std::string chunk1 = length + chunkX_1 + mid + mLength + data;
  std::string chunk2 = length + chunkX_2 + mid + mLength + data;

  ASSERT_EQ(chunk0.size(), 28);
  ASSERT_EQ(chunk1.size(), 28);
  ASSERT_EQ(chunk2.size(), 28);
  
  fu::vst::Chunk resultChunk;

  // chunk 0
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(chunk0.c_str());
  fu::vst::parser::ChunkState state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, chunk0.size());
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);

  ASSERT_EQ(resultChunk.header.chunkLength(), 28);
  ASSERT_EQ(resultChunk.header.messageID(), (1ULL << 56ULL) + 1ULL);
  ASSERT_EQ(resultChunk.header.messageLength(), 0x0C);
  ASSERT_TRUE(resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 0);
  ASSERT_EQ(resultChunk.header.numberOfChunks(), 3);
  ASSERT_EQ(resultChunk.body.size(), 4);
  ptr = reinterpret_cast<uint8_t const*>(resultChunk.body.data());
  uint32_t val = fu::basics::uintFromPersistentLE<uint32_t>(ptr);
  ASSERT_EQ(val, static_cast<uint32_t>(0x0d0c0b0a));

  // chunk 1
  ptr = reinterpret_cast<uint8_t const*>(chunk1.c_str());
  state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, chunk1.size());
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  
  ASSERT_EQ(resultChunk.header.chunkLength(), 28);
  ASSERT_EQ(resultChunk.header.messageID(), (1ULL << 56ULL) + 1ULL);
  ASSERT_EQ(resultChunk.header.messageLength(), 0x0C);
  ASSERT_TRUE(!resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 1);
  ASSERT_EQ(resultChunk.body.size(), 4);
  ptr = reinterpret_cast<uint8_t const*>(resultChunk.body.data());
  val = fu::basics::uintFromPersistentLE<uint32_t>(ptr);
  ASSERT_EQ(val, static_cast<uint32_t>(0x0d0c0b0a));

  // chunk 2
  ptr = reinterpret_cast<uint8_t const*>(chunk2.c_str());
  state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, chunk2.size());
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  ASSERT_EQ(resultChunk.header.chunkLength(), 28);
  ASSERT_EQ(resultChunk.header.messageID(), (1ULL << 56ULL) + 1ULL);
  ASSERT_EQ(resultChunk.header.messageLength(), 0x0C);
  ASSERT_TRUE(!resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 2);
  ASSERT_EQ(resultChunk.body.size(), 4);
  ptr = reinterpret_cast<uint8_t const*>(resultChunk.body.data());
  val = fu::basics::uintFromPersistentLE<uint32_t>(ptr);
  ASSERT_EQ(val, static_cast<uint32_t>(0x0d0c0b0a));
}

TEST(VelocyStream_11, prepareForNetworkSingleChunk) {
  fu::vst::VSTVersion vstVersion = fu::vst::VSTVersion::VST1_1;
  fu::vst::MessageID messageId(1234);
  
  std::string prefix(16, 'a');
  VPackBuffer<uint8_t> buffer;
  buffer.append(prefix.data(), prefix.size());
  
  std::string data(128, 'b');
  asio_ns::const_buffer payload(data.data(), data.size());
  
  std::vector<asio_ns::const_buffer> result;

  fu::vst::message::prepareForNetwork(vstVersion, messageId, buffer, payload, result);
  
  ASSERT_EQ(result.size(), 3);
  
  
  ASSERT_EQ(result[0].size(), fu::vst::maxChunkHeaderSize);
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(result[0].data());
  
  fu::vst::Chunk resultChunk;
  auto state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr,
                                                fu::vst::maxChunkHeaderSize + prefix.size() + data.size());
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  
  ASSERT_EQ(resultChunk.header.chunkLength(), fu::vst::maxChunkHeaderSize + prefix.size() + data.size());
  ASSERT_EQ(resultChunk.header.messageID(), 1234);
  ASSERT_EQ(resultChunk.header.messageLength(), prefix.size() + data.size());
  ASSERT_TRUE(resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 0);
  ASSERT_EQ(resultChunk.body.size(), prefix.size() + data.size());
  
  ASSERT_EQ(result[1].size(), prefix.size());
  ASSERT_EQ(prefix, std::string(reinterpret_cast<char const*>(result[1].data()), result[1].size()));
  
  ASSERT_EQ(result[2].size(), data.size());
  ASSERT_EQ(data, std::string(reinterpret_cast<char const*>(result[2].data()), result[2].size()));
}

TEST(VelocyStream_11, prepareForNetworkMultipleChunks) {
  fu::vst::VSTVersion vstVersion = fu::vst::VSTVersion::VST1_1;
  fu::vst::MessageID messageId(12345);
  
  std::string prefix(16, 'a');
  VPackBuffer<uint8_t> buffer;
  buffer.append(prefix.data(), prefix.size());
  
  std::string data(2 * fu::vst::defaultMaxChunkSize, 'b');
  asio_ns::const_buffer payload(data.data(), data.size());
  
  std::vector<asio_ns::const_buffer> result;

  fu::vst::message::prepareForNetwork(vstVersion, messageId, buffer, payload, result);
  
  ASSERT_EQ(result.size(), 7);
  
  ASSERT_EQ(result[0].size(), fu::vst::maxChunkHeaderSize);
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(result[0].data());
  
  fu::vst::Chunk resultChunk;
  auto state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, fu::vst::defaultMaxChunkSize);
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  ASSERT_EQ(resultChunk.header.chunkLength(), fu::vst::defaultMaxChunkSize);
  ASSERT_EQ(resultChunk.header.messageID(), 12345);
  ASSERT_EQ(resultChunk.header.messageLength(), prefix.size() + (2 * fu::vst::defaultMaxChunkSize));
  ASSERT_TRUE(resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 0);
  ASSERT_EQ(resultChunk.body.size(), fu::vst::defaultMaxChunkSize - fu::vst::maxChunkHeaderSize);
  
  ASSERT_EQ(result[1].size(), prefix.size());
  ASSERT_EQ(prefix, std::string(reinterpret_cast<char const*>(result[1].data()), result[1].size()));
  
  size_t expectedLength0 = fu::vst::defaultMaxChunkSize - prefix.size() - fu::vst::maxChunkHeaderSize;
  ASSERT_EQ(result[2].size(), expectedLength0);
  ASSERT_EQ(data.substr(0, expectedLength0), std::string(reinterpret_cast<char const*>(result[2].data()), result[2].size()));

  ASSERT_EQ(result[3].size(), fu::vst::maxChunkHeaderSize);
  ptr = reinterpret_cast<uint8_t const*>(result[3].data());
  
  state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, fu::vst::defaultMaxChunkSize);
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  
  ASSERT_EQ(resultChunk.header.chunkLength(), fu::vst::defaultMaxChunkSize);
  ASSERT_EQ(resultChunk.header.messageID(), 12345);
  ASSERT_EQ(resultChunk.header.messageLength(), prefix.size() + (2 * fu::vst::defaultMaxChunkSize));
  ASSERT_FALSE(resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 1);
  size_t expectedLength1 = fu::vst::defaultMaxChunkSize - fu::vst::maxChunkHeaderSize;
  ASSERT_EQ(resultChunk.body.size(), expectedLength1);

  ASSERT_EQ(result[4].size(), expectedLength1);
  ASSERT_EQ(data.substr(expectedLength0, expectedLength1), std::string(reinterpret_cast<char const*>(result[4].data()), result[4].size()));
  
  ASSERT_EQ(result[5].size(), fu::vst::maxChunkHeaderSize);
  ptr = reinterpret_cast<uint8_t const*>(result[5].data());
  
  state = fu::vst::parser::readChunkVST1_1(resultChunk, ptr, 3 * fu::vst::maxChunkHeaderSize + prefix.size());
  ASSERT_EQ(state, fu::vst::parser::ChunkState::Complete);
  
  ASSERT_EQ(resultChunk.header.chunkLength(), 3 * fu::vst::maxChunkHeaderSize + prefix.size());
  ASSERT_EQ(resultChunk.header.messageID(), 12345);
  ASSERT_EQ(resultChunk.header.messageLength(), prefix.size() + (2 * fu::vst::defaultMaxChunkSize));
  ASSERT_FALSE(resultChunk.header.isFirst());
  ASSERT_EQ(resultChunk.header.index(), 2);
  ASSERT_EQ(resultChunk.body.size(), 2 * fu::vst::maxChunkHeaderSize + prefix.size());

  size_t expectedLength2 = 2 * fu::vst::maxChunkHeaderSize + prefix.size();
  ASSERT_EQ(result[6].size(), expectedLength2);
  ASSERT_EQ(data.substr(expectedLength1, expectedLength2), std::string(reinterpret_cast<char const*>(result[6].data()), result[6].size()));
}
