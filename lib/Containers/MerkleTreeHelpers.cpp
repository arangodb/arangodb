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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstring>
#include <string>

#include "MerkleTreeHelpers.h"
#include "Basics/debugging.h"

namespace {
/// @brief an empty shard that is reused when serializing empty shards
struct EmptyShard {
  EmptyShard()
      : buffer(arangodb::containers::MerkleTreeBase::Data::buildShard(
            arangodb::containers::MerkleTreeBase::ShardSize)) {}

  char* data() const { return reinterpret_cast<char*>(buffer.get()); }

  arangodb::containers::MerkleTreeBase::Data::ShardType buffer;
};

}  // namespace

/// @brief an empty shard that is reused when serializing empty shards
EmptyShard const emptyShard;

namespace arangodb::containers::helpers {

SnappyStringAppendSink::SnappyStringAppendSink(std::string& output)
    : output(output) {}

void SnappyStringAppendSink::Append(const char* bytes, size_t n) {
  output.append(bytes, n);
}

MerkleTreeSnappySource::MerkleTreeSnappySource(
    std::uint64_t numberOfShards, std::uint64_t allocationSize,
    arangodb::containers::MerkleTreeBase::Data const& data)
    : numberOfShards(numberOfShards),
      data(data),
      bytesRead(0),
      bytesLeftToRead(allocationSize) {}

size_t MerkleTreeSnappySource::Available() const { return bytesLeftToRead; }

char const* MerkleTreeSnappySource::Peek(size_t* len) {
  if (bytesRead < arangodb::containers::MerkleTreeBase::MetaSize) {
    TRI_ASSERT(bytesRead == 0);
    *len = arangodb::containers::MerkleTreeBase::MetaSize;
    return reinterpret_cast<char const*>(&data.meta);
  }

  TRI_ASSERT(bytesRead >= arangodb::containers::MerkleTreeBase::MetaSize);
  std::uint64_t shard =
      (bytesRead - arangodb::containers::MerkleTreeBase::MetaSize) /
      arangodb::containers::MerkleTreeBase::ShardSize;
  std::uint64_t offsetInShard =
      (bytesRead - arangodb::containers::MerkleTreeBase::MetaSize) %
      arangodb::containers::MerkleTreeBase::ShardSize;

  if (shard < numberOfShards) {
    *len = arangodb::containers::MerkleTreeBase::ShardSize - offsetInShard;
    if (shard >= data.shards.size() || data.shards[shard] == nullptr) {
      // for compressing empty shards, we use the static EmptyShard object,
      // which consists of one shard that is completely empty
      return ::emptyShard.data() + offsetInShard;
    }
    return reinterpret_cast<char const*>(data.shards[shard].get()) +
           offsetInShard;
  }
  // no more data
  *len = 0;
  return "";
}

void MerkleTreeSnappySource::Skip(size_t n) {
  bytesRead += n;
  TRI_ASSERT(n <= bytesLeftToRead);
  bytesLeftToRead -= n;
}

}  // namespace arangodb::containers::helpers
