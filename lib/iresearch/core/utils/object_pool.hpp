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

#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <utility>
#include <vector>

#include "async_utils.hpp"
#include "memory.hpp"
#include "misc.hpp"
#include "noncopyable.hpp"
#include "shared.hpp"
#include "thread_utils.hpp"

namespace irs {

// Lock-free stack.
// Move construction/assignment is not thread-safe.
template<typename T>
class concurrent_stack : private util::noncopyable {
 public:
  using element_type = T;

  struct node_type {
    element_type value{};
    // next needs to be atomic because
    // nodes are kept in a free-list and reused!
    std::atomic<node_type*> next{};
  };

  explicit concurrent_stack(node_type* head = nullptr) noexcept
    : head_{concurrent_node{head}} {}

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

      new_head.node = head.node->next.load(std::memory_order_acquire);
      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(
      head, new_head, std::memory_order_acquire, std::memory_order_relaxed));

    return head.node;
  }

  void push(node_type& new_node) noexcept {
    concurrent_node head = head_.load(std::memory_order_relaxed);
    concurrent_node new_head{&new_node};

    do {
      new_node.next.store(head.node, std::memory_order_release);

      new_head.version = head.version + 1;
    } while (!head_.compare_exchange_weak(
      head, new_head, std::memory_order_release, std::memory_order_relaxed));
  }

 private:
  // CMPXCHG16B requires that the destination
  // (memory) operand be 16-byte aligned
  struct alignas(kCmpXChg16Align) concurrent_node {
    explicit concurrent_node(node_type* node = nullptr) noexcept
      : version{0}, node{node} {}

    uintptr_t version;  // avoid aba problem
    node_type* node;
  };

  static_assert(kCmpXChg16Align == alignof(concurrent_node),
                "invalid alignment");

  void move_unsynchronized(concurrent_stack&& rhs) noexcept {
    head_ = rhs.head_.load(std::memory_order_relaxed);
    rhs.head_.store(concurrent_node{}, std::memory_order_relaxed);
  }

  std::atomic<concurrent_node> head_;
};

// Represents a control object of unbounded_object_pool
template<typename T, typename D>
class pool_control_ptr final : public std::unique_ptr<T, D> {
 public:
  using std::unique_ptr<T, D>::unique_ptr;

  pool_control_ptr() = default;

  // Intentionally hides std::unique_ptr<...>::reset() as we
  // disallow changing the owned pointer.
  void reset() noexcept { std::unique_ptr<T, D>::reset(); }
};

// A fixed size pool of objects.
// if the pool is empty then a new object is created via make(...)
// if an object is available in a pool then in is returned but tracked
// by the pool.
// when the object is released it is placed back into the pool
// Object 'ptr' that evaluate to false after make(...) are not considered
// part of the pool.
template<typename T>
class bounded_object_pool {
 public:
  using element_type = typename T::ptr::element_type;

 private:
  struct slot : util::noncopyable {
    bounded_object_pool* owner;
    typename T::ptr ptr;
    std::atomic<element_type*> value{};
  };

  using stack = concurrent_stack<slot>;
  using node_type = typename stack::node_type;

  // Private because we want to disallow upcasts to std::unique_ptr<...>.
  class releaser final {
   public:
    explicit releaser(node_type* slot) noexcept : slot_{slot} {}

    void operator()(element_type*) noexcept {
      IRS_ASSERT(slot_ && slot_->value.owner);  // Ensured by emplace(...)
      slot_->value.owner->unlock(*slot_);
    }

   private:
    node_type* slot_;
  };

 public:
  // Represents a control object of unbounded_object_pool
  using ptr = pool_control_ptr<element_type, releaser>;

  explicit bounded_object_pool(size_t size) : pool_{std::max(size_t(1), size)} {
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
        slot.value.store(p, std::memory_order_release);
        return ptr(p, releaser{head});
      }

      free_list_.push(*head);  // put empty slot back into the free list
      cond_.notify_all();

      return ptr(nullptr, releaser{nullptr});
    }

    return ptr(p, releaser{head});
  }

  size_t size() const noexcept { return pool_.size(); }

  template<typename Visitor>
  bool visit(const Visitor& visitor) {
    return const_cast<const bounded_object_pool&>(*this).visit(visitor);
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    stack list;

    Finally release_all = [this, &list]() noexcept {
      while (auto* head = list.pop()) {
        free_list_.push(*head);
      }
    };

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

    std::unique_lock lock{mutex_};

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
};

