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

using namespace arangodb::basics;

/// @brief locks for writing
void ReadWriteLockCPP11::writeLock() {
  std::unique_lock<std::mutex> guard(_mut);
  if (_state == 0) {
    _state = -1;
    return;
  }
  do {
    _wantWrite = true;
    _bell.wait(guard);
  } while (_state != 0);
  _state = -1;
  _wantWrite = false;
}

/// @brief locks for writing, but only tries
bool ReadWriteLockCPP11::tryWriteLock() {
  std::unique_lock<std::mutex> guard(_mut);
  if (_state == 0) {
    _state = -1;
    return true;
  }
  return false;
}

/// @brief locks for reading
void ReadWriteLockCPP11::readLock() {
  std::unique_lock<std::mutex> guard(_mut);
  if (!_wantWrite && _state >= 0) {
    _state += 1;
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
  _state += 1;
}

/// @brief locks for reading, tries only
bool ReadWriteLockCPP11::tryReadLock() {
  std::unique_lock<std::mutex> guard(_mut);
  if (!_wantWrite && _state >= 0) {
    _state += 1;
    return true;
  }
  return false;
}

/// @brief releases the read-lock or write-lock
void ReadWriteLockCPP11::unlock() {
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
}

/// @brief releases the read-lock
void ReadWriteLockCPP11::unlockRead() { unlock(); }

/// @brief releases the write-lock
void ReadWriteLockCPP11::unlockWrite() { unlock(); }
