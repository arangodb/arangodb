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

#ifndef LIB_UTILITIES_GUARDED_H
#define LIB_UTILITIES_GUARDED_H

#include <algorithm>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <variant>

namespace arangodb {

class Mutex;

template <class T, class L>
class MutexGuard {
 public:
  explicit MutexGuard(T& value, L mutexLock);
  ~MutexGuard() = default;

  auto get() noexcept -> T&;
  auto get() const noexcept -> T const&;

  auto operator->() noexcept -> T*;
  auto operator->() const noexcept -> T const*;

 private:
  T& _value;
  L _mutexLock;
};

template <class T, class L>
MutexGuard<T, L>::MutexGuard(T& value, L mutexLock)
    : _value(value), _mutexLock(std::move(mutexLock)) {
  if (ADB_UNLIKELY(!_mutexLock.owns_lock())) {
    throw std::invalid_argument("Lock not owned");
  }
}

template <class T, class L>
auto MutexGuard<T, L>::get() noexcept -> T& {
  return _value;
}

template <class T, class L>
auto MutexGuard<T, L>::get() const noexcept -> T const& {
  return _value;
}

template <class T, class L>
auto MutexGuard<T, L>::operator->() noexcept -> T* {
  return std::addressof(get());
}

template <class T, class L>
auto MutexGuard<T, L>::operator->() const noexcept -> T const* {
  return std::addressof(get());
}

template <class T, class M = Mutex>
class Guarded {
 public:
  explicit Guarded(T&& value = T());
  template <typename... Args>
  explicit Guarded(Args...);

  ~Guarded() = default;
  Guarded(Guarded const&) = delete;
  Guarded(Guarded&&) = delete;
  auto operator=(Guarded const&) -> Guarded& = delete;
  auto operator=(Guarded&&) -> Guarded& = delete;

  template <class F, class R = std::invoke_result_t<F, T&>>
  auto doUnderLock(F&& callback) -> R;
  template <class F, class R = std::invoke_result_t<F, T&>>
  auto doUnderLock(F&& callback) const -> R;

  template <class F, class R = std::invoke_result_t<F, T&>,
            class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) -> std::optional<Q>;
  template <class F, class R = std::invoke_result_t<F, T&>,
            class Q = std::conditional_t<std::is_void_v<R>, std::monostate, R>>
  [[nodiscard]] auto tryUnderLock(F&& callback) const -> std::optional<Q>;

  using L = std::unique_lock<M>;
  auto getLockedGuard() -> MutexGuard<T, L>;
  auto getLockedGuard() const -> MutexGuard<T const, L>;

  auto tryLockedGuard() -> std::optional<MutexGuard<T, L>>;
  auto tryLockedGuard() const -> std::optional<MutexGuard<T const, L>>;
  // TODO add more types of "get guard" (like try or eventual)

 private:
  T _value;
  M _mutex;
};

template <class T, class M>
Guarded<T, M>::Guarded(T&& value) : _value{std::move(value)}, _mutex{} {}

template <class T, class M>
template <typename... Args>
Guarded<T, M>::Guarded(Args... args) : _value{args...}, _mutex{} {}

template <class T, class M>
template <class F, class R>
auto Guarded<T, M>::doUnderLock(F&& callback) -> R {
  auto guard = std::unique_lock<M>(_mutex);

  if constexpr (!std::is_void_v<R>) {
    return std::forward<F>(callback)(_value);
  } else {
    std::forward<F>(callback)(_value);
    return;
  }
}

template <class T, class M>
template <class F, class R>
auto Guarded<T, M>::doUnderLock(F&& callback) const -> R {
  auto guard = std::unique_lock<M>(_mutex);

  if constexpr (!std::is_void_v<R>) {
    return std::forward<F>(callback)(_value);
  } else {
    std::forward<F>(callback)(_value);
    return;
  }
}

template <class T, class M>
template <class F, class R, class Q>
auto Guarded<T, M>::tryUnderLock(F&& callback) -> std::optional<Q> {
  auto guard = std::unique_lock<M>(_mutex, std::try_to_lock);

  if (guard.owns_lock()) {
    if constexpr (!std::is_void_v<R>) {
      return std::forward<F>(callback)(_value);
    } else {
      std::forward<F>(callback)(_value);
      return std::monostate{};
    }
  } else {
    return std::nullopt;
  }
}

template <class T, class M>
template <class F, class R, class Q>
auto Guarded<T, M>::tryUnderLock(F&& callback) const -> std::optional<Q> {
  auto guard = std::unique_lock<M>(_mutex, std::try_to_lock);

  if (guard.owns_lock()) {
    if constexpr (!std::is_void_v<R>) {
      return std::forward<F>(callback)(_value);
    } else {
      std::forward<F>(callback)(_value);
      return std::monostate{};
    }
  } else {
    return std::nullopt;
  }
}

template <class T, class M>
auto Guarded<T, M>::getLockedGuard() -> MutexGuard<T, L> {
  return MutexGuard(_value, L(_mutex));
}

template <class T, class M>
auto Guarded<T, M>::getLockedGuard() const -> MutexGuard<T const, L> {
  return MutexGuard(_value, L(_mutex));
}

template <class T, class M>
auto Guarded<T, M>::tryLockedGuard() -> std::optional<MutexGuard<T, L>> {
  auto lock = L(_mutex, std::try_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

template <class T, class M>
auto Guarded<T, M>::tryLockedGuard() const -> std::optional<MutexGuard<const T, L>> {
  auto lock = L(_mutex, std::try_lock);

  if (lock.owns_lock()) {
    return MutexGuard(_value, std::move(lock));
  } else {
    return std::nullopt;
  }
}

}  // namespace arangodb

#endif  // LIB_UTILITIES_GUARDED_H
