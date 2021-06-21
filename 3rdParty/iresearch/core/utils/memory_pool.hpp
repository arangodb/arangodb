////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MEMORY_POOL_H
#define IRESEARCH_MEMORY_POOL_H

#include <map>
#include <memory>

#include "shared.hpp"
#include "ebo.hpp"
#include "map_utils.hpp"
#include "noncopyable.hpp"
#include "memory.hpp"

namespace iresearch {
namespace memory {

///////////////////////////////////////////////////////////////////////////////
/// @class freelist
///////////////////////////////////////////////////////////////////////////////
class freelist : private util::noncopyable {
 private:
  union slot {
    char* mem;
    slot* next;
  };

 public:
  static const size_t MIN_SIZE = sizeof(slot*);
  static const size_t MIN_ALIGN = alignof(slot*);

  freelist() = default;
  freelist(freelist&& rhs) noexcept
    : head_(rhs.head_) {
    rhs.head_ = nullptr;
  }
  freelist& operator=(freelist&& rhs) noexcept {
    if (this != &rhs) {
      head_ = rhs.head_;
      rhs.head_ = nullptr;
    }
    return *this;
  }

  // push the specified element 'p' to the stack denoted by 'head'
  FORCE_INLINE void push(void* p) noexcept {
    assert(p);
    auto* free = static_cast<slot*>(p);
    free->next = head_;
    head_ = free;
  }

  FORCE_INLINE void assign(void* p, const size_t slot_size, const size_t count) noexcept {
    segregate(p, slot_size, count);
    head_ = static_cast<slot*>(p);
  }

  FORCE_INLINE void push_n(void* p, const size_t slot_size, const size_t count) noexcept {
    assert(p);

    auto* end = segregate(p, slot_size, count);
    end->next = head_;
    head_ = static_cast<slot*>(p);
  }

  FORCE_INLINE bool empty() const noexcept {
    return !head_;
  }

  // pops an element from the stack denoted by 'head'
  FORCE_INLINE void* pop() noexcept {
    assert(head_);
    void* p = head_;
    head_ = head_->next;
    return p;
  }

  void* pop_n(const size_t slot_size, size_t count) noexcept {
    slot* begin = reinterpret_cast<slot*>(&head_);
    slot* end;
    while (nullptr == (end = try_pop_n(begin, slot_size, count))) {
      if (!begin) {
        return nullptr;
      }
    }

    auto* const ret = begin;
    begin->next = end->next;
    return ret;
  }

 private:
  static slot* segregate(void* p, const size_t slot_size, const size_t count) noexcept {
    assert(p);

    auto* head = static_cast<slot*>(p);
    auto* begin = static_cast<char*>(p);
    auto* const end = begin + slot_size*count;
    while ((begin += slot_size) < end) {
      head = head->next = reinterpret_cast<slot*>(begin);
    }
    assert(begin == end);

    head->next = nullptr; // mark last element
    return head; // return last element
  }

  // tries to find 'n' sequential slots of size 'slot_size' starting from the pointer
  // denoted by 'begin'
  // on success, puts the beginning of the found range into the 'begin'
  // and retuns 'n-1'th element starting from 'begin'
  // on fail, puts failed element into the 'begin' and returns nullptr
  static slot* try_pop_n(slot*& begin, const size_t slot_size, size_t count) noexcept {
    auto* it = begin->next;
    while (it && --count) {
      if ((reinterpret_cast<char*>(it) + slot_size) != reinterpret_cast<char*>(it->next)) {
        begin = it->next;
        return nullptr;
      }
      it = it->next;
    }
    return it;
  }

  slot* head_{};
}; // freelist

///////////////////////////////////////////////////////////////////////////////
/// @brief BlockAllocator concept
///
/// struct BlockAllocator {
///   typedef ... size_type;
///   typedef ... difference_type;
///
///   char* malloc(size_type size_in_bytes) noexcept;
///   void free(const char* ptr, size_t size) noexcept:
/// };
///
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// @class malloc_free_allocator
/// @brief block allocator utilizing malloc/free for memory allocations
///////////////////////////////////////////////////////////////////////////////
struct malloc_free_allocator {
  typedef size_t size_type;
  typedef std::ptrdiff_t difference_type;

  char* allocate(size_type size) noexcept {
    return static_cast<char*>(std::malloc(size));
  }

