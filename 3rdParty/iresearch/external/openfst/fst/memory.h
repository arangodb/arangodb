// memory.h

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: riley@google.com (Michael Riley)
//
// \file
// FST memory utilities

#ifndef FST_LIB_MEMORY_H__
#define FST_LIB_MEMORY_H__

#include <list>
#include <memory>
#include <utility>
using std::pair; using std::make_pair;

#include <fst/types.h>

namespace fst {

//
// MEMORY ALLOCATION UTILITIES
//

// Default block allocation size
const int kAllocSize = 64;

// Minimum number of allocations per block
const int kAllocFit = 4;

// Base class for MemoryArena that allows e.g. MemoryArenaCollection to
// easily manipulate collections of variously sized arenas.
class MemoryArenaBase {
 public:
  virtual ~MemoryArenaBase() { }
  virtual size_t Size() const = 0;
};

// Allocates 'size' unintialized memory chunks of size sizeof(T) from
// underlying blocks of (at least) size 'block_size * sizeof(T)'.  All
// blocks are freed when this class is deleted. Result of allocate()
// will be aligned to sizeof(T).
template <typename T>
class MemoryArena : public MemoryArenaBase {
 public:
  explicit MemoryArena(size_t block_size = kAllocSize)
      : block_size_(block_size * sizeof(T)),
        block_pos_(0) {
    blocks_.push_front(new char[block_size_]);
  }

  virtual ~MemoryArena() {
    for (typename list<char *>::iterator it = blocks_.begin();
         it != blocks_.end();
         ++it) {
      delete[] *it;
    }
  }

  void* Allocate(size_t size) {
    size_t byte_size = size * sizeof(T);
    if (byte_size * kAllocFit > block_size_) {
      // large block; add new large block
      char *ptr = new char[byte_size];
      blocks_.push_back(ptr);
      return ptr;
    }

    if (block_pos_ + byte_size > block_size_) {
      // Doesn't fit; add new standard block
      char *ptr = new char[block_size_];
      block_pos_ = 0;
      blocks_.push_front(ptr);
    }

    // Fits; use current block
    char *ptr = blocks_.front() + block_pos_;
    block_pos_ += byte_size;
    return ptr;
  }

  virtual size_t Size() const { return sizeof(T); }

 private:
  size_t block_size_;     // default block size in bytes
  size_t block_pos_;      // current position in block in bytes
  list<char *> blocks_;   // list of allocated blocks

  DISALLOW_COPY_AND_ASSIGN(MemoryArena);
};


// Base class for MemoryPool that allows e.g. MemoryPoolCollection to
// easily manipulate collections of variously sized pools.
class MemoryPoolBase {
 public:
  virtual ~MemoryPoolBase() { }
  virtual size_t Size() const = 0;
};

// Allocates and frees initially uninitialized memory chunks of size
// sizeof(T).  Keeps an internal list of freed chunks that are reused
// (as is) on the next allocation if available. Chunks are constructed
// in blocks of size 'pool_size'.  All memory is freed when the class is
// deleted. The result of Allocate() will be suitably memory-aligned.
//
// Combined with placement operator new and destroy fucntions for the
// T class, this can be used to improve allocation efficiency.  See
// nlp/fst/lib/visit.h (global new) and
// nlp/fst/lib/dfs-visit.h (class new) for examples of this usage.
template <typename T>
class MemoryPool : public MemoryPoolBase {
 public:
  struct Link {
    char buf[sizeof(T)];
    Link *next;
  };

  // 'pool_size' specifies the size of the initial pool and how it is extended
  explicit MemoryPool(size_t pool_size = kAllocSize)
      : mem_arena_(pool_size), free_list_(0) {
  }

  virtual ~MemoryPool() { }

  void* Allocate() {
    if (free_list_ == 0) {
      Link *link = static_cast<Link *>(mem_arena_.Allocate(1));
      link->next = 0;
      return link;
    } else {
      Link *link = free_list_;
      free_list_ = link->next;
      return link;
    }
  }

  void Free(void* ptr) {
    if (ptr) {
      Link *link = static_cast<Link *>(ptr);
      link->next = free_list_;
      free_list_ = link;
    }
  }

  virtual size_t Size() const { return sizeof(T); }

 private:
  MemoryArena<Link> mem_arena_;
  Link *free_list_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPool);
};

//
// MEMORY ALLOCATION COLLECTION UTILITIES
//

