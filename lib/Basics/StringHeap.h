////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <cstddef>
#include <vector>

#include "Basics/Common.h"

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class HashedStringRef;
}

class StringHeap {
 public:
  StringHeap(StringHeap const&) = delete;
  StringHeap(StringHeap&&) = default;
  StringHeap& operator=(StringHeap const&) = delete;
  StringHeap& operator=(StringHeap&&) = delete;

  explicit StringHeap(ResourceMonitor& resourceMonitor, size_t blockSize);
  ~StringHeap();

  /// @brief register a string - implemented for std::string_view and
  /// HashedStringRef
  template<typename T>
  T registerString(T str);

  /// @brief clear all data from the StringHeap, not releasing any occupied
  /// memory the caller must make sure that nothing points into the data of the
  /// StringHeap when calling this method
  void clear() noexcept;

 private:
  /// @brief allocate a new block of memory
  void allocateBlock();

  /// @brief register a string
  char const* registerString(char const* data, size_t length);

 private:
  /// @brief memory usage tracker
  ResourceMonitor& _resourceMonitor;

  /// @brief already allocated string blocks
  std::vector<char*> _blocks;

  /// @brief size of each block
  size_t _blockSize;

  /// @brief offset into current block
  char* _current;

  /// @brief end of current block
  char* _end;
};
}  // namespace arangodb
