////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H
#define ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H 1

#include "Basics/Common.h"
#include "Basics/debugging.h"

#include <vector>

namespace arangodb {

template <typename T>
class FixedSizeAllocator {
 private:
  class MemoryBlock {
   public:
    MemoryBlock(MemoryBlock const&) = delete;
    MemoryBlock& operator=(MemoryBlock const&) = delete;

    MemoryBlock(size_t numItems)
        : _numAllocated(0), _numUsed(0), _alloc(nullptr), _data(nullptr) {
      TRI_ASSERT(numItems >= 32);
      _alloc = new char[(sizeof(T) * numItems) + 64];

      _numAllocated = numItems;

      // adjust to cache line offset (assumed to be 64 bytes)
      _data = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(_alloc) + 63) &
                                      ~((uintptr_t)0x3fu));

      TRI_ASSERT(reinterpret_cast<uintptr_t>(_data) % sizeof(void*) == 0);
    }

    ~MemoryBlock() noexcept { 
      // free all items
      for (size_t i = 0; i < _numUsed; ++i) {
        T* p =  reinterpret_cast<T*>(_data + (sizeof(T) * i));
        // call destructor
        p->~T();
      }
      delete[] _alloc; 
    }

    /// @brief return memory address for next in-place object construction
    T* nextSlot() noexcept {
      TRI_ASSERT(_numUsed < _numAllocated);
      return reinterpret_cast<T*>(_data + (sizeof(T) * _numUsed++));
    }
    
    bool full() const noexcept { return _numUsed == _numAllocated; }

    size_t numUsed() const noexcept { return _numUsed; }

   private:
    size_t _numAllocated;
    size_t _numUsed;
    char* _alloc;
    char* _data;
  };

 public:
  FixedSizeAllocator(FixedSizeAllocator const&) = delete;
  FixedSizeAllocator& operator=(FixedSizeAllocator const&) = delete;

  FixedSizeAllocator() {
    _blocks.reserve(2);
  }

  ~FixedSizeAllocator() = default;

  T* nextSlot() {
    if (_blocks.empty() || _blocks.back()->full()) {
      allocateBlock();
    }
    TRI_ASSERT(!_blocks.empty());
    TRI_ASSERT(!_blocks.back()->full());

    return _blocks.back()->nextSlot();
  }

  void clear() {
    _blocks.clear();
  }

  size_t numUsed() const noexcept {
    size_t used = 0;
    for (auto const& block : _blocks) {
      used += block->numUsed();
    }
    return used;
  }

 private:
  void allocateBlock() {
    // minimum block size is for 64 items
    // maximum block size is for 16384 items
    size_t const numItems = 16 << std::min<size_t>(6, std::max<size_t>(2, _blocks.size()));
    _blocks.emplace_back(std::make_unique<MemoryBlock>(numItems));
  }

  std::vector<std::unique_ptr<MemoryBlock>> _blocks;
};

}  // namespace arangodb

#endif