// Stores a collection of memory arenas
class MemoryArenaCollection {
 public:
  // 'block_size' specifies the block size of the arenas
  explicit MemoryArenaCollection(size_t block_size = kAllocSize)
      : block_size_(block_size), ref_count_(1) { }

  ~MemoryArenaCollection() {
    for (size_t i = 0; i < arenas_.size(); ++i)
      delete arenas_[i];
  }

  template <typename T>
  MemoryArena<T> *Arena() {
    if (sizeof(T) >= arenas_.size())
      arenas_.resize(sizeof(T) + 1, 0);
    MemoryArenaBase *arena = arenas_[sizeof(T)];
    if (arena == 0) {
      arena = new MemoryArena<T>(block_size_);
      arenas_[sizeof(T)] = arena;
    }
    return static_cast<MemoryArena<T> *>(arena);
  }

  size_t BlockSize() const { return block_size_; }

  size_t RefCount() const { return ref_count_; }
  size_t IncrRefCount() { return ++ref_count_; }
  size_t DecrRefCount() { return --ref_count_; }

 private:
  size_t block_size_;
  size_t ref_count_;
  vector<MemoryArenaBase *> arenas_;

  DISALLOW_COPY_AND_ASSIGN(MemoryArenaCollection);
};


// Stores a collection of memory pools
class MemoryPoolCollection {
 public:
  // 'pool_size' specifies the size of initial pool and how it is extended
  explicit MemoryPoolCollection(size_t pool_size = kAllocSize)
      : pool_size_(pool_size), ref_count_(1) { }

  ~MemoryPoolCollection() {
    for (size_t i = 0; i < pools_.size(); ++i)
      delete pools_[i];
  }

  template <typename T>
  MemoryPool<T> *Pool() {
    if (sizeof(T) >= pools_.size())
      pools_.resize(sizeof(T) + 1, 0);
    MemoryPoolBase *pool = pools_[sizeof(T)];
    if (pool == 0) {
      pool = new MemoryPool<T>(pool_size_);
      pools_[sizeof(T)] = pool;
    }
    return static_cast<MemoryPool<T> *>(pool);
  }

  size_t PoolSize() const { return pool_size_; }

  size_t RefCount() const { return ref_count_; }
  size_t IncrRefCount() { return ++ref_count_; }
  size_t DecrRefCount() { return --ref_count_; }

 private:
  size_t pool_size_;
  size_t ref_count_;
  vector<MemoryPoolBase *> pools_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPoolCollection);
};

//
// STL MEMORY ALLOCATORS
//

// STL allocator using memory arenas. Memory is allocated from
// underlying blocks of size 'block_size * sizeof(T)'. Memory is freed
// only when all objects using this allocator are destroyed and there
// is otherwise no reuse (unlike PoolAllocator).
//
// This allocator has object-local state so it should not be used with
// splicing or swapping operations between objects created with
// different allocators nor should it be used if copies must be
// thread-safe. The result of allocate() will be suitably
// memory-aligned.
template <typename T>
class BlockAllocator {
 public:
  typedef std::allocator<T> A;
  typedef typename A::size_type size_type;
  typedef typename A::difference_type difference_type;
  typedef typename A::pointer pointer;
  typedef typename A::const_pointer const_pointer;
  typedef typename A::reference reference;
  typedef typename A::const_reference const_reference;
  typedef typename A::value_type value_type;

  template <typename U> struct rebind { typedef BlockAllocator<U> other; };

  explicit BlockAllocator(size_t block_size = kAllocSize)
  : arenas_(new MemoryArenaCollection(block_size)) { }

  BlockAllocator(const BlockAllocator<T> &arena_alloc)
      : arenas_(arena_alloc.Arenas()) { Arenas()->IncrRefCount(); }

  template <typename U>
  BlockAllocator(const BlockAllocator<U> &arena_alloc)
      : arenas_(arena_alloc.Arenas()) { Arenas()->IncrRefCount(); }

  ~BlockAllocator() {
    if (Arenas()->DecrRefCount() == 0)
      delete Arenas();
  }

  pointer address(reference ref) const { return A().address(ref); }

  const_pointer address(const_reference ref) const {
    return A().address(ref);
  }

  size_type max_size() const { return A().max_size(); }

  template <class U, class... Args>
  void construct(U* p, Args&&... args) {
    A().construct(p, std::forward<Args>(args)...);
  }

  void destroy(pointer p) { A().destroy(p); }

