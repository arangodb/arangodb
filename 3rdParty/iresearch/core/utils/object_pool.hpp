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

#ifndef IRESEARCH_OBJECT_POOL_H
#define IRESEARCH_OBJECT_POOL_H

#include <algorithm>
#include <atomic>
#include <functional>
#include <vector>

#include "memory.hpp"
#include "shared.hpp"
#include "thread_utils.hpp"
#include "async_utils.hpp"
#include "misc.hpp"
#include "noncopyable.hpp"

namespace iresearch {

template<typename T>
class atomic_shared_ptr_helper {
 public:
  static std::shared_ptr<T> atomic_exchange(std::shared_ptr<T>* p, std::shared_ptr<T> r) {
    return std::atomic_exchange(p, r);
  }

  static void atomic_store(std::shared_ptr<T>* p, std::shared_ptr<T> r) {
    std::atomic_store(p, r);
  }

  static std::shared_ptr<T> atomic_load(const std::shared_ptr<T>* p) {
    return std::atomic_load(p);
  }
}; // atomic_shared_ptr_helper

//////////////////////////////////////////////////////////////////////////////
/// @class concurrent_stack
/// @brief lock-free stack
/// @note move construction/assignment is not thread-safe
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class concurrent_stack : private util::noncopyable {
 public:
  using element_type = T;

  struct node_type {
    element_type value{};
    std::atomic<node_type*> next{}; // next needs to be atomic because
                                    // nodes are kept in a free-list and reused!
  };

  explicit concurrent_stack(node_type* head = nullptr) noexcept
    : head_{concurrent_node{head}} {
  }

  concurrent_stack(concurrent_stack&& rhs) noexcept {
    move_unsynchronized(std::move(rhs));
  }

  concurrent_stack& operator=(concurrent_stack&& rhs) noexcept {
    if (this != &rhs) {
      move_unsynchronized(std::move(rhs));
    }
    return *this;
  }

  bool empty() const noexcept {
    return nullptr == head_.load(std::memory_order_acquire).node;
  }

  node_type* pop() noexcept {
    concurrent_node head = head_.load(std::memory_order_acquire);
    concurrent_node new_head;

    do {
      if (!head.node) {
        return nullptr;
      }

      new_head.node = head.node->next.load(std::memory_order_relaxed);
      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(head, new_head,
                                          std::memory_order_acquire));

    return head.node;
  }

  void push(node_type& new_node) noexcept {
    concurrent_node head = head_.load(std::memory_order_relaxed);
    concurrent_node new_head{&new_node};

    do {
      new_node.next.store(head.node, std::memory_order_relaxed);

      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(head, new_head,
                                          std::memory_order_release));
  }

 private:
  // CMPXCHG16B requires that the destination
  // (memory) operand be 16-byte aligned
  struct alignas(IRESEARCH_CMPXCHG16B_ALIGNMENT) concurrent_node {
    explicit concurrent_node(node_type* node = nullptr) noexcept
      : version{0}, node{node} {
    }

    uintptr_t version; // avoid aba problem
    node_type* node;
  }; // concurrent_node

  static_assert(
    IRESEARCH_CMPXCHG16B_ALIGNMENT == alignof(concurrent_node),
    "invalid alignment");

  void move_unsynchronized(concurrent_stack&& rhs) noexcept {
    head_ = rhs.head_.load(std::memory_order_relaxed);
    rhs.head_.store(concurrent_node{}, std::memory_order_relaxed);
  }

  std::atomic<concurrent_node> head_;
}; // concurrent_stack

//////////////////////////////////////////////////////////////////////////////
/// @class async_value
/// @brief convenient class storing value and associated read-write lock
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class async_value {
 public:
  using value_type = T;

  template<typename... Args>
  explicit async_value(Args&&... args)
    : value_{std::forward<Args>(args)...} {
  }

  const value_type& value() const noexcept {
    return value_;
  }

  value_type& value() noexcept {
    return value_;
  }

  auto lock_read() {
    return make_shared_lock(lock_);
  }

  auto lock_write() {
    return make_unique_lock(lock_);
  }

 protected:
  value_type value_;
  std::shared_mutex lock_;
}; // async_value

//////////////////////////////////////////////////////////////////////////////
/// @brief a fixed size pool of objects
///        if the pool is empty then a new object is created via make(...)
///        if an object is available in a pool then in is returned but tracked
///        by the pool
///        when the object is released it is placed back into the pool
/// @note object 'ptr' that evaluate to false after make(...) are not considered
///       part of the pool
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class bounded_object_pool {
 public:
  using element_type = typename T::ptr::element_type;

 private:
  struct slot : util::noncopyable {
    bounded_object_pool* owner;
    typename T::ptr ptr;
    std::atomic<element_type*> value{};
  }; // slot

  using stack = concurrent_stack<slot> ;
  using node_type = typename stack::node_type;

 public:
  /////////////////////////////////////////////////////////////////////////////
  /// @class ptr
  /// @brief represents a control object of bounded_object_pool with
  ///        semantic similar to smart pointers
  /////////////////////////////////////////////////////////////////////////////
  class ptr : util::noncopyable {
   private:
    static node_type EMPTY_SLOT;

   public:
    ptr() noexcept
      : slot_{&EMPTY_SLOT},
        ptr_{nullptr} {
    }

    explicit ptr(node_type& slot, element_type& ptr) noexcept
      : slot_{&slot},
        ptr_{&ptr}  {
    }

    ptr(ptr&& rhs) noexcept
      : slot_{rhs.slot_},
        ptr_{rhs.ptr_} {
      rhs.slot_ = &EMPTY_SLOT; // take ownership
      rhs.ptr_ = nullptr;
    }

    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        slot_ = rhs.slot_;
        ptr_ = rhs.ptr_;
        rhs.slot_ = &EMPTY_SLOT; // take ownership
        rhs.ptr_ = nullptr;
      }
      return *this;
    }

