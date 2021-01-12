////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_SPIN_LOCKER_H
#define ARANGO_SPIN_LOCKER_H 1

#include <cstdint>

#include "Basics/ReadWriteSpinLock.h"

namespace arangodb::basics {

class SpinLocker {
 public:
  enum class Mode : std::uint8_t { Read, Write };
  enum class Effort { Try, Succeed };

 public:
  SpinLocker(Mode mode, ReadWriteSpinLock& lock, bool doLock = true,
             Effort effort = Effort::Succeed)
      : _lock(lock), _mode(mode), _locked(false) {
    if (doLock) {      
      if (effort == Effort::Succeed) {
        if (_mode == Mode::Read) {
          _lock.lockRead();
        } else {
          _lock.lockWrite();
        }
        _locked = true;
      } else {
        if (_mode == Mode::Read) {
          _locked = _lock.tryLockRead();
        } else {
          _locked = _lock.tryLockWrite();
        }
      }
    }
  }

  SpinLocker(Mode mode, ReadWriteSpinLock& lock, std::size_t maxAttempts)
      : _lock(lock), _mode(mode), _locked(false) {
    if (_mode == Mode::Read) {
      _locked = _lock.lockRead(maxAttempts);
    } else {
      _locked = _lock.lockWrite(maxAttempts);
    }
  }

  ~SpinLocker() {
    release();
  }

  SpinLocker(SpinLocker&& other)
    : _lock(other._lock), _mode(other._mode), _locked(other._locked) {
    other._locked = false;
  }

  // no move assignment (no reference assignment)
  SpinLocker& operator=(SpinLocker&& other) = delete;

  // no copy
  SpinLocker(SpinLocker const&) = delete;
  SpinLocker& operator=(SpinLocker const&) = delete;

  bool isLocked() const { return _locked; }

  void release() {
    if (_locked) {
      if (_mode == Mode::Read) {
        _lock.unlockRead();
      } else {
        _lock.unlockWrite();
      }
      _locked = false;
    }
  }

 private:
  ReadWriteSpinLock& _lock;
  Mode _mode;
  bool _locked;
};

}  // namespace arangodb::basics

#endif
