////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

  // register a short string
  char* registerString(char const* p, size_t length);

  // register a short string, unescaping it
  char* unescape(char const* p, size_t length, size_t* outLength);

  // frees all blocks
  void clear() noexcept;

  // frees all blocks but the first one. we keep one block to avoid
  // later memory re-allocations.
  void clearMost() noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  size_t usedBlocks() const noexcept { return _blocks.size(); }
#endif

  // maximum length of strings in short string storage
  static constexpr size_t maxStringLength = 127;

 private:
  // allocate a new block of memory
  void allocateBlock();

  // resource monitor used for tracking allocations/deallocations
  arangodb::ResourceMonitor& _resourceMonitor;

  // already allocated string blocks
  std::vector<std::unique_ptr<char[]>> _blocks;

  // size of each block
  size_t const _blockSize;

  // offset into current block
  char* _current;

  // end of current block
  char* _end;
};
}  // namespace aql
}  // namespace arangodb
