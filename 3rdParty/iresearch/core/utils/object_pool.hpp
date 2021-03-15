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
  #if defined(IRESEARCH_VALGRIND) // suppress valgrind false-positives related to std::atomic_*
   public:
    // for compatibility 'std::mutex' is not moveable
    atomic_shared_ptr_helper() = default;
    atomic_shared_ptr_helper(atomic_shared_ptr_helper&&) noexcept { }
    atomic_shared_ptr_helper& operator=(atomic_shared_ptr_helper&&) noexcept { return *this; }

    std::shared_ptr<T> atomic_exchange(std::shared_ptr<T>* p, std::shared_ptr<T> r) const {
      auto lock = irs::make_lock_guard(mutex_);
      return std::atomic_exchange(p, r);
    }

    void atomic_store(std::shared_ptr<T>* p, std::shared_ptr<T> r) const {
      auto lock = irs::make_lock_guard(mutex_);
      std::atomic_store(p, r);
    }

    std::shared_ptr<T> atomic_load(const std::shared_ptr<T>* p) const {
      auto lock = irs::make_lock_guard(mutex_);
      return std::atomic_load(p);
    }

   private:
    mutable std::mutex mutex_;
  #else
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
  #endif // defined(IRESEARCH_VALGRIND)
}; // atomic_shared_ptr_helper

//////////////////////////////////////////////////////////////////////////////
/// @class concurrent_stack
/// @brief lock-free single linked list
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class concurrent_stack : private util::noncopyable {
 public:
  typedef T element_type;

  struct node_type {
    element_type value{};
    node_type* next{};
  };

  explicit concurrent_stack(node_type* head = nullptr) noexcept
    : head_(head) {
  }

  concurrent_stack(concurrent_stack&& rhs) noexcept
    : head_(nullptr) {
    if (this != &rhs) {
      auto rhshead = rhs.head_.load();
      while (!rhs.head_.compare_exchange_weak(rhshead, head_));
      head_.store(rhshead);
    }
  }

  concurrent_stack& operator=(concurrent_stack&& rhs) noexcept {
    if (this != &rhs) {
      auto rhshead = rhs.head_.load();
      const concurrent_node empty;
      while (!rhs.head_.compare_exchange_weak(rhshead, empty));
      head_.store(rhshead);
    }
    return *this;
  }

  bool empty() const noexcept {
    VALGRIND_ONLY(auto lock = make_lock_guard(mutex_);) // suppress valgrind false-positives related to std::atomic_*
    return nullptr == head_.load().node;
  }

  node_type* pop() noexcept {
    VALGRIND_ONLY(auto lock = make_lock_guard(mutex_);) // suppress valgrind false-positives related to std::atomic_*
    concurrent_node head = head_.load();
    concurrent_node new_head;

    do {
      if (!head.node) {
        return nullptr;
      }

      new_head.node = head.node->next;
      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(head, new_head));

    return head.node;
  }

  void push(node_type& new_node) noexcept {
    VALGRIND_ONLY(auto lock = make_lock_guard(mutex_);) // suppress valgrind false-positives related to std::atomic_*
    concurrent_node head = head_.load();
    concurrent_node new_head;

    do {
      new_node.next = head.node;

      new_head.node = &new_node;
      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(head, new_head));
  }

 private:
  // CMPXCHG16B requires that the destination
  // (memory) operand be 16-byte aligned
  struct alignas(IRESEARCH_CMPXCHG16B_ALIGNMENT) concurrent_node {
    concurrent_node(node_type* node = nullptr) noexcept
      : version(0), node(node) {
    }

    uintptr_t version; // avoid aba problem
    node_type* node;
  }; // concurrent_node

  static_assert(
    IRESEARCH_CMPXCHG16B_ALIGNMENT == alignof(concurrent_node),
    "invalid alignment"
  );

  std::atomic<concurrent_node> head_;
  VALGRIND_ONLY(mutable std::mutex mutex_;)
}; // concurrent_stack


