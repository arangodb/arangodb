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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_OBJECT_POOL_H
#define IRESEARCH_OBJECT_POOL_H

#include <algorithm>
#include <atomic>
#include <functional>

#include "memory.hpp"
#include "shared.hpp"
#include "thread_utils.hpp"
#include "misc.hpp"
#include "noncopyable.hpp"

NS_ROOT

template<typename Ptr>
class atomic_base {
 public:
  Ptr& atomic_exchange(Ptr* p, Ptr& r) const {
    SCOPED_LOCK(mutex_);
    p->swap(r);
    return r;
  }

  void atomic_store(Ptr* p, Ptr& r) const  {
    SCOPED_LOCK(mutex_);
    p->swap(r); // FIXME
  }

  const Ptr& atomic_load(const Ptr* p) const {
    SCOPED_LOCK(mutex_);
    return *p;
  }

 private:
  mutable std::mutex mutex_;
};

// GCC prior the 5.0 does not support std::atomic_exchange(std::shared_ptr<T>*, std::shared_ptr<T>)
#if !defined(__GNUC__) || (__GNUC__ >= 5)
  template<typename T>
  class atomic_base<std::shared_ptr<T>> {
    #if defined(IRESEARCH_VALGRIND) // suppress valgrind false-positives related to std::atomic_*
     public:
      std::shared_ptr<T> atomic_exchange(std::shared_ptr<T>* p, std::shared_ptr<T> r) const {
        SCOPED_LOCK(mutex_);
        return std::atomic_exchange(p, r);
      }

      void atomic_store(std::shared_ptr<T>* p, std::shared_ptr<T> r) const {
        SCOPED_LOCK(mutex_);
        std::atomic_store(p, r);
      }

      std::shared_ptr<T> atomic_load(const std::shared_ptr<T>* p) const {
        SCOPED_LOCK(mutex_);
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
  };
#endif

template<typename T>
class move_on_copy { 
 public:
  move_on_copy(T&& value) : value_(std::move(value)) {}
  move_on_copy(const move_on_copy& rhs) : value_(std::move(rhs.value_)) {}

  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  move_on_copy& operator=(move_on_copy&&) = delete;
  move_on_copy& operator=(const move_on_copy&) = delete;
  
  mutable T value_;
};

template<typename T>
move_on_copy<T> make_move_on_copy(T&& value) {
  static_assert(std::is_rvalue_reference<decltype(value)>::value, "parameter should be an rvalue");
  return move_on_copy<T>(std::move(value));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief a fixed size pool of objects
///        if the pool is empty then a new object is created via make(...)
///        if an object is available in a pool then in is returned but tracked
///        by the pool
///        when the object is released it is placed back into the pool
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class bounded_object_pool : atomic_base<typename T::ptr> {
 private:
  struct slot_t : util::noncopyable {
    bounded_object_pool* owner;
    typename T::ptr ptr;
    std::atomic_flag used;
  }; // slot_t

 public:
  typedef atomic_base<typename T::ptr> atomic_utils;
  typedef typename T::ptr::element_type element_type;

  // do not use std::shared_ptr to avoid unnecessary heap allocatons
  class ptr : util::noncopyable {
   public:
    ptr(slot_t& slot) NOEXCEPT
      : slot_(&slot) {
    }

    ptr(ptr&& rhs) NOEXCEPT
      : slot_(rhs.slot_) {
      rhs.slot_ = nullptr; // take ownership
    }

    ptr& operator=(ptr&& rhs) NOEXCEPT {
      if (this != &rhs) {
        slot_ = rhs.slot_;
        rhs.slot_ = nullptr; // take ownership
      }
      return *this;
    }

    ~ptr() NOEXCEPT {
      reset();
    }

    void reset() NOEXCEPT {
      if (!slot_) {
        // nothing to do
        return;
      }

      assert(slot_->owner);
      slot_->owner->unlock(*slot_);
      slot_ = nullptr; // release ownership
    }

    element_type& operator*() const NOEXCEPT { return *slot_->ptr; }
    element_type* operator->() const NOEXCEPT { return get(); }
    element_type* get() const NOEXCEPT { return slot_->ptr.get(); }
    operator bool() const NOEXCEPT {
      return static_cast<bool>(slot_->ptr);
    }

   private:
    slot_t* slot_;
  }; // ptr

  explicit bounded_object_pool(size_t size)
    : free_count_(std::max(size_t(1), size)), 
      pool_(std::max(size_t(1), size)) {
    // initialize pool
    for (auto& slot : pool_) {
      slot.used.clear();
      slot.owner = this;
    }
  }

  template<typename... Args>
  ptr emplace(Args&&... args) {
    for(;;) {
      // linear search for an unused slot in the pool
      for (auto& slot : pool_) {
        if (lock(slot)) {
          auto& p = slot.ptr;
          if (!p) {
            try {
              auto value = T::make(std::forward<Args>(args)...);
              atomic_utils::atomic_store(&p, value);
            } catch (...) {
              unlock(slot);
              throw;
            }
          }

          return ptr(slot);
        }
      }

      wait_for_free_slots();
    }
  }

  size_t size() const NOEXCEPT { return pool_.size(); }

  template<typename Visitor>
  bool visit(const Visitor& visitor, bool shared = false) {
    return const_cast<const bounded_object_pool&>(*this).visit(visitor, shared);
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor, bool shared = false) const {
    // shared visitation does not acquire exclusive lock on object
    // it is up to the caller to guarantee proper synchronization
    if (shared) {
      for (auto& slot : pool_) {
        auto& p = slot.ptr;
        auto* obj = atomic_utils::atomic_load(&p).get();

        if (obj && !visitor(*obj)) {
          return false;
        }
      }

      return true;
    }

    std::vector<bool> used(pool_.size(), false);
    auto size = used.size();

    auto unlock_all = make_finally([this, &used] () {
      for (size_t i = 0, count = used.size(); i < count; ++i) {
        if (used[i]) {
          unlock(pool_[i]);
        }
      }
    });

    for (;;) {
      for (size_t i = 0, count = pool_.size(); i < count; ++i) {
        if (used[i]) {
          continue;
        }

        auto& slot = pool_[i];
        if (lock(slot)) {
          used[i] = true;

          auto& p = slot.ptr;
          auto* obj = atomic_utils::atomic_load(&p).get();

          if (obj && !visitor(*obj)) {
            return false;
          }

          if (1 == size) {
            return true;
          }

          --size;
        }
      }

      wait_for_free_slots();
    }

    return true;
  }

 private:
  void wait_for_free_slots() const {
    SCOPED_LOCK_NAMED(mutex_, lock);

    if (!free_count_) {
      cond_.wait_for(lock, std::chrono::milliseconds(1000));
    }
  }

  // MSVC 2017.3, 2017.4 and 2017.5 incorectly decrement counter if this function is inlined during optimization
  // MSVC 2017.2 and below TODO test for both debug and release
  MSVC2017_345_OPTIMIZED_WORKAROUND(__declspec(noinline))
  bool lock(slot_t& slot) const {
    if (!slot.used.test_and_set()) {
      --free_count_;

      return true;
    }

    return false;
  }

  // MSVC 2017.3, 2017.4 and 2017.5 incorectly increment counter if this function is inlined during optimization
  // MSVC 2017.2 and below TODO test for both debug and release
  MSVC2017_345_OPTIMIZED_WORKAROUND(__declspec(noinline))
  void unlock(slot_t& slot) const {
    slot.used.clear();
    SCOPED_LOCK(mutex_);
    ++free_count_; // under lock to ensure cond_.wait_for(...) not triggered
    cond_.notify_all();
  }

  mutable std::condition_variable cond_;
  mutable std::atomic<size_t> free_count_;
  mutable std::mutex mutex_;
  mutable std::vector<slot_t> pool_;
}; // bounded_object_pool

//////////////////////////////////////////////////////////////////////////////
/// @brief a fixed size pool of objects
///        if the pool is empty then a new object is created via make(...)
///        if an object is available in a pool then in is returned and no
///        longer tracked by the pool
///        when the object is released it is placed back into the pool if
///        space in the pool is available
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class unbounded_object_pool
  : private atomic_base<typename T::ptr>,
    private atomic_base<std::shared_ptr<std::atomic<bool>>> {
 public:
  DECLARE_SPTR(typename T::ptr::element_type);

  explicit unbounded_object_pool(size_t size)
    : pool_(size), reusable_(memory::make_unique<std::atomic<bool>>(true)) {
  }

  void clear() {
    auto reusable = atomic_bool_utils::atomic_load(&reusable_);

    reusable->store(false); // prevent existing element from returning into the pool
    reusable = memory::make_unique<std::atomic<bool>>(true); // mark new generation
    atomic_bool_utils::atomic_store(&reusable_, reusable);

    // linearly reset all cached instances
    for(size_t i = 0, count = pool_.size(); i < count; ++i) {
      typename T::ptr empty;
      atomic_utils::atomic_exchange(&(pool_[i]), empty);
    }
  }

  template<typename... Args>
  ptr emplace(Args&&... args) {
    auto reusable = atomic_bool_utils::atomic_load(&reusable_); // retrieve before seek/instantiate
    typename T::ptr value = nullptr;

    // liniar search for an existing entry in the pool
    for(size_t i = 0, count = pool_.size(); i < count && !value; ++i) {
      value = std::move(atomic_utils::atomic_exchange(&(pool_[i]), value));
    }

    if (!value) {
      value = T::make(std::forward<Args>(args)...);
    }

    // unique_ptr can't be copied
    auto raw = value.get();
    auto moved = make_move_on_copy(std::move(value));

    return ptr(
      raw,
      [this, moved, reusable](typename ptr::element_type*) mutable ->void {
        if (!static_cast<atomic_bool_utils*>(this)->atomic_load(&reusable)->load()) {
          return; // do not return non-reusable elements into the pool
        }

        typename T::ptr val = std::move(moved.value());

        // linear search for an empty slot in the pool
        for(size_t i = 0, count = pool_.size(); i < count && val; ++i) {
          val = std::move(atomic_utils::atomic_exchange(&(pool_[i]), val));
        }
      }
    );
  }

  size_t size() const { return pool_.size(); }

 private:
  typedef std::shared_ptr<std::atomic<bool>> reusable_t;
  typedef atomic_base<typename T::ptr> atomic_utils;
  typedef atomic_base<reusable_t> atomic_bool_utils;
  std::vector<typename T::ptr> pool_;
  reusable_t reusable_;
}; // unbounded_object_pool

NS_END

#endif