  pointer allocate(size_type n, const void *hint = 0) {
    if (n * kAllocFit <= kAllocSize) {
      return static_cast<pointer>(Arena()->Allocate(n));
    } else {
      return A().allocate(n, hint);
    }
  }

  void deallocate(pointer p, size_type n) {
    if (n * kAllocFit > kAllocSize) {
      A().deallocate(p, n);
    }
  }

  MemoryArenaCollection *Arenas() const { return arenas_; }

 private:
  MemoryArena<T> *Arena() { return arenas_->Arena<T>(); }

  MemoryArenaCollection *arenas_;

  BlockAllocator<T> operator=(const BlockAllocator<T> &);
};


template <typename T, typename U>
bool operator==(const BlockAllocator<T>& alloc1,
                const BlockAllocator<U>& alloc2) { return false; }

template <typename T, typename U>
bool operator!=(const BlockAllocator<T>& alloc1,
                const BlockAllocator<U>& alloc2) { return true; }


// STL allocator using memory pools. Memory is allocated from underlying
// blocks of size 'block_size * sizeof(T)'. Keeps an internal list of freed
// chunks thare are reused on the next allocation.
//
// This allocator has object-local state so it should not be used with
// splicing or swapping operations between objects created with
// different allocators nor should it be used if copies must be
// thread-safe. The result of allocate() will be suitably
// memory-aligned.
template <typename T>
class PoolAllocator {
 public:
  typedef std::allocator<T> A;
  typedef typename A::size_type size_type;
  typedef typename A::difference_type difference_type;
  typedef typename A::pointer pointer;
  typedef typename A::const_pointer const_pointer;
  typedef typename A::reference reference;
  typedef typename A::const_reference const_reference;
  typedef typename A::value_type value_type;

  template <typename U> struct rebind { typedef PoolAllocator<U> other; };

  explicit PoolAllocator(size_t pool_size = kAllocSize)
  : pools_(new MemoryPoolCollection(pool_size)) { }

  PoolAllocator(const PoolAllocator<T> &pool_alloc)
      : pools_(pool_alloc.Pools()) { Pools()->IncrRefCount(); }

  template <typename U>
  PoolAllocator(const PoolAllocator<U> &pool_alloc)
      : pools_(pool_alloc.Pools()) { Pools()->IncrRefCount(); }

  ~PoolAllocator() {
    if (Pools()->DecrRefCount() == 0)
      delete Pools();
  }

  pointer address(reference ref) const { return A().address(ref); }

  const_pointer address(const_reference ref) const {
    return A().address(ref);
  }

  size_type max_size() const { return A().max_size(); }

  template <class U, class... Args>
  void construct(U* p, Args&&... args) {
    A().construct(p, std::forward<Args>(args)...);
  }

  void destroy(pointer p) { A().destroy(p); }

  pointer allocate(size_type n, const void *hint = 0) {
    if (n == 1) {
      return static_cast<pointer>(Pool<1>()->Allocate());
    } else if (n == 2) {
      return static_cast<pointer>(Pool<2>()->Allocate());
    } else if (n <= 4) {
      return static_cast<pointer>(Pool<4>()->Allocate());
    }  else if (n <= 8) {
      return static_cast<pointer>(Pool<8>()->Allocate());
    } else if (n <= 16) {
      return static_cast<pointer>(Pool<16>()->Allocate());
    } else if (n <= 32) {
      return static_cast<pointer>(Pool<32>()->Allocate());
    } else if (n <= 64) {
      return static_cast<pointer>(Pool<64>()->Allocate());
    } else {
      return A().allocate(n, hint);
    }
  }

  void deallocate(pointer p, size_type n) {
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
      A().deallocate(p, n);
    }
  }

  MemoryPoolCollection *Pools() const { return pools_; }

 private:
  template <int n> struct TN { T buf[n]; };

  template <int n>
  MemoryPool< TN<n> > *Pool() { return pools_->Pool< TN<n> >(); }

  MemoryPoolCollection *pools_;

  PoolAllocator<T> operator=(const PoolAllocator<T> &);
};

template <typename T, typename U>
bool operator==(const PoolAllocator<T>& alloc1,
                const PoolAllocator<U>& alloc2) { return false; }

template <typename T, typename U>
bool operator!=(const PoolAllocator<T>& alloc1,
                const PoolAllocator<U>& alloc2) { return true; }

}  // namespace fst

#endif  // FST_LIB_MEMORY_H__