// Base class for all unbounded object pool implementations
template<typename T,
         typename =
           std::enable_if_t<is_unique_ptr_v<typename T::ptr> &&
                            std::is_empty_v<typename T::ptr::deleter_type>>>
class unbounded_object_pool_base : private util::noncopyable {
 public:
  using deleter_type = typename T::ptr::deleter_type;
  using element_type = typename T::ptr::element_type;
  using pointer = typename T::ptr::pointer;

 private:
  struct slot : util::noncopyable {
    pointer value{};
  };

 public:
  size_t size() const noexcept { return pool_.size(); }

 protected:
  using stack = concurrent_stack<slot>;
  using node = typename stack::node_type;

  explicit unbounded_object_pool_base(size_t size)
    : pool_(size), free_slots_{pool_.data()} {
    // build up linked list
    for (auto begin = pool_.begin(), end = pool_.end(),
              next = begin < end ? (begin + 1) : end;
         next < end; begin = next, ++next) {
      begin->next = &*next;
    }
  }

  template<typename... Args>
  pointer acquire(Args&&... args) {
    auto* head = free_objects_.pop();

    if (head) {
      auto value = std::exchange(head->value.value, nullptr);
      IRS_ASSERT(value);
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

    [[maybe_unused]] const auto old_value =
      std::exchange(slot->value.value, value);
    IRS_ASSERT(!old_value);
    free_objects_.push(*slot);
  }

  unbounded_object_pool_base(unbounded_object_pool_base&& rhs) noexcept
    : pool_{std::move(rhs.pool_)} {
    // need for volatile pool only
  }
  unbounded_object_pool_base& operator=(unbounded_object_pool_base&&) = delete;

  std::vector<node> pool_;
  stack free_objects_;  // list of created objects that are ready to be reused
  stack free_slots_;    // list of free slots to be reused
};

// A fixed size pool of objects
// if the pool is empty then a new object is created via make(...)
// if an object is available in a pool then in is returned and no
// longer tracked by the pool
// when the object is released it is placed back into the pool if
// space in the pool is available
// pool owns produced object so it's not allowed to destroy before
// all acquired objects will be destroyed.
// Object 'ptr' that evaluate to false when returned back into the pool
// will be discarded instead.
template<typename T>
class unbounded_object_pool : public unbounded_object_pool_base<T> {
 private:
  using base_t = unbounded_object_pool_base<T>;
  using node = typename base_t::node;

 public:
  using element_type = typename base_t::element_type;
  using pointer = typename base_t::pointer;

 private:
  // Private because we want to disallow upcasts to std::unique_ptr<...>.
  class releaser final {
   public:
    explicit releaser(unbounded_object_pool& owner) noexcept : owner_{&owner} {}

    releaser() noexcept : owner_{nullptr} {}

    void operator()(pointer p) const noexcept {
      IRS_ASSERT(p);  // Ensured by std::unique_ptr<...>
      IRS_ASSERT(owner_);
      owner_->release(p);
    }

   private:
    unbounded_object_pool* owner_;
  };

 public:
  // Represents a control object of unbounded_object_pool
  using ptr = pool_control_ptr<element_type, releaser>;

  explicit unbounded_object_pool(size_t size = 0) : base_t{size} {}

  ~unbounded_object_pool() {
    for (auto& slot : this->pool_) {
      if (auto p = slot.value.value; p != nullptr) {
        typename base_t::deleter_type{}(p);
      }
    }
  }

#if defined(_MSC_VER)
#pragma warning(disable : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

  // Clears all cached objects
  void clear() {
    node* head = nullptr;

    // reset all cached instances
    while ((head = this->free_objects_.pop())) {
      auto p = std::exchange(head->value.value, nullptr);
      IRS_ASSERT(p);
      typename base_t::deleter_type{}(p);
      this->free_slots_.push(*head);
    }
  }

#if defined(_MSC_VER)
#pragma warning(default : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  template<typename... Args>
  ptr emplace(Args&&... args) {
    return {this->acquire(std::forward<Args>(args)...), releaser{*this}};
  }

 private:
  // disallow move
  unbounded_object_pool(unbounded_object_pool&&) = delete;
  unbounded_object_pool& operator=(unbounded_object_pool&&) = delete;
};

}  // namespace irs
