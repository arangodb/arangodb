////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/system-compiler.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

/**
 * SYNOPSIS
 *
 * The class Guarded contains a value and an associated mutex. It does allow
 * access to the value only while owning a lock to the mutex.
 *
 * For example, given a
 *
 *     struct UnderGuard { int value; }
 *
 * , define a guarded object as follows:
 *
 *     Guarded<UnderGuard> guarded(7);
 *
 * The constructor's arguments will be forwarded.
 *
 * It can either be accessed by passing a lambda:
 *
 *     guarded.doUnderLock([](UnderGuard& obj) { obj.value = 12; });
 *
 * This will lock the mutex before the lambda's execution, and release it after.
 *
 * Or it can be accessed by creating a MutexGuard:
 *
 *     auto mutexGuard = guarded.getLockedGuard();
 *     mutexGuard->value = 13;
 *
 * getLockedGuard() will lock the mutex, and mutexGuard will release it upon
 * destruction.
 *
 * For simple access, there are copy() and store():
 *
 *     UnderGuard value = guarded.copy();
 *     guarded.store(UnderGuard{3});
 *
 * If copy/assign don't suffice for some reason - e.g. because you want to:
 *  - "try" for the lock, or
 *  - access to one specific member or anything like that, or
 *  - get/modify a non-copy-constructible or non-copy-assignable value,
 *  use any of the more general methods described above instead.
 */

namespace arangodb {

namespace basics {
class UnshackledMutex;
}

namespace details {
template<typename Lock, typename ConditionVariable>
concept CanWait =
    (std::is_same_v<ConditionVariable, std::condition_variable> &&
     std::is_same_v<Lock, std::unique_lock<std::mutex>>) ||
    (std::is_same_v<ConditionVariable, std::condition_variable_any> &&
     std::is_same_v<Lock, std::unique_lock<basics::UnshackledMutex>>);

template<typename Lock>
concept IsSharedLock = requires(Lock lock) {
  lock.lock_shared();
};
}  // namespace details

template<class T, class L>
class MutexGuard {
 public:
  explicit MutexGuard(T& value, L mutexLock);
  ~MutexGuard() = default;

  MutexGuard(MutexGuard&&) noexcept = default;
  auto operator=(MutexGuard&&) noexcept -> MutexGuard& = default;

  auto get() const noexcept -> T&;
  auto operator->() const noexcept -> T*;

  // @brief Unlocks and releases the mutex, and releases the value.
  //        The guard is unusable after this, and the value inaccessible from
  //        the guard.
  void unlock() noexcept(noexcept(std::declval<L>().unlock()));

  // The wait() methods here delegate to std::condition_variable or
  // std::condition_variable_any. Except
  // that these here are methods of the MutexGuard (rather than
  // std::condition_variable), and accordingly take the condition variable as
  // first argument (rather than the lock).
  // Feel free to add waitFor and/or waitUntil if needed.
  template<typename ConditionVariable>
  auto wait(
      ConditionVariable& cv) requires details::CanWait<L, ConditionVariable>;

  template<class Predicate, typename ConditionVariable>
  auto wait(ConditionVariable& cv, Predicate stop_waiting) requires
      details::CanWait<L, ConditionVariable>;

  auto isLocked() const noexcept -> bool;

 private:
  struct nop {
    void operator()(T*) {}
  };
  std::unique_ptr<T, nop> _value;
  L _mutexLock;
};

template<class T, class L>
MutexGuard<T, L>::MutexGuard(T& value, L mutexLock)
    : _value(&value), _mutexLock(std::move(mutexLock)) {
  if (ADB_UNLIKELY(!_mutexLock.owns_lock())) {
    throw std::invalid_argument("Lock not owned");
  }
}

template<class T, class L>
auto MutexGuard<T, L>::get() const noexcept -> T& {
  return *_value;
}

template<class T, class L>
auto MutexGuard<T, L>::operator->() const noexcept -> T* {
  return _value.get();
}

template<class T, class L>
void MutexGuard<T, L>::unlock() noexcept(noexcept(std::declval<L>().unlock())) {
  static_assert(noexcept(_value.reset()));
  static_assert(noexcept(_mutexLock.release()));
  _value.reset();
  _mutexLock.unlock();
  _mutexLock.release();
}

template<class T, class L>
auto MutexGuard<T, L>::isLocked() const noexcept -> bool {
  return _value != nullptr;
}

template<class T, class L>
template<typename ConditionVariable>
auto MutexGuard<T, L>::wait(
    ConditionVariable& cv) requires details::CanWait<L, ConditionVariable> {
  return cv.wait(_mutexLock);
}

template<class T, class L>
template<class Predicate, typename ConditionVariable>
auto MutexGuard<T, L>::wait(
    ConditionVariable& cv,
    Predicate stop_waiting) requires details::CanWait<L, ConditionVariable> {
  return cv.wait(_mutexLock, std::forward<Predicate>(stop_waiting));
}

template<class T, class M = std::mutex,
         template<class> class UniqueLock = std::unique_lock,
         template<class> class SharedLock = std::shared_lock>
class Guarded {
 public:
  using value_type = T;
  using mutex_type = M;
  using lock_type = UniqueLock<M>;
  using shared_lock_type =
      std::conditional_t<details::IsSharedLock<M>, SharedLock<M>, void>;

  explicit Guarded(T&& value = T());
  template<typename... Args>
  explicit Guarded(Args&&...);