    ~ptr() noexcept {
      reset();
    }

    FORCE_INLINE void reset() noexcept {
      reset_impl(slot_);
      ptr_ = nullptr;
    }

    std::shared_ptr<element_type> release() {
      auto* slot = slot_;
      auto* ptr = ptr_;
      slot_ = &EMPTY_SLOT; // moved
      ptr_ = nullptr;

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        ptr,
        [slot] (element_type*) mutable noexcept {
          reset_impl(slot);
      });
    }

    operator bool() const noexcept { return &EMPTY_SLOT != slot_; }
    element_type& operator*() const noexcept { return *slot_->value.ptr; }
    element_type* operator->() const noexcept { return get(); }
    element_type* get() const noexcept { return ptr_; }

    friend bool operator==(const ptr& lhs, std::nullptr_t) noexcept {
      return !static_cast<bool>(lhs);
    }
    friend bool operator!=(const ptr& lhs, std::nullptr_t rhs) noexcept {
      return !(lhs == rhs);
    }
    friend bool operator==(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return (rhs == lhs);
    }
    friend bool operator!=(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return !(rhs == lhs);
    }

   private:
    static void reset_impl(node_type*& slot) noexcept {
      if (slot == &EMPTY_SLOT) {
        // nothing to do
        return;
      }

      assert(slot->value.owner);
      slot->value.owner->unlock(*slot);
      slot = &EMPTY_SLOT; // release ownership
    }

    node_type* slot_;
    element_type* ptr_;
  }; // ptr

  explicit bounded_object_pool(size_t size)
    : pool_(std::max(size_t(1), size)) {
    // initialize pool
    for (auto& node : pool_) {
      auto& slot = node.value;
      slot.owner = this;

      free_list_.push(node);
    }
  }

  template<typename... Args>
  ptr emplace(Args&&... args) {
    node_type* head = nullptr;

    while (!(head = free_list_.pop())) {
      wait_for_free_slots();
    }

    auto& slot = head->value;

    auto* p = slot.value.load(std::memory_order_acquire);

    if (!p) {
      auto& value = slot.ptr;

      try {
        value = T::make(std::forward<Args>(args)...);
      } catch (...) {
        free_list_.push(*head);
        cond_.notify_all();
        throw;
      }

      p = value.get();

      if (p) {
        slot.value.store(p, std::memory_order::memory_order_release);
        return ptr(*head, *p);
      }

      free_list_.push(*head); // put empty slot back into the free list
      cond_.notify_all();

      return ptr();
    }

    return ptr(*head, *p);
  }

  size_t size() const noexcept { return pool_.size(); }

  template<typename Visitor>
  bool visit(const Visitor& visitor) {
    return const_cast<const bounded_object_pool&>(*this).visit(visitor);
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    stack list;

    auto release_all = make_finally([this, &list]()noexcept{
      while (auto* head = list.pop()) {
        free_list_.push(*head);
      }
    });

    auto size = pool_.size();

    while (size) {
      node_type* head = nullptr;

      while (!(head = free_list_.pop())) {
        wait_for_free_slots();
      }

      list.push(*head);

      auto& obj = head->value.ptr;

      if (obj && !visitor(*obj)) {
        return false;
      }

      --size;
    }

    return true;
  }

 private:
  void wait_for_free_slots() const {
    using namespace std::chrono_literals;

    auto lock = make_unique_lock(mutex_);

    if (free_list_.empty()) {
      cond_.wait_for(lock, 100ms);
    }
  }

  void unlock(node_type& slot) const {
    free_list_.push(slot);
    cond_.notify_all();
  }

  mutable std::condition_variable cond_;
  mutable std::mutex mutex_;
  mutable std::vector<node_type> pool_;
  mutable stack free_list_;
}; // bounded_object_pool

