// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// FST memory utilities.

#ifndef FST_MEMORY_H_
#define FST_MEMORY_H_

#include <cstddef>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fstream>

namespace fst {

// Default block allocation size.
inline constexpr int kAllocSize = 64;

// Minimum number of allocations per block.
inline constexpr int kAllocFit = 4;

// Base class for MemoryArena that allows (e.g.) MemoryArenaCollection to
// easily manipulate collections of variously sized arenas.
class MemoryArenaBase {
 public:
  virtual ~MemoryArenaBase() {}
  virtual size_t Size() const = 0;
};

namespace internal {

// Allocates 'size' unintialized memory chunks of size object_size from
// underlying blocks of (at least) size 'block_size * object_size'.
// All blocks are freed when this class is deleted. Result of allocate() will
// be aligned to object_size.
template <size_t object_size>
class MemoryArenaImpl : public MemoryArenaBase {
 public:
  static constexpr size_t kObjectSize = object_size;

  explicit MemoryArenaImpl(size_t block_size = kAllocSize)
      : block_size_(block_size * kObjectSize), block_pos_(0) {
    blocks_.push_front(
        fst::make_unique_for_overwrite<std::byte[]>(block_size_));
  }

  void *Allocate(size_t size) {
    const auto byte_size = size * kObjectSize;
    if (byte_size * kAllocFit > block_size_) {
      // Large block; adds new large block.
      blocks_.push_back(
          fst::make_unique_for_overwrite<std::byte[]>(byte_size));
      return blocks_.back().get();
    }
    if (block_pos_ + byte_size > block_size_) {
      // Doesn't fit; adds new standard block.
      block_pos_ = 0;
      blocks_.push_front(
          fst::make_unique_for_overwrite<std::byte[]>(block_size_));
    }
    // Fits; uses current block.
    auto *ptr = &blocks_.front()[block_pos_];
    block_pos_ += byte_size;
    return ptr;
  }

  size_t Size() const override { return kObjectSize; }

 private:
  const size_t block_size_;  // Default block size in bytes.
  size_t block_pos_;         // Current position in block in bytes.
  std::list<std::unique_ptr<std::byte[]>> blocks_;  // List of allocated blocks.
};

}  // namespace internal

template <typename T>
using MemoryArena = internal::MemoryArenaImpl<sizeof(T)>;

// Base class for MemoryPool that allows (e.g.) MemoryPoolCollection to easily
// manipulate collections of variously sized pools.
class MemoryPoolBase {
 public:
  virtual ~MemoryPoolBase() {}
  virtual size_t Size() const = 0;
};

namespace internal {

// Allocates and frees initially uninitialized memory chunks of size
// object_size. Keeps an internal list of freed chunks that are reused (as is)
// on the next allocation if available. Chunks are constructed in blocks of size
// 'pool_size'.
template <size_t object_size>
class MemoryPoolImpl : public MemoryPoolBase {
 public:
  static constexpr size_t kObjectSize = object_size;

  struct Link {
    std::byte buf[kObjectSize];
    Link *next;
  };

  explicit MemoryPoolImpl(size_t pool_size)
      : mem_arena_(pool_size), free_list_(nullptr) {}

  void *Allocate() {
    Link *link;
    if (free_list_ == nullptr) {
      link = static_cast<Link *>(mem_arena_.Allocate(1));
      link->next = nullptr;
    } else {
      link = free_list_;
      free_list_ = link->next;
    }
    return fst::implicit_cast<std::byte *>(link->buf);
  }

  void Free(void *ptr) {
    if (ptr) {
      auto *link = static_cast<Link *>(ptr);
      link->next = free_list_;
      free_list_ = link;
    }
  }

  size_t Size() const override { return kObjectSize; }

 private:
  MemoryArena<Link> mem_arena_;
  Link *free_list_;  // Not owned.

  MemoryPoolImpl(const MemoryPoolImpl &) = delete;
  MemoryPoolImpl &operator=(const MemoryPoolImpl &) = delete;
};

}  // namespace internal

// Allocates and frees initially uninitialized memory chunks of size sizeof(T).
// All memory is freed when the class is deleted. The result of Allocate() will
// be suitably memory-aligned. Combined with placement operator new and destroy
// functions for the T class, this can be used to improve allocation efficiency.
// See nlp/fst/lib/visit.h (global new) and nlp/fst/lib/dfs-visit.h (class new)
// for examples.
template <typename T>
class MemoryPool : public internal::MemoryPoolImpl<sizeof(T)> {
 public:
  // 'pool_size' specifies the size of the initial pool and how it is extended.
  explicit MemoryPool(size_t pool_size = kAllocSize)
      : internal::MemoryPoolImpl<sizeof(T)>(pool_size) {}
};

// Stores a collection of memory arenas.
class MemoryArenaCollection {
 public:
  // 'block_size' specifies the block size of the arenas.
  explicit MemoryArenaCollection(size_t block_size = kAllocSize)
      : block_size_(block_size) {}

  template <typename T>
  MemoryArena<T> *Arena() {
    if (sizeof(T) >= arenas_.size()) arenas_.resize(sizeof(T) + 1);
    auto &arena = arenas_[sizeof(T)];
    if (arena == nullptr) {
      arena = std::make_unique<MemoryArena<T>>(block_size_);
    }
    return down_cast<MemoryArena<T> *>(arena.get());
  }

  size_t BlockSize() const { return block_size_; }

 private:
  size_t block_size_;
  std::vector<std::unique_ptr<MemoryArenaBase>> arenas_;
};

// Stores a collection of memory pools
class MemoryPoolCollection {
 public:
  // 'pool_size' specifies the size of initial pool and how it is extended.
  explicit MemoryPoolCollection(size_t pool_size = kAllocSize)
      : pool_size_(pool_size) {}

