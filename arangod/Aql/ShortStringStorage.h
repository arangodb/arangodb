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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SHORT_STRING_STORAGE_H
#define ARANGOD_AQL_SHORT_STRING_STORAGE_H 1

#include <cstddef>
#include <memory>
#include <vector>

#include "Basics/Common.h"

namespace arangodb {
struct ResourceMonitor;

namespace aql {

class ShortStringStorage {
 public:
  ShortStringStorage(ShortStringStorage const&) = delete;
  ShortStringStorage& operator=(ShortStringStorage const&) = delete;

  ShortStringStorage(arangodb::ResourceMonitor& resourceMonitor, size_t);

  ~ShortStringStorage();

  /// @brief register a short string
  char* registerString(char const* p, size_t length);

  /// @brief register a short string, unescaping it
  char* unescape(char const* p, size_t length, size_t* outLength);

 private:
  /// @brief allocate a new block of memory
  void allocateBlock();

 public:
  /// @brief maximum length of strings in short string storage
  static constexpr size_t maxStringLength = 127;

 private:
  arangodb::ResourceMonitor& _resourceMonitor;

  /// @brief already allocated string blocks
  std::vector<std::unique_ptr<char[]>> _blocks;

  /// @brief size of each block
  size_t const _blockSize;

  /// @brief offset into current block
  char* _current;

  /// @brief end of current block
  char* _end;
};
}  // namespace aql
}  // namespace arangodb

#endif