template<typename T>
typename bounded_object_pool<T>::node_type bounded_object_pool<T>::ptr::EMPTY_SLOT;

//////////////////////////////////////////////////////////////////////////////
/// @class unbounded_object_pool_base
/// @brief base class for all unbounded object pool implementations
//////////////////////////////////////////////////////////////////////////////
template<
  typename T,
  typename = std::enable_if_t<
    is_unique_ptr_v<typename T::ptr> &&
    std::is_empty_v<typename T::ptr::deleter_type>>>
class unbounded_object_pool_base : private util::noncopyable {
 public:
  using deleter_type = typename T::ptr::deleter_type;
  using element_type = typename T::ptr::element_type;
  using pointer = typename T::ptr::pointer;

 private:
  struct slot : util::noncopyable {
    std::atomic<pointer> value{};
  }; // slot

 public:
  size_t size() const noexcept { return pool_.size(); }

 protected:
  using stack = concurrent_stack<slot>;
  using node = typename stack::node_type;

  explicit unbounded_object_pool_base(size_t size)
    : pool_(size),
      free_slots_{pool_.data()} {
    // build up linked list
    for (auto begin = pool_.begin(), end = pool_.end(), next = begin < end ? (begin + 1) : end;
         next < end;
         begin = next, ++next) {
      begin->next = &*next;
    }
  }

  template<typename... Args>
  pointer acquire(Args&&... args) {
    auto* head = free_objects_.pop();

    if (head) {
      pointer value = head->value.value.exchange(nullptr, std::memory_order_relaxed);
      assert(value);
      free_slots_.push(*head);
      return value;
    }

    auto ptr = T::make(std::forward<Args>(args)...);

    return ptr.release();
  }

  void release(pointer value) noexcept {
    if (!value) {
      // do not hold nullptr values in the pool since
      // emplace(...) uses nullptr to denote creation failure
      return;
    }

    auto* slot = free_slots_.pop();

    if (!slot) {
      // no free slots
      deleter_type{}(value);
      return;
    }

    assert(value);
    [[maybe_unused]] const pointer old_value = slot->value.value.exchange(
      value, std::memory_order_relaxed);
    assert(!old_value);
    free_objects_.push(*slot);
  }

  unbounded_object_pool_base(unbounded_object_pool_base&& rhs) noexcept
    : pool_{std::move(rhs.pool_)} {
    // need for volatile pool only
  }
  unbounded_object_pool_base& operator=(unbounded_object_pool_base&&) = delete;

  std::vector<node> pool_;
  stack free_objects_; // list of created objects that are ready to be reused
  stack free_slots_; // list of free slots to be reused
}; // unbounded_object_pool_base