  void deallocate(char* const ptr, size_t /*size*/) noexcept {
    std::free(ptr);
  }
}; // malloc_free_allocator

///////////////////////////////////////////////////////////////////////////////
/// @class new_delete_allocator
/// @brief block allocator utilizing new[]/delete[] for memory allocations
///////////////////////////////////////////////////////////////////////////////
struct new_delete_allocator {
  typedef size_t size_type;
  typedef std::ptrdiff_t difference_type;

  char* allocate(size_type size) noexcept {
    return new (std::nothrow) char[size];
  }

  void deallocate(const char* ptr, size_t /*size*/) noexcept {
    delete[] ptr;
  }
}; // new_delete_allocator

///////////////////////////////////////////////////////////////////////////////
/// @brief GrowPolicy concept
///
/// struct GrowPolicy {
///   size_t operator()(size_t) const;
/// };
///
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// @class log2_grow
/// @brief policy allows unbounded exponential grow (1 -> 2 -> 4 -> 8 -> ... )
///////////////////////////////////////////////////////////////////////////////
struct log2_grow {
  size_t operator()(size_t size) const {
    return size << 1;
  }
}; // log2_grow

///////////////////////////////////////////////////////////////////////////////
/// @class bounded_log2_grow
/// @brief policy allows bounded exponential grow
///        (1 -> 2 -> 4 -> 8 -> ... , but no more than MAX)
///////////////////////////////////////////////////////////////////////////////
class bounded_log2_grow {
 public:
  bounded_log2_grow(size_t max)
    : max_(max) {
  }

  size_t operator()(size_t size) const {
    return std::min(size << 1, max_);
  }

