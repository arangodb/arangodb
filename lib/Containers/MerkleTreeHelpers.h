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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <snappy-sinksource.h>

#include "MerkleTree.h"

namespace arangodb::containers::helpers {

class SnappyStringAppendSink : public snappy::Sink {
 public:
  explicit SnappyStringAppendSink(std::string& output);

  void Append(const char* bytes, size_t n) override;

 private:
  std::string& output;
};

/// @brief helper class for compressing a Merkle tree using Snappy
class MerkleTreeSnappySource : public snappy::Source {
 public:
  explicit MerkleTreeSnappySource(std::uint64_t numberOfShards, 
                                  std::uint64_t allocationSize,
                                  arangodb::containers::MerkleTreeBase::Data const& data);
  
  size_t Available() const override;
  
  char const* Peek(size_t* len) override;

  void Skip(size_t n) override;
   
 private:
  std::uint64_t const numberOfShards;
  arangodb::containers::MerkleTreeBase::Data const& data;
  std::uint64_t bytesRead;
  std::uint64_t bytesLeftToRead;
};

}  // namespace