//////////////////////////////////////////////////////////////////////////////
/// @class unbounded_object_pool
/// @brief a fixed size pool of objects
///        if the pool is empty then a new object is created via make(...)
///        if an object is available in a pool then in is returned and no
///        longer tracked by the pool
///        when the object is released it is placed back into the pool if
///        space in the pool is available
///        pool owns produced object so it's not allowed to destroy before
///        all acquired objects will be destroyed
/// @note object 'ptr' that evaluate to false when returnd back into the pool
///       will be discarded instead
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class unbounded_object_pool : public unbounded_object_pool_base<T> {
 private:
  using base_t = unbounded_object_pool_base<T>;
  using node = typename base_t::node;

 public:
  using element_type = typename base_t::element_type;
  using pointer = typename base_t::pointer;

  /////////////////////////////////////////////////////////////////////////////
  /// @class ptr
  /// @brief represents a control object of unbounded_object_pool with
  ///        semantic similar to smart pointers
  /////////////////////////////////////////////////////////////////////////////
  class ptr : util::noncopyable {
   public:
    ptr() noexcept : value_{nullptr} {}
    ptr(pointer value,
        unbounded_object_pool& owner)
      : value_{value},
        owner_{&owner} {
    }

    ptr(ptr&& rhs) noexcept
      : value_{rhs.value_},
        owner_{rhs.owner_} {
      rhs.value_ = nullptr;
    }

    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        reset();
        value_ = rhs.value_;
        rhs.value_ = nullptr;
        owner_ = rhs.owner_;
      }
      return *this;
    }
   
    ~ptr() noexcept {
      reset();
    }

    FORCE_INLINE void reset() noexcept {
      if (value_) {
        owner_->release(value_);
        value_ = nullptr;
      }
    }

    std::shared_ptr<element_type> release() {
      auto* raw = value_;
      value_ = nullptr; // moved

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        raw,
        [owner = owner_] (pointer p) mutable noexcept {
          owner->release(p);
      });
    }

    element_type& operator*() const noexcept { return *value_; }
    pointer operator->() const noexcept { return get(); }
    pointer get() const noexcept { return value_; }
    operator bool() const noexcept {
      return static_cast<bool>(value_);
    }

    friend bool operator==(const ptr& lhs, std::nullptr_t) noexcept {
      return !static_cast<bool>(lhs);
    }
    friend bool operator!=(const ptr& lhs, std::nullptr_t rhs) noexcept {
      return !(lhs == rhs);
    }
    friend bool operator==(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return (rhs == lhs);
    }
    friend bool operator!=(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return !(rhs == lhs);
    }

   private:
    pointer value_;
    unbounded_object_pool* owner_;
  };

  explicit unbounded_object_pool(size_t size = 0)
    : base_t{size} {
  }

  ~unbounded_object_pool() {
    for (auto& slot : this->pool_) {
      auto p = slot.value.value.load(std::memory_order_relaxed);
      typename base_t::deleter_type{}(p);
    }
  }

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

  /////////////////////////////////////////////////////////////////////////////
  /// @brief clears all cached objects
  /////////////////////////////////////////////////////////////////////////////
  void clear() {
    node* head = nullptr;

    // reset all cached instances
    while (head = this->free_objects_.pop()) {
      auto p = head->value.value.exchange(nullptr, std::memory_order_relaxed);
      assert(p);
      typename base_t::deleter_type{}(p);
      this->free_slots_.push(*head);
    }
  }

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

  template<typename... Args>
  ptr emplace(Args&&... args) {
    return ptr(this->acquire(std::forward<Args>(args)...), *this);
  }

 private:
  // disallow move
  unbounded_object_pool(unbounded_object_pool&&) = delete;
  unbounded_object_pool& operator=(unbounded_object_pool&&) = delete;
}; // unbounded_object_pool

