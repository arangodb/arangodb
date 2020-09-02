////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef LIB_BASICS_RECURSIVE_LOCKER_H
#define LIB_BASICS_RECURSIVE_LOCKER_H 1

#include "MutexLocker.h"
#include "ReadLocker.h"
#include "WriteLocker.h"

#include "Basics/debugging.h"

namespace arangodb {

// identical code to RecursiveWriteLocker except for type
template <typename T>
class RecursiveMutexLocker {
 public:
  RecursiveMutexLocker( // recursive locker
      T& mutex, // mutex
      std::atomic<std::thread::id>& owner, // owner
      arangodb::basics::LockerType type, // locker type
      bool acquire, // acquire flag
      char const* file, // file
      int line // line
): _locker(&mutex, type, false, file, line), _owner(owner), _update(noop) {
    if (acquire) {
      lock();
    }
  }

  ~RecursiveMutexLocker() { unlock(); }

  bool isLocked() { return _locker.isLocked(); }

  void lock() {
    // recursive locking of the same instance is not yet supported (create a new instance instead)
    TRI_ASSERT(_update != owned);

    if (std::this_thread::get_id() != _owner.load()) {  // not recursive
      _locker.lock();
      _owner.store(std::this_thread::get_id());
      _update = owned;
    }
  }

  void unlock() { _update(*this); }

 private:
  arangodb::basics::MutexLocker<T> _locker;
  std::atomic<std::thread::id>& _owner;
  void (*_update)(RecursiveMutexLocker& locker);

  static void noop(RecursiveMutexLocker&) {}
  static void owned(RecursiveMutexLocker& locker) {
    static std::thread::id unowned;
    locker._owner.store(unowned);
    locker._locker.unlock();
    locker._update = noop;
  }
};

#define NAME__(name, line) name##line
#define NAME_EXPANDER__(name, line) NAME__(name, line)
#define NAME(name) NAME_EXPANDER__(name, __LINE__)
#define RECURSIVE_MUTEX_LOCKER_NAMED(name, lock, owner, acquire)                     \
  RecursiveMutexLocker<typename std::decay<decltype(lock)>::type> name(              \
    lock, owner, arangodb::basics::LockerType::BLOCKING, acquire, __FILE__, __LINE__ \
  )
#define RECURSIVE_MUTEX_LOCKER(lock, owner) \
  RECURSIVE_MUTEX_LOCKER_NAMED(NAME(RecursiveLocker), lock, owner, true)

template <typename T>
class RecursiveReadLocker {
 public:
  RecursiveReadLocker( // recursive locker
      T& mutex, // mutex
      std::atomic<std::thread::id>& owner, // owner
      char const* file, // file
      int line // line
): _locker(&mutex, arangodb::basics::LockerType::TRY, true, file, line) {
    if (!_locker.isLocked() && owner.load() != std::this_thread::get_id()) {
      _locker.lock();
    }
  }

 private:
  arangodb::basics::ReadLocker<T> _locker;
};

// identical code to RecursiveMutexLocker except for type
template <typename T>
class RecursiveWriteLocker {
 public:
  RecursiveWriteLocker( // recursive locker
      T& mutex, // mutex
      std::atomic<std::thread::id>& owner, // owner
      arangodb::basics::LockerType type, // locker type
      bool acquire, // acquire flag
      char const* file, // file
      int line // line
): _locked(false), // locked
   _locker(&mutex, type, false, file, line), // locker
   _owner(owner), // owner
   _update(noop) {
    if (acquire) {
      lock();
    }
  }

  ~RecursiveWriteLocker() { unlock(); }

  bool isLocked() { return _locked; }

  void lock() {
    // recursive locking of the same instance is not yet supported (create a new instance instead)
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
  arangodb::basics::WriteLocker<T> _locker;
  std::atomic<std::thread::id>& _owner;
  void (*_update)(RecursiveWriteLocker& locker);

  static void noop(RecursiveWriteLocker&) {}
  static void owned(RecursiveWriteLocker& locker) {
    static std::thread::id unowned;
    locker._owner.store(unowned);
    locker._locker.unlock();
    locker._update = noop;
  }
};

#define NAME__(name, line) name##line
#define NAME_EXPANDER__(name, line) NAME__(name, line)
#define NAME(name) NAME_EXPANDER__(name, __LINE__)
#define RECURSIVE_READ_LOCKER(lock, owner)                                             \
  RecursiveReadLocker<typename std::decay<decltype(lock)>::type> NAME(RecursiveLocker)(\
    lock, owner, __FILE__, __LINE__                                                    \
  )
#define RECURSIVE_WRITE_LOCKER_NAMED(name, lock, owner, acquire)                       \
  RecursiveWriteLocker<typename std::decay<decltype(lock)>::type> name(                \
      lock, owner, arangodb::basics::LockerType::BLOCKING, acquire, __FILE__, __LINE__ \
  )
#define RECURSIVE_WRITE_LOCKER(lock, owner) \
  RECURSIVE_WRITE_LOCKER_NAMED(NAME(RecursiveLocker), lock, owner, true)

} // arangodb

#endif
