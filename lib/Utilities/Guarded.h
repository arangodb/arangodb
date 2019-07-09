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
  explicit MutexGuard(T& value, M mutex);
  ~MutexGuard();

  T& get() noexcept;
  T const& get() const noexcept;

 private:
  T& _value;
  M& _mutex;
};

template <class T, class M>
MutexGuard<T, M>::MutexGuard(T& value, M mutex) : _value(value), _mutex(mutex) {
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
  explicit Guarded(T&& value = T(), M&& mutex = M());
  explicit Guarded(T value = T(), M mutex = M());

  // TODO with C++17, these could be noexcept depending on callback being noexcept
  template <class F>
  decltype(F()) doUnderLock(F callback);
  template <class F>
  decltype(F(false)) tryUnderLock(F callback);
  // TODO add more types of "do under lock"?

  MutexGuard<T, M> getLockedGuard();
  // TODO add more types of "get guard" (like try or eventual)

 private:
  T _value;
  M _mutex;
};

template <class T, class M>
Guarded<T, M>::Guarded(T&& value, M&& mutex)
    : _value(std::forward<T>(value)), _mutex(std::forward<T>(mutex)) {}

template <class T, class M>
Guarded<T, M>::Guarded(T value, M mutex)
    : _value(std::move(value)), _mutex(std::move(mutex)) {}

template <class T, class M>
template <class F>
decltype(F()) Guarded<T, M>::doUnderLock(F callback) {
  MUTEX_LOCKER(guard, _mutex);

  return callback();
}

template <class T, class M>
template <class F>
decltype(F(false)) Guarded<T, M>::tryUnderLock(F callback) {
  TRY_MUTEX_LOCKER(guard, _mutex);

  return callback(guard.isLocked());
}
template <class T, class M>
MutexGuard Guarded<T, M>::getLockedGuard() {
  return MutexGuard(_value, _mutex);
}

}  // namespace arangodb

#endif  // LIB_UTILITIES_GUARDED_H