//////////////////////////////////////////////////////////////////////////////
/// @class unbounded_object_pool_volatile
/// @brief a fixed size pool of objects
///        if the pool is empty then a new object is created via make(...)
///        if an object is available in a pool then in is returned and no
///        longer tracked by the pool
///        when the object is released it is placed back into the pool if
///        space in the pool is available
///        pool may be safely destroyed even there are some produced objects
///        alive
/// @note object 'ptr' that evaluate to false when returnd back into the pool
///       will be discarded instead
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class unbounded_object_pool_volatile : public unbounded_object_pool_base<T> {
 private:
  struct generation {
    explicit generation(unbounded_object_pool_volatile * owner) noexcept
      : owner{owner} {
    }

    unbounded_object_pool_volatile* owner; // current owner (null == stale generation)
  }; // generation

  using base_t = unbounded_object_pool_base<T>;
  using generation_t = async_value<generation>;
  using generation_ptr_t = std::shared_ptr<generation_t>;
  using atomic_utils = atomic_shared_ptr_helper<generation_t>;
  using deleter_type = typename base_t::deleter_type;

 public:
  using element_type = typename base_t::element_type;
  using pointer = typename base_t::pointer;

  /////////////////////////////////////////////////////////////////////////////
  /// @class ptr
  /// @brief represents a control object of unbounded_object_pool with
  ///        semantic similar to smart pointers
  /////////////////////////////////////////////////////////////////////////////
  class ptr : private util::noncopyable {
   public:
    ptr(pointer value,
        generation_ptr_t gen) noexcept
      : value_{value},
        gen_{std::move(gen)} {
    }

    ptr(ptr&& rhs) noexcept
      : value_{rhs.value_},
        gen_{std::move(rhs.gen_)} {
      rhs.value_ = nullptr;
    }
    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        value_ = rhs.value_;
        rhs.value_ = nullptr;
        gen_ = std::move(rhs.gen_);
      }
      return *this;
    }

    ~ptr() noexcept {
      reset();
    }

    void reset() noexcept {
      if (!gen_) {
        // already destroyed
        return;
      }

      reset_impl(value_, *gen_);
      value_ = nullptr;
      gen_ = nullptr; // mark as destroyed
    }

    std::shared_ptr<element_type> release() {
      auto raw = value_;
      auto moved_gen = make_move_on_copy(std::move(gen_));
      assert(!gen_); // moved

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        raw,
        [moved_gen] (pointer p) noexcept {
          if (p) {
            reset_impl(p, *moved_gen.value());
          }
      });
    }

    element_type& operator*() const noexcept { return *value_; }
    pointer operator->() const noexcept { return get(); }
    pointer get() const noexcept { return value_; }
    operator bool() const noexcept {
      return static_cast<bool>(gen_);
    }

    friend bool operator==(const ptr& lhs, std::nullptr_t) noexcept {
      return !static_cast<bool>(lhs);
    }
    friend bool operator!=(const ptr& lhs, std::nullptr_t rhs) noexcept {
      return !(lhs == rhs);
    }
    friend bool operator==(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return (rhs == lhs);
    }
    friend bool operator!=(std::nullptr_t lhs, const ptr& rhs) noexcept {
      return !(rhs == lhs);
    }

   private:
    static void reset_impl(pointer obj, generation_t& gen) noexcept {
      assert(obj);

      // do not remove scope!!!
      // variable 'lock' below must be destroyed before 'gen_'
      {
        auto lock = gen.lock_read();
        auto& value = gen.value();

        if (auto* owner = value.owner; owner) {
          owner->release(obj);
          return;
        }
      }

      // clear object oustide the lock if necessary
      deleter_type{}(obj);
    }

    pointer value_;
    generation_ptr_t gen_;
  }; // ptr

  explicit unbounded_object_pool_volatile(size_t size = 0)
    : base_t{size},
      gen_{memory::make_shared<generation_t>(this)} {
  }

  // FIXME check what if
  //
  // unbounded_object_pool_volatile p0, p1;
  // thread0: p0.clear();
  // thread1: unbounded_object_pool_volatile p1(std::move(p0));
  unbounded_object_pool_volatile(unbounded_object_pool_volatile&& rhs) noexcept
    : base_t{std::move(rhs)} {
    gen_ = atomic_utils::atomic_load(&rhs.gen_);

    auto lock = gen_->lock_write();
    gen_->value().owner = this; // change owner

    this->free_slots_ = std::move(rhs.free_slots_);
    this->free_objects_ = std::move(rhs.free_objects_);
  }

  ~unbounded_object_pool_volatile() noexcept {
    // prevent existing elements from returning into the pool
    // if pool doesn't own generation anymore
    const auto gen = atomic_utils::atomic_load(&gen_);
    auto lock = gen->lock_write();

    auto& value = gen->value();

    if (value.owner == this) {
      value.owner = nullptr;
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  /// @brief clears all cached objects and optionally prevents already created
  ///        objects from returning into the pool
  /// @param new_generation if true, prevents already created objects from
  ///        returning into the pool, otherwise just clears all cached objects
  /////////////////////////////////////////////////////////////////////////////
  void clear(bool new_generation = false) {
    // prevent existing elements from returning into the pool
    if (new_generation) {
      {
        auto gen = atomic_utils::atomic_load(&gen_);
        auto lock = gen->lock_write();
        gen->value().owner = nullptr;
      }

      // mark new generation
      atomic_utils::atomic_store(&gen_, memory::make_shared<generation_t>(this));
    }

    typename base_t::node* head = nullptr;

    // reset all cached instances
    while ((head = this->free_objects_.pop())) {
      auto p = head->value.value.exchange(nullptr, std::memory_order_relaxed);
      assert(p);
      deleter_type{}(p);
      this->free_slots_.push(*head);
    }
  }

  template<typename... Args>
  ptr emplace(Args&&... args) {
    const auto gen = atomic_utils::atomic_load(&gen_); // retrieve before seek/instantiate
    auto value = this->acquire(std::forward<Args>(args)...);

    return value
      ? ptr{value, gen}
      : ptr{nullptr, generation_ptr_t{}};
  }

  size_t generation_size() const noexcept {
    const auto use_count = atomic_utils::atomic_load(&gen_).use_count();
    assert(use_count >= 2);
    return use_count - 2; // -1 for temporary object, -1 for this->_gen
  }

 private:
  // disallow move assignment
  unbounded_object_pool_volatile& operator=(unbounded_object_pool_volatile&&) = delete;

  generation_ptr_t gen_; // current generation
}; // unbounded_object_pool_volatile

}

#endif
