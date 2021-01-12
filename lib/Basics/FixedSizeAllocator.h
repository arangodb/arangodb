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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H
#define ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H

#include "Basics/Common.h"
#include "Basics/debugging.h"

#include <algorithm>
#include <vector>

namespace arangodb {

template <typename T>
class FixedSizeAllocator {
 private:
  // sizeof(T) is always a multiple of alignof(T) unless T is packed (which should never be the case here)!
  static_assert((sizeof(T) % alignof(T)) == 0);

  class MemoryBlock {
   public:
    MemoryBlock(MemoryBlock const&) = delete;
    MemoryBlock& operator=(MemoryBlock const&) = delete;

    MemoryBlock(size_t numItems, T* data) noexcept
        : _numAllocated(numItems), _numUsed(0), _data(data), _next() {
      TRI_ASSERT(numItems >= 32);

      // the result should at least be 8-byte aligned
      TRI_ASSERT(reinterpret_cast<uintptr_t>(_data) % sizeof(void*) == 0);
      TRI_ASSERT(reinterpret_cast<uintptr_t>(_data) % 64u == 0);
    }

    ~MemoryBlock() noexcept { 
      // destroy all items
      for (size_t i = 0; i < _numUsed; ++i) {
        T* p =  _data + i;
        // call destructor for each item
        p->~T();
      }
    }

    MemoryBlock* getNextBlock() const noexcept {
      return _next;
    }

    void setNextBlock(MemoryBlock* next) noexcept {
      _next = next;
    }

    /// @brief return memory address for next in-place object construction.
    T* nextSlot() noexcept {
      TRI_ASSERT(_numUsed < _numAllocated);
      return _data + _numUsed++;
    }

    /// @brief roll back the effect of nextSlot()
    void rollbackSlot() noexcept {
      TRI_ASSERT(_numUsed > 0);
      --_numUsed;
    }
    
    bool full() const noexcept { return _numUsed == _numAllocated; }

    size_t numUsed() const noexcept { return _numUsed; }

   private:
    size_t _numAllocated;
    size_t _numUsed;
    T* _data;
    MemoryBlock* _next;
  };

 public:
  FixedSizeAllocator(FixedSizeAllocator const&) = delete;
  FixedSizeAllocator& operator=(FixedSizeAllocator const&) = delete;

  FixedSizeAllocator() = default;
  ~FixedSizeAllocator() noexcept {
    clear();
  }

  template <typename... Args>
  T* allocate(Args&&... args) {
    if (_head == nullptr || _head->full()) {
      allocateBlock();
    }
    TRI_ASSERT(_head != nullptr);
    TRI_ASSERT(!_head->full());
   
    try {
      T* p = _head->nextSlot();
      return new (p) T(std::forward<Args>(args)...);
    } catch (...) {
      _head->rollbackSlot();
      throw;
    }
  }

  void clear() noexcept {
    auto* block = _head;
    while (block != nullptr) {
      auto* next = block->getNextBlock();
      block->~MemoryBlock();
      ::operator delete(block);
      block = next;
    }
    _head = nullptr;
  }

  /// @brief return the total number of used elements, in all blocks
  size_t numUsed() const noexcept {
    size_t used = 0;
    auto* block = _head;
    while (block != nullptr) {
      used += block->numUsed();
      block = block->getNextBlock();
    }
    return used;
  }

 private:
  void allocateBlock() {
    // minimum block size is for 64 items
    // maximum block size is for 4096 items
    size_t const numItems = 64 << std::min<size_t>(6, _numBlocks);

    // assumption is that the size of a cache line is at least 64,
    // so we are allocating 64 bytes in addition
    auto const dataSize = sizeof(T) * numItems + 64;
    void* p = ::operator new(sizeof(MemoryBlock) + dataSize);

    // adjust memory address to cache line offset (assumed to be 64 bytes)
    auto* data = reinterpret_cast<T*>(
      (reinterpret_cast<uintptr_t>(p) + sizeof(MemoryBlock) + 63u) & ~(uintptr_t(63u)));

    // creating a MemoryBlock is noexcept, it should not fail
    new (p)MemoryBlock(numItems, data);

    MemoryBlock* block = static_cast<MemoryBlock*>(p);
    block->setNextBlock(_head);
    _head = block;
    ++_numBlocks;
  }

  MemoryBlock* _head{nullptr};
  std::size_t _numBlocks{0};
};

}  // namespace arangodb

#endif
