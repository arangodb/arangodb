////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ReadWriteLockCPP11.h"

#ifdef TRI_TRACE_LOCKS

#include <iostream>

static thread_local std::unordered_map<ReadWriteLockCPP11*, int> _threadLocks;

static void LockError(ReadWriteLockCPP11* lock, int mode) {
  auto it = _threadLocks.find(lock);
  auto m = (*it).second;
  std::cout << "ERROR. TRYING TO ACQUIRE " << (mode == 1 ? "READ" : "WRITE")
            << " LOCK WHILE ALREADY HOLDING IT IN "
            << (m == 1 ? "READ" : "WRITE") << " MODE" << std::endl;
  TRI_ASSERT(false);
}

#endif

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLockCPP11::writeLock() {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(this) != _threadLocks.end()) {
    LockError(this, 0);
  }
#endif

  std::unique_lock<std::mutex> guard(_mut);
  if (_state == 0) {
    _state = -1;
#ifdef TRI_TRACE_LOCKS
    _threadLocks.emplace(this, 0);
#endif
    return;
  }
  do {
    _wantWrite = true;
    _bell.wait(guard);
  } while (_state != 0);
  _state = -1;
  _wantWrite = false;

#ifdef TRI_TRACE_LOCKS
  _threadLocks.emplace(this, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing, but only tries
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLockCPP11::tryWriteLock() {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(this) != _threadLocks.end()) {
    LockError(this, 0);
  }
#endif

  std::unique_lock<std::mutex> guard(_mut);
  if (_state == 0) {
    _state = -1;
#ifdef TRI_TRACE_LOCKS
    _threadLocks.emplace(this, 0);
#endif
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLockCPP11::readLock() {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(this) != _threadLocks.end()) {
    LockError(this, 1);
  }
#endif
  std::unique_lock<std::mutex> guard(_mut);
  if (!_wantWrite && _state >= 0) {
    _state += 1;
#ifdef TRI_TRACE_LOCKS
    _threadLocks.emplace(this, 1);
#endif
    return;
  }
  while (true) {
    while (_wantWrite || _state < 0) {
      _bell.wait(guard);
    }
    if (!_wantWrite) {
      break;
    }
  }
#ifdef TRI_TRACE_LOCKS
  _threadLocks.emplace(this, 1);
#endif
  _state += 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading, tries only
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLockCPP11::tryReadLock() {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(this) != _threadLocks.end()) {
    LockError(this, 0);
  }
#endif
  std::unique_lock<std::mutex> guard(_mut);
  if (!_wantWrite && _state >= 0) {
    _state += 1;
#ifdef TRI_TRACE_LOCKS
    _threadLocks.emplace(this, 1);
#endif
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock or write-lock
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLockCPP11::unlock() {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(this) == _threadLocks.end()) {
    LockError(this, 0);
  }
#endif
  std::unique_lock<std::mutex> guard(_mut);
  if (_state == -1) {
    _state = 0;
    _bell.notify_all();
  } else {
    _state -= 1;
    if (_state == 0) {
      _bell.notify_all();
    }
  }
#ifdef TRI_TRACE_LOCKS
  _threadLocks.erase(this);
#endif
}
