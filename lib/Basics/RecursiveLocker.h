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
namespace basics {

/// @brief mutex locker
/// A MutexLocker locks a mutex during its lifetime und unlocks the mutex
/// when it is destroyed.
template<class LockType>
class MutexLocker {
  MutexLocker(MutexLocker const&) = delete;
  MutexLocker& operator=(MutexLocker const&) = delete;

 public:
  /// @brief acquires a mutex
  /// The constructor acquires the mutex, the destructor unlocks the mutex.
  MutexLocker(LockType* mutex, LockerType type, bool condition,
              char const* file, int line) noexcept
      : _mutex(mutex), _file(file), _line(line), _isLocked(false) {
    if (condition) {
      if (type == LockerType::BLOCKING) {
        lock();
        TRI_ASSERT(_isLocked);
      } else if (type == LockerType::EVENTUAL) {
        lockEventual();
        TRI_ASSERT(_isLocked);
      } else if (type == LockerType::TRY) {
        _isLocked = tryLock();
      }
    }
  }

  /// @brief releases the read-lock
  ~MutexLocker() {
    if (_isLocked) {
      _mutex->unlock();
    }
  }

  bool isLocked() const noexcept { return _isLocked; }

  /// @brief eventually acquire the read lock
  void lockEventual() {
    while (!tryLock()) {
      std::this_thread::yield();
    }
    TRI_ASSERT(_isLocked);
  }

  bool tryLock() {
    TRI_ASSERT(!_isLocked);
    if (_mutex->try_lock()) {
      _isLocked = true;
    }
    return _isLocked;
  }

  /// @brief acquire the mutex, blocking
  void lock() noexcept {
    TRI_ASSERT(!_isLocked);
    _mutex->lock();
    _isLocked = true;
  }

  /// @brief unlocks the mutex if we own it
  bool unlock() noexcept {
    if (_isLocked) {
      _isLocked = false;
      _mutex->unlock();
      return true;
    }
    return false;
  }

  /// @brief steals the lock, but does not unlock it
  bool steal() noexcept {
    if (_isLocked) {
      _isLocked = false;
      return true;
    }
    return false;
  }

 private:
  /// @brief the mutex
  LockType* _mutex;

  /// @brief file
  char const* _file;

  /// @brief line number
  int _line;

  /// @brief whether or not the mutex is locked
  bool _isLocked;
};

}  // namespace basics

// identical code to RecursiveWriteLocker except for type
template<typename T>
class RecursiveMutexLocker {
 public:
  RecursiveMutexLocker(T& mutex, std::atomic<std::thread::id>& owner,
                       basics::LockerType type, bool acquire, char const* file,
                       int line)
      : _locked(false),
        _locker(&mutex, type, false, file, line),  // does not lock yet
        _owner(owner),
        _update(noop) {
    if (acquire) {
      lock();
    }
  }

  RecursiveMutexLocker(RecursiveMutexLocker const& other) = delete;
  RecursiveMutexLocker& operator=(RecursiveMutexLocker const& other) = delete;
  RecursiveMutexLocker(RecursiveMutexLocker&& other) = delete;
  RecursiveMutexLocker& operator=(RecursiveMutexLocker&& other) = delete;

  ~RecursiveMutexLocker() { unlock(); }

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

  void unlock() { _update(*this); }

 private:
  bool _locked;  // track locked state separately for recursive lock acquisition
  basics::MutexLocker<T> _locker;
  std::atomic<std::thread::id>& _owner;
  void (*_update)(RecursiveMutexLocker& locker);

  static void noop(RecursiveMutexLocker&) noexcept {}
  static void owned(RecursiveMutexLocker& locker) noexcept {
    locker._owner.store(std::thread::id());
    locker._locker.unlock();
    locker._update = noop;
    locker._locked = false;
  }
};

#define NAME__(name, line) name##line
#define NAME_EXPANDER__(name, line) NAME__(name, line)
#define NAME(name) NAME_EXPANDER__(name, __LINE__)
#define RECURSIVE_MUTEX_LOCKER_NAMED(name, lock, owner, acquire)            \
  arangodb::RecursiveMutexLocker<typename std::decay<decltype(lock)>::type> \
      name(lock, owner, arangodb::basics::LockerType::BLOCKING, acquire,    \
           __FILE__, __LINE__)
#define RECURSIVE_MUTEX_LOCKER(lock, owner) \
  RECURSIVE_MUTEX_LOCKER_NAMED(NAME(RecursiveLocker), lock, owner, true)

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

// identical code to RecursiveMutexLocker except for type
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