// -----------------------------------------------------------------------------
// --SECTION--                                                      bounded pool
// -----------------------------------------------------------------------------

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
 private:
  struct slot_t : util::noncopyable {
    bounded_object_pool* owner;
    typename T::ptr ptr;
  }; // slot_t

  typedef concurrent_stack<slot_t> stack;
  typedef typename stack::node_type node_type;

 public:
  typedef typename T::ptr::element_type element_type;

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
      : slot_(&EMPTY_SLOT) {
    }

    explicit ptr(node_type& slot) noexcept
      : slot_(&slot) {
    }

    ptr(ptr&& rhs) noexcept
      : slot_(rhs.slot_) {
      rhs.slot_ = &EMPTY_SLOT; // take ownership
    }

    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        slot_ = rhs.slot_;
        rhs.slot_ = &EMPTY_SLOT; // take ownership
      }
      return *this;
    }

    ~ptr() noexcept {
      reset();
    }

    FORCE_INLINE void reset() noexcept {
      reset_impl(slot_);
    }

    std::shared_ptr<element_type> release() {
      auto* raw = get();
      auto* slot = slot_;
      slot_ = &EMPTY_SLOT; // moved

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        raw,
        [slot] (element_type*) mutable noexcept {
          reset_impl(slot);
      });
    }

    operator bool() const noexcept { return &EMPTY_SLOT != slot_; }
    element_type& operator*() const noexcept { return *slot_->value.ptr; }
    element_type* operator->() const noexcept { return get(); }
    element_type* get() const noexcept {
      return slot_->value.ptr.get();
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
    auto& value = slot.ptr;

    if (!value) {
      try {
        value = T::make(std::forward<Args>(args)...);
      } catch (...) {
        free_list_.push(*head);
        throw;
      }
    }

    if (value) {
      return ptr(*head);
    }

    free_list_.push(*head); // put empty slot back into the free list

    return ptr();
  }

  size_t size() const noexcept { return pool_.size(); }

  template<typename Visitor>
  bool visit(const Visitor& visitor) {
    return const_cast<const bounded_object_pool&>(*this).visit(visitor);
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    stack list;

    auto release_all = make_finally([this, &list] () {
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
      cond_.wait_for(lock, 1000ms);
    }
  }

  void unlock(node_type& slot) const noexcept {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    unbounded pool
// -----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @class async_value
/// @brief convenient class storing value and associated read-write lock
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class async_value {
 public:
  typedef T value_type;
  typedef async_utils::read_write_mutex::read_mutex read_lock_type;
  typedef async_utils::read_write_mutex::write_mutex write_lock_type;

  template<typename... Args>
  explicit async_value(Args&&... args)
    : value_(std::forward<Args>(args)...) {
  }

  const value_type& value() const noexcept {
    return value_;
  }

  value_type& value() noexcept {
    return value_;
  }

  read_lock_type& read_lock() const noexcept {
    return read_lock_;
  }

  write_lock_type& write_lock() const noexcept {
    return write_lock_;
  }

 protected:
  value_type value_;
  async_utils::read_write_mutex lock_;
  mutable read_lock_type read_lock_{ lock_ };
  mutable write_lock_type write_lock_{ lock_ };
}; // async_value

//////////////////////////////////////////////////////////////////////////////
/// @class unbounded_object_pool_base
/// @brief base class for all unbounded object pool implementations
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class unbounded_object_pool_base : private util::noncopyable {
 public:
  typedef typename T::ptr::element_type element_type;

  size_t size() const noexcept { return pool_.size(); }

 protected:
  typedef concurrent_stack<typename T::ptr> stack;
  typedef typename stack::node_type node;

  explicit unbounded_object_pool_base(size_t size)
    : pool_(size), free_slots_(pool_.data()) {
    // build up linked list
    for (auto begin = pool_.begin(), end = pool_.end(), next = begin < end ? (begin + 1) : end;
         next < end;
         begin = next, ++next) {
      begin->next = &*next;
    }
  }

  unbounded_object_pool_base(unbounded_object_pool_base&& rhs) noexcept
    : pool_(std::move(rhs.pool_)) {
  }

  template<typename... Args>
  typename T::ptr acquire(Args&&... args) {
    auto* head = free_objects_.pop();

    if (head) {
      auto value = std::move(head->value);
      assert(value);
      free_slots_.push(*head);
      return value;
    }

    return T::make(std::forward<Args>(args)...);
  }

  void release(typename T::ptr&& value) noexcept {
    if (!value) {
      return; // do not hold nullptr values in the pool since emplace(...) uses nullptr to denote creation failure
    }

    auto* slot = free_slots_.pop();

    if (!slot) {
      // no free slots
      return;
    }

    assert(value);
    slot->value = std::move(value);
    free_objects_.push(*slot);
  }

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
  typedef unbounded_object_pool_base<T> base_t;
  typedef typename base_t::node node;

 public:
  typedef typename base_t::element_type element_type;

  /////////////////////////////////////////////////////////////////////////////
  /// @class ptr
  /// @brief represents a control object of unbounded_object_pool with
  ///        semantic similar to smart pointers
  /////////////////////////////////////////////////////////////////////////////
  class ptr : util::noncopyable {
   public:
    ptr(typename T::ptr&& value,
        unbounded_object_pool& owner
    ) : value_(std::move(value)),
        owner_(&owner) {
    }

    ptr(ptr&& rhs) noexcept
      : value_(std::move(rhs.value_)),
        owner_(rhs.owner_) {
      rhs.owner_ = nullptr;
    }

    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        value_ = std::move(rhs.value_);
        owner_ = rhs.owner_;
        rhs.owner_ = nullptr;
      }
      return *this;
    }

    ~ptr() noexcept {
      reset();
    }

    FORCE_INLINE void reset() noexcept {
      reset_impl(value_, owner_);
    }

    // FIXME handle potential bad_alloc in shared_ptr constructor
    // mark method as noexcept and move back all the stuff in case of error
    std::shared_ptr<element_type> release() {
      auto* raw = get();
      auto moved_value = make_move_on_copy(std::move(value_));
      auto* owner = owner_;
      owner_ = nullptr; // moved

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        raw,
        [owner, moved_value] (element_type*) mutable noexcept {
          reset_impl(moved_value.value(), owner);
      });
    }

    element_type& operator*() const noexcept { return *value_; }
    element_type* operator->() const noexcept { return get(); }
    element_type* get() const noexcept { return value_.get(); }
    operator bool() const noexcept {
      return static_cast<bool>(value_);
    }

   private:
    static void reset_impl(typename T::ptr& obj, unbounded_object_pool*& owner) noexcept {
      if (!owner) {
        // already destroyed
        return;
      }

      owner->release(std::move(obj));
      obj = typename T::ptr{}; // reset value
      owner = nullptr; // mark as destroyed
    }

    typename T::ptr value_;
    unbounded_object_pool* owner_;
  };

  explicit unbounded_object_pool(size_t size = 0)
    : base_t(size) {
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
      head->value = typename T::ptr{}; // empty instance
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

namespace detail {

template<typename Pool>
struct pool_generation {
  explicit pool_generation(Pool* owner) noexcept
    : owner(owner) {
  }

  bool stale{false}; // stale mark
  Pool* owner; // current owner
}; // pool_generation

} // detail

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
class unbounded_object_pool_volatile
    : public unbounded_object_pool_base<T>,
      private atomic_shared_ptr_helper<
        async_value<detail::pool_generation<unbounded_object_pool_volatile<T>>>
      > {
 private:
  typedef unbounded_object_pool_base<T> base_t;

  typedef async_value<detail::pool_generation<unbounded_object_pool_volatile<T>>> generation_t;
  typedef std::shared_ptr<generation_t> generation_ptr_t;
  typedef atomic_shared_ptr_helper<generation_t> atomic_utils;

  typedef std::lock_guard<typename generation_t::read_lock_type> read_guard_t;
  typedef std::lock_guard<typename generation_t::write_lock_type> write_guard_t;

 public:
  typedef typename base_t::element_type element_type;

  /////////////////////////////////////////////////////////////////////////////
  /// @class ptr
  /// @brief represents a control object of unbounded_object_pool with
  ///        semantic similar to smart pointers
  /////////////////////////////////////////////////////////////////////////////
  class ptr : util::noncopyable {
   public:
    ptr(typename T::ptr&& value,
        const generation_ptr_t& gen) noexcept
      : value_(std::move(value)),
        gen_(std::move(gen)) {
    }

    ptr(ptr&& rhs) noexcept
      : value_(std::move(rhs.value_)),
        gen_(std::move(rhs.gen_)) {
    }

    ptr& operator=(ptr&& rhs) noexcept {
      if (this != &rhs) {
        value_ = std::move(rhs.value_);
        gen_ = std::move(rhs.gen_);
      }
      return *this;
    }

    ~ptr() noexcept {
      reset();
    }

    FORCE_INLINE void reset() noexcept {
      reset_impl(value_, gen_);
    }

    std::shared_ptr<element_type> release() {
      auto* raw = get();
      auto moved_value = make_move_on_copy(std::move(value_));
      auto moved_gen = make_move_on_copy(std::move(gen_));
      assert(!gen_); // moved

      // in case if exception occurs
      // destructor will be called
      return std::shared_ptr<element_type>(
        raw,
        [moved_gen, moved_value] (element_type*) mutable noexcept {
          reset_impl(moved_value.value(), moved_gen.value());
      });
    }

    element_type& operator*() const noexcept { return *value_; }
    element_type* operator->() const noexcept { return get(); }
    element_type* get() const noexcept { return value_.get(); }
    operator bool() const noexcept {
      return static_cast<bool>(gen_);
    }

   private:
    static void reset_impl(typename T::ptr& obj, generation_ptr_t& gen) noexcept {
      if (!gen) {
        // already destroyed
        return;
      }

      // do not remove scope!!!
      // variable 'lock' below must be destroyed before 'gen_'
      {
        read_guard_t lock(gen->read_lock());
        auto& value = gen->value();

        if (!value.stale) {
          value.owner->release(std::move(obj));
        }
      }

      obj = typename T::ptr{}; // clear object oustide the lock
      gen = nullptr; // mark as destroyed
    }

    typename T::ptr value_;
    generation_ptr_t gen_;
  }; // ptr

  explicit unbounded_object_pool_volatile(size_t size = 0)
    : base_t(size),
      gen_(memory::make_shared<generation_t>(this)) {
  }

  // FIXME check what if
  //
  // unbounded_object_pool_volatile p0, p1;
  // thread0: p0.clear();
  // thread1: unbounded_object_pool_volatile p1(std::move(p0));
  unbounded_object_pool_volatile(unbounded_object_pool_volatile&& rhs) noexcept
    : base_t(std::move(rhs)) {
    gen_ = atomic_utils::atomic_load(&rhs.gen_);

    write_guard_t lock(gen_->write_lock());
    gen_->value().owner = this; // change owner

    this->free_slots_ = std::move(rhs.free_slots_);
    this->free_objects_ = std::move(rhs.free_objects_);
  }

  ~unbounded_object_pool_volatile() noexcept {
    // prevent existing elements from returning into the pool
    // if pool doesn't own generation anymore
    const auto gen = atomic_utils::atomic_load(&gen_);
    write_guard_t lock(gen->write_lock());

    auto& value = gen->value();

    if (value.owner == this) {
      value.stale = true;
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
        write_guard_t lock(gen->write_lock());
        gen->value().stale = true;
      }

      // mark new generation
      atomic_utils::atomic_store(&gen_, memory::make_shared<generation_t>(this));
    }

    typename base_t::node* head = nullptr;

    // reset all cached instances
    while ((head = this->free_objects_.pop())) {
      head->value = typename T::ptr{}; // empty instance
      this->free_slots_.push(*head);
    }
  }

  template<typename... Args>
  ptr emplace(Args&&... args) {
    static const generation_ptr_t no_gen;
    const auto gen = atomic_utils::atomic_load(&gen_); // retrieve before seek/instantiate
    auto value = this->acquire(std::forward<Args>(args)...);

    return value ? ptr(std::move(value), gen) : ptr(nullptr, no_gen);
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
