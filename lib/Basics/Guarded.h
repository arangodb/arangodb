////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
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

class Mutex;

template <class T, class L>
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

 private:
  struct nop {
    void operator()(T*) {}
  };
  std::unique_ptr<T, nop> _value;
  L _mutexLock;
};

template <class T, class L>
MutexGuard<T, L>::MutexGuard(T& value, L mutexLock)
    : _value(&value), _mutexLock(std::move(mutexLock)) {
  if (ADB_UNLIKELY(!_mutexLock.owns_lock())) {
    throw std::invalid_argument("Lock not owned");
  }
}

template <class T, class L>
auto MutexGuard<T, L>::get() const noexcept -> T& {
  return *_value;
}

template <class T, class L>
auto MutexGuard<T, L>::operator->() const noexcept -> T* {
  return _value.get();
}

template <class T, class L>
void MutexGuard<T, L>::unlock() noexcept(noexcept(std::declval<L>().unlock())) {
  static_assert(noexcept(_value.reset()));
  static_assert(noexcept(_mutexLock.release()));
  _value.reset();
  _mutexLock.unlock();
  _mutexLock.release();
}

template <class T, class M = std::mutex, template <class> class L = std::unique_lock>
class Guarded {
 public:
  using value_type = T;
  using mutex_type = M;
  using lock_type = L<M>;

  explicit Guarded(T&& value = T());
  template <typename... Args>
  explicit Guarded(Args&&...);

  ~Guarded() = default;
  Guarded(Guarded const&) = delete;
  Guarded(Guarded&&) = delete;
  auto operator=(Guarded const&) -> Guarded& = delete;
  auto operator=(Guarded&&) -> Guarded& = delete;

  template <class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderLock(F&& callback) -> R;
  template <class F, class R = std::invoke_result_t<F, value_type&>>
  auto doUnderLock(F&& callback) const -> R;

  template <class F, class R = std::invoke_result_t<F, value_type&>,
            class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) -> std::optional<Q>;
  template <class F, class R = std::invoke_result_t<F, value_type&>,
            class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) const -> std::optional<Q>;

  // get a copy of the value, made under the lock.
  template <typename U = T, std::enable_if_t<std::is_copy_constructible_v<U>, int> = 0>
  auto copy() const -> T;

  // assign a new value using operator=, under the lock.
  template <class U, std::enable_if_t<std::is_assignable_v<T, U>, int> = 0>
  void assign(U&&);

  using mutex_guard_type = MutexGuard<value_type, lock_type>;
  using const_mutex_guard_type = MutexGuard<value_type const, lock_type>;

  auto getLockedGuard() -> mutex_guard_type;
  auto getLockedGuard() const -> const_mutex_guard_type;

  auto tryLockedGuard() -> std::optional<MutexGuard<value_type, lock_type>>;
  auto tryLockedGuard() const -> std::optional<MutexGuard<value_type const, lock_type>>;
  // TODO add more types of "get guard" (like try or eventual)

 private:
  template <class F, class R = std::invoke_result_t<F, value_type&>,
            class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] static auto tryCallUnderLock(M& Mutex, F&& callback, T& value)
      -> std::optional<Q>;

  value_type _value;
  mutable mutex_type _mutex;
};

template <class T, class M, template <class> class L>
Guarded<T, M, L>::Guarded(T&& value) : _value{std::move(value)}, _mutex{} {}

template <class T, class M, template <class> class L>
template <typename... Args>
Guarded<T, M, L>::Guarded(Args&&... args) : _value{std::forward<Args>(args)...}, _mutex{} {}

template <class T, class M, template <class> class L>
template <class F, class R>
auto Guarded<T, M, L>::doUnderLock(F&& callback) -> R {
  auto guard = lock_type(_mutex);
  return std::invoke(std::forward<F>(callback), _value);
}

template <class T, class M, template <class> class L>
template <class F, class R>
auto Guarded<T, M, L>::doUnderLock(F&& callback) const -> R {
  auto guard = lock_type(_mutex);
  return std::invoke(std::forward<F>(callback), _value);
}

template <class T, class M, template <class> class L>
template <class F, class R, class Q>
auto Guarded<T, M, L>::tryCallUnderLock(M& mutex, F&& callback, T& value) -> std::optional<Q> {
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

template <class T, class M, template <class> class L>
template <class F, class R, class Q>
auto Guarded<T, M, L>::tryUnderLock(F&& callback) -> std::optional<Q> {
  return tryCallUnderLock(_mutex, std::forward<F>(callback), _value);
}

template <class T, class M, template <class> class L>
template <class F, class R, class Q>
auto Guarded<T, M, L>::tryUnderLock(F&& callback) const -> std::optional<Q> {
  return tryCallUnderLock(_mutex, std::forward<F>(callback), _value);
}

template <class T, class M, template <class> class L>
template <typename U, std::enable_if_t<std::is_copy_constructible_v<U>, int>>
auto Guarded<T, M, L>::copy() const -> T {
  auto guard = lock_type(this->_mutex);
  return _value;
}

template <class T, class M, template <class> class L>
template <class U, std::enable_if_t<std::is_assignable_v<T, U>, int>>
void Guarded<T, M, L>::assign(U&& value) {
  auto guard = lock_type(_mutex);
  _value = std::forward<U>(value);
}

template <class T, class M, template <class> class L>
auto Guarded<T, M, L>::getLockedGuard() -> MutexGuard<T, L<M>> {
  return MutexGuard(_value, lock_type(_mutex));
}

template <class T, class M, template <class> class L>
auto Guarded<T, M, L>::getLockedGuard() const -> MutexGuard<T const, L<M>> {
  return MutexGuard(_value, lock_type(_mutex));
}

template <class T, class M, template <class> class L>
auto Guarded<T, M, L>::tryLockedGuard() -> std::optional<MutexGuard<T, L<M>>> {
  auto lock = lock_type(_mutex, std::try_to_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

template <class T, class M, template <class> class L>
auto Guarded<T, M, L>::tryLockedGuard() const
    -> std::optional<MutexGuard<T const, L<M>>> {
  auto lock = lock_type(_mutex, std::try_to_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

}  // namespace arangodb