  ~Guarded() = default;
  Guarded(Guarded const&) = delete;
  Guarded(Guarded&&) = delete;
  auto operator=(Guarded const&) -> Guarded& = delete;
  auto operator=(Guarded&&) -> Guarded& = delete;

  template<class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderLock(F&& callback) -> R;
  template<class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderLock(F&& callback) const -> R;

  template<class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderExclusiveLock(F&& callback) -> R;
  template<class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderExclusiveLock(F&& callback) const -> R;

  template<class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderSharedLock(F&& callback) const
      -> R requires details::IsSharedLock<M>;

  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) -> std::optional<Q>;
  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) const -> std::optional<Q>;

  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderExclusiveLock(F&& callback) -> std::optional<Q>;
  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderExclusiveLock(F&& callback) const
      -> std::optional<Q>;

  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderSharedLock(F&& callback) const -> std::optional<Q>
  requires details::IsSharedLock<M>;

  // get a copy of the value, made under the lock.
  template<typename U = T>
  auto copy() const -> T requires std::is_copy_constructible_v<U>;

  // assign a new value using operator=, under the lock.
  template<class U>
  void assign(U&&) requires std::is_assignable_v<T, U>;

  using mutex_guard_type = MutexGuard<value_type, lock_type>;
  using const_mutex_guard_type = MutexGuard<value_type const, lock_type>;

  auto getLockedGuard() -> mutex_guard_type;
  auto getLockedGuard() const -> const_mutex_guard_type;

  auto getExclusiveLockedGuard() -> mutex_guard_type;
  auto getExclusiveLockedGuard() const -> const_mutex_guard_type;

  auto getSharedLockedGuard() const -> const_mutex_guard_type requires
      details::IsSharedLock<M>;  // TODO use shared_lock_type

  auto tryLockedGuard() -> std::optional<MutexGuard<value_type, lock_type>>;
  auto tryLockedGuard() const
      -> std::optional<MutexGuard<value_type const, lock_type>>;
  // TODO add more types of "get guard" (like try or eventual)

 private:
  template<class F, class R = std::invoke_result_t<F, value_type&>,
           class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] static auto tryCallUnderLock(M& mutex, F&& callback, T& value)
      -> std::optional<Q>;

  value_type _value;
  mutable mutex_type _mutex;
};

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
Guarded<T, M, UniqueLock, SharedLock>::Guarded(T&& value)
    : _value{std::move(value)}, _mutex{} {}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<typename... Args>
Guarded<T, M, UniqueLock, SharedLock>::Guarded(Args&&... args)
    : _value{std::forward<Args>(args)...}, _mutex{} {}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class F, class R>
auto Guarded<T, M, UniqueLock, SharedLock>::doUnderLock(F&& callback) -> R {
  auto guard = lock_type(_mutex);
  return std::forward<F>(callback)(_value);
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class F, class R>
auto Guarded<T, M, UniqueLock, SharedLock>::doUnderLock(F&& callback) const
    -> R {
  auto guard = lock_type(_mutex);
  return std::forward<F>(callback)(_value);
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class F, class R, class Q>
auto Guarded<T, M, UniqueLock, SharedLock>::tryCallUnderLock(M& mutex,
                                                             F&& callback,
                                                             T& value)
    -> std::optional<Q> {
  auto guard = lock_type(mutex, std::try_to_lock);

  if (guard.owns_lock()) {
    if constexpr (!std::is_void_v<R>) {
      return std::forward<F>(callback)(value);
    } else {
      std::forward<F>(callback)(value);
      return std::monostate{};
    }
  } else {
    return std::nullopt;
  }
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class F, class R, class Q>
auto Guarded<T, M, UniqueLock, SharedLock>::tryUnderLock(F&& callback)
    -> std::optional<Q> {
  return tryCallUnderLock(_mutex, std::forward<F>(callback), _value);
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class F, class R, class Q>
auto Guarded<T, M, UniqueLock, SharedLock>::tryUnderLock(F&& callback) const
    -> std::optional<Q> {
  return tryCallUnderLock(_mutex, std::forward<F>(callback), _value);
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<typename U>
auto Guarded<T, M, UniqueLock, SharedLock>::copy() const
    -> T requires std::is_copy_constructible_v<U> {
  auto guard = lock_type(this->_mutex);
  return _value;
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
template<class U>
void Guarded<T, M, UniqueLock, SharedLock>::assign(
    U&& value) requires std::is_assignable_v<T, U> {
  auto guard = lock_type(_mutex);
  _value = std::forward<U>(value);
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
auto Guarded<T, M, UniqueLock, SharedLock>::getLockedGuard()
    -> MutexGuard<T, lock_type> {
  return MutexGuard(_value, lock_type(_mutex));
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
auto Guarded<T, M, UniqueLock, SharedLock>::getLockedGuard() const
    -> MutexGuard<T const, lock_type> {
  return MutexGuard(_value, lock_type(_mutex));
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
auto Guarded<T, M, UniqueLock, SharedLock>::tryLockedGuard()
    -> std::optional<MutexGuard<T, lock_type>> {
  auto lock = lock_type(_mutex, std::try_to_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

template<class T, class M, template<class> class UniqueLock,
         template<class> class SharedLock>
auto Guarded<T, M, UniqueLock, SharedLock>::tryLockedGuard() const
    -> std::optional<MutexGuard<T const, lock_type>> {
  auto lock = lock_type(_mutex, std::try_to_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

}  // namespace arangodb