 private:
  size_t max_;
}; // bounded_log2_grow

///////////////////////////////////////////////////////////////////////////////
/// @class identity_grow
/// @brief policy just returns the provided size
///////////////////////////////////////////////////////////////////////////////
struct identity_grow {
  size_t operator()(size_t size) const {
    return size;
  }
}; // identity_grow

///////////////////////////////////////////////////////////////////////////////
/// @class pool_base
/// @brief base class for all memory pools that provides unified access to
///        BlockAllocator and GrowPolicy for all derivatives
///////////////////////////////////////////////////////////////////////////////
template<typename GrowPolicy, typename BlockAllocator>
class pool_base : private compact_ref<0, BlockAllocator>,
                  private compact<0, GrowPolicy>,
                  private util::noncopyable {
 public:
  typedef GrowPolicy grow_policy_t;
  typedef compact<0, GrowPolicy> grow_policy_store_t;

  typedef BlockAllocator block_allocator_t;
  typedef compact_ref<0, BlockAllocator> block_allocator_store_t;

  pool_base(const grow_policy_t& policy, const block_allocator_t& allocator)
    : block_allocator_store_t(allocator),
      grow_policy_store_t(policy) {
  }

  pool_base(pool_base&& rhs)
    : block_allocator_store_t(std::move(rhs)),
      grow_policy_store_t(std::move(rhs)) {
  }

  pool_base& operator=(pool_base&& rhs) {
    if (this != &rhs) {
      block_allocator_store_t::operator=(std::move(rhs));
      grow_policy_store_t::operator =(std::move(rhs));
    }
    return *this;
  }

  const grow_policy_t& grow_policy() const {
    return grow_policy_store_t::get();
  }

  grow_policy_t& grow_policy() {
    return grow_policy_store_t::get();
  }

  const block_allocator_t& allocator() const {
    return block_allocator_store_t::get();
  }

  block_allocator_t& allocator() {
    return block_allocator_store_t::get();
  }
}; // pool_base

///////////////////////////////////////////////////////////////////////////////
/// @class memory_pool
/// @brief a fast free-list based memory allocator, gurantees proper alignment
///        of all allocated slots
///////////////////////////////////////////////////////////////////////////////
template<
  typename GrowPolicy = log2_grow,
  typename BlockAllocator = malloc_free_allocator
> class memory_pool : public pool_base<GrowPolicy, BlockAllocator> {
 public:
  typedef pool_base<GrowPolicy, BlockAllocator> pool_base_t;
  typedef typename pool_base_t::grow_policy_t grow_policy_t;
  typedef typename pool_base_t::block_allocator_t block_allocator_t;
  typedef typename block_allocator_t::size_type size_type;

  explicit memory_pool(
      const size_t slot_min_size = 0,
      size_t initial_size = 32,
      const block_allocator_t& alloc = block_allocator_t(),
      const grow_policy_t& grow_policy = grow_policy_t()) noexcept
    : pool_base_t(grow_policy, alloc),
      slot_size_(adjust_slot_size(slot_min_size)),
      next_size_(adjust_initial_size(initial_size)) {
  }

  memory_pool(memory_pool&& rhs) noexcept
    : pool_base_t(std::move(rhs)),
      slot_size_(rhs.slot_size_),
      next_size_(rhs.next_size_),
      capacity_(rhs.capacity_),
      free_(std::move(rhs.free_)),
      blocks_(std::move(rhs.blocks_)) {
    rhs.next_size_ = 0;
    rhs.capacity_ = 0;
  }

  memory_pool& operator=(memory_pool&& rhs) noexcept {
    if (this != &rhs) {
      pool_base_t::operator=(std::move(rhs));
      slot_size_ = rhs.slot_size_;
      next_size_ = rhs.next_size_;
      capacity_ = rhs.capacity_;
      rhs.next_size_ = 0;
      rhs.capacity_ = 0;
      free_ = std::move(rhs.free_);
      blocks_ = std::move(rhs.blocks_);
    }
    return *this;
  }

  ~memory_pool() noexcept {
    // deallocate previously allocated blocks
    // in reverse order
    while (!blocks_.empty()) {
      auto* block = blocks_.pop();

      this->allocator().deallocate(
        reinterpret_cast<char*>(block), // begin of the allocated block
        *reinterpret_cast<size_t*>(block) // size of the block
      );
    }
  }

  void* allocate(size_t n) {
    auto* p = free_.pop_n(slot_size_, n);

    if (p) {
      // have enough items in a free list
      return p;
    }

    if (n <= next_size_) { // have enough items in the next block
      const auto block_size = next_size_;

      p = allocate_block(block_size);

      // add rest of the allocated block to the free list
      free_.push_n(
        reinterpret_cast<char*>(p) + n*slot_size_,
        slot_size_,
        block_size - n
      );

      return p;
    }

    // too much items requested, just allocate a block of the requested size
    return allocate_block(n);
  }

  void* allocate() {
    if (free_.empty()) {
      allocate_block();
    }

    return free_.pop();
  }

  void deallocate(void* p, size_t n) noexcept {
    if (!p) return;

    free_.push_n(p, slot_size_, n);
  }

  void deallocate(void* p) noexcept {
    free_.push(p);
  }

  size_t capacity() const noexcept {
    return capacity_;
  }

  size_t slot_size() const noexcept {
    return slot_size_;
  }

  size_t next_size() const noexcept {
    return next_size_;
  }

  bool empty() const noexcept {
    return blocks_.empty();
  }

 private:
  static size_t adjust_initial_size(size_t next_size) noexcept {
    return (std::max)(next_size, size_t(2)); // block chain + 1 slot
  }

  static size_t adjust_slot_size(size_t slot_size) noexcept {
    using namespace iresearch::math;
    static_assert(is_power2(freelist::MIN_ALIGN), "MIN_ALIGN must be a power of 2");

    slot_size = align_up(
      (std::max)(slot_size, size_t(freelist::MIN_SIZE)),
      freelist::MIN_ALIGN
    );

    assert(slot_size >= freelist::MIN_SIZE);
    assert(!(slot_size % freelist::MIN_ALIGN));

    return slot_size;
  }

  void* allocate_block(size_t block_size) {
    assert(block_size && slot_size_);

    // allocate memory block
    const auto size_in_bytes = block_size*slot_size_ + sizeof(size_t) + freelist::MIN_SIZE;
    char* begin = this->allocator().allocate(size_in_bytes);

    if (!begin) {
      throw std::bad_alloc();
    }

    *reinterpret_cast<size_t*>(begin) = size_in_bytes; // remember size of the block

    // noexcept
    capacity_ += block_size;
    next_size_ = this->grow_policy()(next_size_);

    // use 1st slot in a block to store pointer
    // to the prevoiusly allocated block
    blocks_.push(begin);

    return begin + sizeof(size_t) + freelist::MIN_SIZE;
  }

  void allocate_block() {
    assert(free_.empty());

    const auto block_size = next_size_;

    auto* begin = allocate_block(block_size);

    // initialize list of free slots in allocated block
    free_.assign(begin, slot_size_, block_size);
  }

  // this method is using by memory_pool_allocator
  // to rebind it from one type to another
  void rebind(size_t slot_size) {
    // can rebind empty allocator only
    assert(slot_size_ == slot_size || this->empty());
    slot_size_ = slot_size;
  }

  size_t slot_size_;
  size_t capacity_{}; // number of allocated slots
  size_t next_size_;
  freelist free_; // list of free slots in the allocated blocks
  freelist blocks_; // list of the allocated blocks

  template<typename, typename, typename>
  friend class memory_pool_allocator;
}; // memory_pool

///////////////////////////////////////////////////////////////////////////////
/// @class template<typename T> allocator_base
/// @brief a base class for all std-compliant allocators
///////////////////////////////////////////////////////////////////////////////
template<typename T>
struct allocator_base {
  typedef T               value_type;
  typedef T*              pointer;
  typedef T&              reference;
  typedef const T*        const_pointer;
  typedef const T&        const_reference;

