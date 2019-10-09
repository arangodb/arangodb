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

#include "Basics/MutexLocker.h"

#include <algorithm>
#include <functional>

namespace arangodb {

class Mutex;

template <class T, class M = Mutex>
class MutexGuard {
 public:
  explicit MutexGuard(T& value, M& mutex);
  ~MutexGuard();

  T& get() noexcept;
  T const& get() const noexcept;

 private:
  T& _value;
  M& _mutex;
};

template <class T, class M>
MutexGuard<T, M>::MutexGuard(T& value, M& mutex) : _value(value), _mutex(mutex) {
  _mutex.lock();
}

template <class T, class M>
MutexGuard<T, M>::~MutexGuard() {
  _mutex.unlock();
}

template <class T, class M>
T& MutexGuard<T, M>::get() noexcept {
  return _value;
}

template <class T, class M>
T const& MutexGuard<T, M>::get() const noexcept {
  return _value;
}

template <class T, class M = Mutex>
class Guarded {
 public:
  explicit Guarded(T&& value = T());
  template <typename... Args>
  explicit Guarded(Args...);

  // TODO with C++17, these could be noexcept depending on callback being noexcept
  template <class F>
  std::result_of_t<F(T&)> doUnderLock(F callback);
  template <class F>
  std::result_of_t<F(T const&)> doUnderLock(F callback);

  // template <class F>
  // decltype(F(false)) tryUnderLock(F callback);
  // TODO add more types of "do under lock"?

  MutexGuard<T, M> getLockedGuard();
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
template <class F>
std::result_of_t<F(T&)> Guarded<T, M>::doUnderLock(F callback) {
  MUTEX_LOCKER(guard, _mutex);

  return callback(_value);
}

template <class T, class M>
template <class F>
std::result_of_t<F(T const&)> Guarded<T, M>::doUnderLock(F callback) {
  MUTEX_LOCKER(guard, _mutex);

  return callback(_value);
}

// template <class T, class M>
// template <class F>
// decltype(F(false)) Guarded<T, M>::tryUnderLock(F callback) {
//   TRY_MUTEX_LOCKER(guard, _mutex);
//
//   return callback(guard.isLocked());
// }

template <class T, class M>
MutexGuard<T, M> Guarded<T, M>::getLockedGuard() {
  return MutexGuard<T, M>(_value, _mutex);
}

}  // namespace arangodb

#endif  // LIB_UTILITIES_GUARDED_H
