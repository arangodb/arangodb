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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H
#define ARANGODB_BASICS_FIXED_SIZE_ALLOCATOR_H 1

#include "Basics/Common.h"
#include "Basics/debugging.h"

namespace arangodb {

class FixedSizeAllocator {
 private:
  class MemoryBlock {
   public:
    MemoryBlock(MemoryBlock const&) = delete;
    MemoryBlock& operator=(MemoryBlock const&) = delete;

    MemoryBlock(size_t itemSize, size_t nrItems)
        : _itemSize(itemSize), _nrAlloc(nrItems), _nrUsed(0), _alloc(nullptr), _data(nullptr) {
      _alloc = new char[(itemSize * nrItems) + 64];

      // adjust to cache line offset (assumed to be 64 bytes)
      _data = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(_alloc) + 63) &
                                      ~((uintptr_t)0x3fu));

      TRI_ASSERT(reinterpret_cast<uintptr_t>(_data) % sizeof(void*) == 0);
    }

    MemoryBlock(MemoryBlock&& other) noexcept
        : _itemSize(other._itemSize),
          _nrAlloc(other._nrAlloc),
          _nrUsed(other._nrUsed),
          _alloc(other._alloc),
          _data(other._data) {
      other._nrAlloc = 0;
      other._nrUsed = 0;
      other._alloc = nullptr;
      other._data = nullptr;
    }

    MemoryBlock& operator=(MemoryBlock&& other) noexcept {
      if (this != &other) {
        TRI_ASSERT(_itemSize == other._itemSize);

        delete[] _alloc;
        _nrAlloc = other._nrAlloc;
        _nrUsed = other._nrUsed;
        _alloc = other._alloc;
        _data = other._data;

        other._nrAlloc = 0;
        other._nrUsed = 0;
        other._alloc = nullptr;
        other._data = nullptr;
      }

      return *this;
    }

    ~MemoryBlock() { delete[] _alloc; }

    void* next() {
      TRI_ASSERT(_nrUsed < _nrAlloc);
      return static_cast<void*>(_data + (_itemSize * _nrUsed++));
    }

    inline bool full() const { return _nrUsed == _nrAlloc; }

    size_t memoryUsage() const {
      return (_data - _alloc) + _itemSize * _nrAlloc;
    }

   private:
    size_t const _itemSize;
    size_t _nrAlloc;
    size_t _nrUsed;
    char* _alloc;
    char* _data;
  };

 public:
  FixedSizeAllocator(FixedSizeAllocator const&) = delete;
  FixedSizeAllocator& operator=(FixedSizeAllocator const&) = delete;

  explicit FixedSizeAllocator(size_t itemSize)
      : _itemSize(itemSize), _freelist(nullptr) {
    _blocks.reserve(4);

#ifndef TRI_UNALIGNED_ACCESS
    // align _itemSize to a multiple of sizeof(void*) on system that require it
    _itemSize +=
        (_itemSize % sizeof(void*) == 0 ? 0 : sizeof(void*) - _itemSize % sizeof(void*));
#endif
  }

  ~FixedSizeAllocator() = default;

  void* allocate() {
    if (_freelist != nullptr) {
      void* element = _freelist;
      _freelist = *reinterpret_cast<void**>(_freelist);
      return element;
    }

    if (_blocks.empty() || _blocks.back()->full()) {
      allocateBlock();
    }
    TRI_ASSERT(!_blocks.empty());
    TRI_ASSERT(!_blocks.back()->full());

    return _blocks.back()->next();
  }

  void deallocateAll() {
    _blocks.clear();
    _freelist = nullptr;
  }

  void deallocate(void* value) noexcept {
    *reinterpret_cast<void**>(value) = _freelist;
    _freelist = value;
  }

  size_t memoryUsage() const {
    size_t total = 0;
    for (auto const& it : _blocks) {
      total += it->memoryUsage();
    }
    return total;
  }

 private:
  void allocateBlock() {
    size_t const size = 128 << (std::min)(size_t(8), _blocks.size());
    auto block = std::make_unique<MemoryBlock>(_itemSize, size);
    _blocks.emplace_back(std::move(block));
  }

  std::vector<std::unique_ptr<MemoryBlock>> _blocks;
  size_t _itemSize;
  void* _freelist;
};

}  // namespace arangodb

#endif
