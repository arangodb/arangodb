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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ReadLocker.h"
#include "WriteLocker.h"

#include "Basics/debugging.h"

#include <atomic>
#include <thread>

namespace arangodb {

template<typename T>
class RecursiveReadLocker {
 public:
  RecursiveReadLocker(T& mutex, std::atomic<std::thread::id>& owner,
                      char const* file, int line)
      : _locker(&mutex, basics::LockerType::TRY, false, file,
                line) {  // does not lock yet
    if (owner.load() != std::this_thread::get_id()) {
      // only try to lock if we don't already have the read-lock
      _locker.lock();
    }
  }

  RecursiveReadLocker(RecursiveReadLocker&& other) = default;
  RecursiveReadLocker(RecursiveReadLocker const&) = delete;
  RecursiveReadLocker& operator=(RecursiveReadLocker const&) = delete;
  RecursiveReadLocker& operator=(RecursiveReadLocker&&) = delete;

  void unlock() {
    if (_locker.isLocked()) {
      _locker.unlock();
    }
  }

 private:
  basics::ReadLocker<T> _locker;
};

template<typename T>
class RecursiveWriteLocker {
 public:
  RecursiveWriteLocker(T& mutex, std::atomic<std::thread::id>& owner,
                       basics::LockerType type, bool acquire, char const* file,
                       int line)
      : _locked(false),
        _locker(&mutex, type, false, file, line),
        _owner(owner),
        _update(noop) {
    if (acquire) {
      lock();
    }
  }

  ~RecursiveWriteLocker() { unlock(); }

  bool isLocked() const noexcept { return _locked; }

  void lock() {
    // recursive locking of the same instance is not yet supported (create a new
    // instance instead)
    TRI_ASSERT(_update != owned);

    if (std::this_thread::get_id() != _owner.load()) {  // not recursive
      _locker.lock();
      _owner.store(std::this_thread::get_id());
      _update = owned;
    }
    _locked = true;
  }

  void unlock() {
    _update(*this);
    _locked = false;
  }

 private:
  bool _locked;  // track locked state separately for recursive lock acquisition
  basics::WriteLocker<T> _locker;
  std::atomic<std::thread::id>& _owner;
  void (*_update)(RecursiveWriteLocker& locker);

  static void noop(RecursiveWriteLocker&) noexcept {}
  static void owned(RecursiveWriteLocker& locker) noexcept {
    locker._owner.store(std::thread::id());
    locker._locker.unlock();
    locker._update = noop;
    locker._locked = false;
  }
};

#define NAME__(name, line) name##line
#define NAME_EXPANDER__(name, line) NAME__(name, line)
#define NAME(name) NAME_EXPANDER__(name, __LINE__)
#define RECURSIVE_READ_LOCKER(lock, owner)                                 \
  arangodb::RecursiveReadLocker<typename std::decay<decltype(lock)>::type> \
      NAME(RecursiveLocker)(lock, owner, __FILE__, __LINE__)
#define RECURSIVE_WRITE_LOCKER_NAMED(name, lock, owner, acquire)            \
  arangodb::RecursiveWriteLocker<typename std::decay<decltype(lock)>::type> \
      name(lock, owner, arangodb::basics::LockerType::BLOCKING, acquire,    \
           __FILE__, __LINE__)
#define RECURSIVE_WRITE_LOCKER(lock, owner) \
  RECURSIVE_WRITE_LOCKER_NAMED(NAME(RecursiveLocker), lock, owner, true)

}  // namespace arangodb