  template <typename T>
  MemoryPool<T> *Pool() {
    if (sizeof(T) >= pools_.size()) pools_.resize(sizeof(T) + 1);
    auto &pool = pools_[sizeof(T)];
    if (pool == nullptr) pool = std::make_unique<MemoryPool<T>>(pool_size_);
    return down_cast<MemoryPool<T> *>(pool.get());
  }

  size_t PoolSize() const { return pool_size_; }

 private:
  size_t pool_size_;
  std::vector<std::unique_ptr<MemoryPoolBase>> pools_;
};

// STL allocator using memory arenas. Memory is allocated from underlying
// blocks of size 'block_size * sizeof(T)'. Memory is freed only when all
// objects using this allocator are destroyed and there is otherwise no reuse
// (unlike PoolAllocator).
//
// This allocator has object-local state so it should not be used with splicing
// or swapping operations between objects created with different allocators nor
// should it be used if copies must be thread-safe. The result of allocate()
// will be suitably memory-aligned.
template <typename T>
class BlockAllocator {
 public:
  using Allocator = std::allocator<T>;
  using size_type = typename Allocator::size_type;
  using value_type = typename Allocator::value_type;

  explicit BlockAllocator(size_t block_size = kAllocSize)
      : arenas_(std::make_shared<MemoryArenaCollection>(block_size)) {}

  template <typename U>
  explicit BlockAllocator(const BlockAllocator<U> &arena_alloc)
      : arenas_(arena_alloc.Arenas()) {}

  T *allocate(size_type n, const void *hint = nullptr) {
    if (n * kAllocFit <= kAllocSize) {
      return static_cast<T *>(Arena()->Allocate(n));
    } else {
      return Allocator().allocate(n, hint);
    }
  }

  void deallocate(T *p, size_type n) {
    if (n * kAllocFit > kAllocSize) Allocator().deallocate(p, n);
  }

  std::shared_ptr<MemoryArenaCollection> Arenas() const { return arenas_; }

 private:
  MemoryArena<T> *Arena() { return arenas_->Arena<T>(); }

  std::shared_ptr<MemoryArenaCollection> arenas_;
};

template <typename T, typename U>
bool operator==(const BlockAllocator<T> &alloc1,
                const BlockAllocator<U> &alloc2) {
  return false;
}

template <typename T, typename U>
bool operator!=(const BlockAllocator<T> &alloc1,
                const BlockAllocator<U> &alloc2) {
  return true;
}

// STL allocator using memory pools. Memory is allocated from underlying
// blocks of size 'block_size * sizeof(T)'. Keeps an internal list of freed
// chunks thare are reused on the next allocation.
//
// This allocator has object-local state so it should not be used with splicing
// or swapping operations between objects created with different allocators nor
// should it be used if copies must be thread-safe. The result of allocate()
// will be suitably memory-aligned.
template <typename T>
class PoolAllocator {
 public:
  using Allocator = std::allocator<T>;
  using size_type = typename Allocator::size_type;
  using value_type = typename Allocator::value_type;

  explicit PoolAllocator(size_t pool_size = kAllocSize)
      : pools_(std::make_shared<MemoryPoolCollection>(pool_size)) {}

  template <typename U>
  explicit PoolAllocator(const PoolAllocator<U> &pool_alloc)
      : pools_(pool_alloc.Pools()) {}

  T *allocate(size_type n, const void *hint = nullptr) {
    if (n == 1) {
      return static_cast<T *>(Pool<1>()->Allocate());
    } else if (n == 2) {
      return static_cast<T *>(Pool<2>()->Allocate());
    } else if (n <= 4) {
      return static_cast<T *>(Pool<4>()->Allocate());
    } else if (n <= 8) {
      return static_cast<T *>(Pool<8>()->Allocate());
    } else if (n <= 16) {
      return static_cast<T *>(Pool<16>()->Allocate());
    } else if (n <= 32) {
      return static_cast<T *>(Pool<32>()->Allocate());
    } else if (n <= 64) {
      return static_cast<T *>(Pool<64>()->Allocate());
    } else {
      return Allocator().allocate(n, hint);
    }
  }

  void deallocate(T *p, size_type n) {
    if (n == 1) {
      Pool<1>()->Free(p);
    } else if (n == 2) {
      Pool<2>()->Free(p);
    } else if (n <= 4) {
      Pool<4>()->Free(p);
    } else if (n <= 8) {
      Pool<8>()->Free(p);
    } else if (n <= 16) {
      Pool<16>()->Free(p);
    } else if (n <= 32) {
      Pool<32>()->Free(p);
    } else if (n <= 64) {
      Pool<64>()->Free(p);
    } else {
      Allocator().deallocate(p, n);
    }
  }

  std::shared_ptr<MemoryPoolCollection> Pools() const { return pools_; }

 private:
  template <int n>
  struct TN {
    T buf[n];
  };

  template <int n>
  MemoryPool<TN<n>> *Pool() {
    return pools_->Pool<TN<n>>();
  }

  std::shared_ptr<MemoryPoolCollection> pools_;
};

template <typename T, typename U>
bool operator==(const PoolAllocator<T> &alloc1,
                const PoolAllocator<U> &alloc2) {
  return false;
}

template <typename T, typename U>
bool operator!=(const PoolAllocator<T> &alloc1,
                const PoolAllocator<U> &alloc2) {
  return true;
}

}  // namespace fst

#endif  // FST_MEMORY_H_