  pointer address(reference x) const noexcept {
    return std::addressof(x);
  }

  const_pointer address(const_reference x) const noexcept {
    return std::addressof(x);
  }

  template<typename... Args>
  void construct(pointer p, Args&&... args) {
    new (p) value_type(std::forward<Args>(args)...); // call ctor
  }

  void destroy(pointer p) noexcept {
    p->~value_type(); // call dtor
  }
}; // allocator_base

struct single_allocator_tag { };
struct bulk_allocator_tag { };

///////////////////////////////////////////////////////////////////////////////
/// @class template<typename T> memory_pool_allocator
/// @brief a std-compliant allocator based of memory_pool
/// @note single_allocator_tag is used for optimizations for node-based
///        memory allocations (e.g. std::map, std::set, std::list)
///////////////////////////////////////////////////////////////////////////////
template<
  typename T,
  typename MemoryPool,
  typename Tag = single_allocator_tag
> class memory_pool_allocator : public allocator_base<T> {
 public:
  typedef MemoryPool memory_pool_t;
  typedef typename MemoryPool::size_type size_type;

  typedef allocator_base<T> allocator_base_t;
  typedef typename allocator_base_t::pointer pointer;
  typedef typename allocator_base_t::const_pointer const_pointer;

  typedef std::false_type propagate_on_container_copy_assignment;
  typedef std::true_type  propagate_on_container_move_assignment;
  typedef std::true_type  propagate_on_container_swap;

  template <typename U> struct rebind {
    typedef memory_pool_allocator<U, MemoryPool, Tag> other;
  };

  template<typename U>
  memory_pool_allocator(const memory_pool_allocator<U, memory_pool_t, Tag>& rhs) noexcept
    : pool_(rhs.pool_) {
    pool_->rebind(sizeof(T));
  }

  explicit memory_pool_allocator(memory_pool_t& pool) noexcept
    : pool_(&pool) {
    pool_->rebind(sizeof(T));
  }

  pointer allocate(size_type n, const_pointer hint = 0) {
    UNUSED(hint);

    if (std::is_same<Tag, single_allocator_tag>::value) {
      assert(1 == n);
      return static_cast<pointer>(pool_->allocate());
    }

    if (1 == n) {
      return static_cast<pointer>(pool_->allocate());
    }

    return static_cast<pointer>(pool_->allocate(n));
  }

  void deallocate(pointer p, size_type n = 1) noexcept {
    if (std::is_same<Tag, single_allocator_tag>::value) {
      assert(1 == n);
      pool_->deallocate(p);
      return;
    }

    if (1 == n) {
      pool_->deallocate(p);
    } else {
      pool_->deallocate(p, n);
    }
  }

 private:
  memory_pool_t* pool_;

  template<typename U, typename, typename>
  friend class memory_pool_allocator;
}; // memory_pool_allocator

///////////////////////////////////////////////////////////////////////////////
/// @class memory_multi_size_pool
/// @brief an allocator that can handle allocations of slots with different
///        sizes
///////////////////////////////////////////////////////////////////////////////
template<
  typename GrowPolicy = log2_grow,
  typename BlockAllocator = malloc_free_allocator
> class memory_multi_size_pool : public pool_base<GrowPolicy, BlockAllocator> {
 public:
  typedef pool_base<GrowPolicy, BlockAllocator> pool_base_t;
  typedef typename pool_base_t::grow_policy_t grow_policy_t;
  typedef typename pool_base_t::block_allocator_t block_allocator_t;
  typedef memory_pool<grow_policy_t, block_allocator_t> memory_pool_t;

  explicit memory_multi_size_pool(
      size_t initial_size = 32,
      const block_allocator_t& block_alloc = block_allocator_t(),
      const grow_policy_t& grow_policy = grow_policy_t()) noexcept
    : pool_base_t(grow_policy, block_alloc),
      initial_size_(initial_size) {
  }

  void* allocate(const size_t slot_size) {
    return this->pool(slot_size).allocate();
  }

  void* allocate(const size_t slot_size, size_t n) {
    return this->pool(slot_size).allocate(n);
  }

  void deallocate(const size_t slot_size, void* p) {
    this->pool(slot_size).deallocate(p);
  }

  void deallocate(const size_t slot_size, void* p, const size_t n) {
    this->pool(slot_size).deallocate(p, n);
  }

  memory_pool_t& pool(const size_t size) const {
    const auto res = pools_.try_emplace(
      size, size, initial_size_,
      this->allocator(), this->grow_policy());

    return res.first->second;
  }

 private:
  mutable std::map<size_t, memory_pool_t> pools_;
  const size_t initial_size_; // initial size for all sub-allocators
}; // memory_multi_size_pool

///////////////////////////////////////////////////////////////////////////////
/// @class template<typename T> memory_pool_multi_size_allocator
/// @brief a std-compliant allocator based of memory_multi_size_pool
/// @note single_allocator_tag is used for optimizations for node-based
///        memory allocations (e.g. std::map, std::set, std::list)
///////////////////////////////////////////////////////////////////////////////
template<
  typename T,
  typename AllocatorsPool,
  typename Tag = single_allocator_tag
> class memory_pool_multi_size_allocator : public allocator_base<T> {
 public:
  typedef AllocatorsPool allocators_t;
  typedef typename allocators_t::memory_pool_t memory_pool_t;

  typedef typename memory_pool_t::size_type size_type;
  typedef allocator_base<T> allocator_base_t;
  typedef typename allocator_base_t::pointer pointer;
  typedef typename allocator_base_t::const_pointer const_pointer;

  typedef std::true_type  propagate_on_container_copy_assignment;
  typedef std::true_type  propagate_on_container_move_assignment;
  typedef std::true_type  propagate_on_container_swap;

  template <typename U> struct rebind {
    typedef memory_pool_multi_size_allocator <U, AllocatorsPool, Tag> other;
  };

  template<typename U>
  memory_pool_multi_size_allocator(const memory_pool_multi_size_allocator<U, AllocatorsPool, Tag>& rhs) noexcept
    : allocators_(rhs.allocators_), pool_(&allocators_->pool(sizeof(T))) {
  }

  memory_pool_multi_size_allocator(allocators_t& pool) noexcept
    : allocators_(&pool), pool_(&allocators_->pool(sizeof(T))) {
  }

  pointer allocate(size_type n, const_pointer hint = 0) {
    UNUSED(hint);

    if (std::is_same<Tag, single_allocator_tag>::value) {
      assert(1 == n);
      return static_cast<pointer>(pool_->allocate());
    }

    if (1 == n) {
      return static_cast<pointer>(pool_->allocate());
    }

    return static_cast<pointer>(pool_->allocate(n));
  }

  void deallocate(pointer p, size_type n = 1) noexcept {
    if (std::is_same<Tag, single_allocator_tag>::value) {
      assert(1 == n);
      pool_->deallocate(p);
      return;
    }

    if (1 == n) {
      pool_->deallocate(p);
    } else {
      pool_->deallocate(p, n);
    }
  }

 private:
  allocators_t* allocators_;
  memory_pool_t* pool_;

  template<typename U, typename, typename>
  friend class memory_pool_multi_size_allocator;
}; // memory_pool_multi_size_allocator

template<
  typename T,
  typename U,
  typename AllocatorsPool,
  typename Tag
> constexpr inline bool operator==(
    const memory_pool_multi_size_allocator<T, AllocatorsPool, Tag>& lhs,
    const memory_pool_multi_size_allocator<U, AllocatorsPool, Tag>& rhs) {
  return lhs.allocators_ == rhs.allocators_ && sizeof(T) == sizeof(U);
}

template<
  typename T,
  typename U,
  typename AllocatorsPool,
  typename Tag
> constexpr inline bool operator!=(
    const memory_pool_multi_size_allocator<T, AllocatorsPool, Tag>& lhs,
    const memory_pool_multi_size_allocator<U, AllocatorsPool, Tag>& rhs) {
  return !(lhs == rhs);
}

} // memory
} // ROOT

#endif // IRESEARCH_MEMORY_POOL_H
