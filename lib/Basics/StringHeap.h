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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_STRING_HEAP_H
#define ARANGODB_BASICS_STRING_HEAP_H 1

#include "Basics/Common.h"

#include <velocypack/StringRef.h>
#include <vector>

namespace arangodb {
struct ResourceMonitor;

class StringHeap {
 public:
  StringHeap(StringHeap const&) = delete;
  StringHeap& operator=(StringHeap const&) = delete;

  explicit StringHeap(ResourceMonitor& resourceMonitor, size_t blockSize);
  ~StringHeap();

  /// @brief register a string
  arangodb::velocypack::StringRef registerString(char const* ptr, size_t length);
  arangodb::velocypack::StringRef registerString(arangodb::velocypack::StringRef const& str) {
    return registerString(str.data(), str.size());
  }
 
  /// @brief clear all data from the StringHeap, not releasing any occupied memory 
  /// the caller must make sure that nothing points into the data of the StringHeap
  /// when calling this method
  void clear() noexcept;
  
 private:
  /// @brief allocate a new block of memory
  void allocateBlock();

 private:
  /// @brief memory usage tracker
  ResourceMonitor& _resourceMonitor;

  /// @brief already allocated string blocks
  std::vector<char*> _blocks;

  /// @brief size of each block
  size_t const _blockSize;

  /// @brief offset into current block
  char* _current;

  /// @brief end of current block
  char* _end;
};
}  // namespace arangodb

#endif
