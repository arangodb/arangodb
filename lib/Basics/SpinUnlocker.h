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

#ifndef ARANGO_SPIN_UNLOCKER_H
#define ARANGO_SPIN_UNLOCKER_H 1

#include <cstdint>

#include "Basics/ReadWriteSpinLock.h"

namespace arangodb::basics {

class SpinUnlocker {
 public:
  enum class Mode : std::uint8_t { Read, Write };

 public:
  SpinUnlocker(Mode mode, ReadWriteSpinLock& lock)
      : _lock(lock), _mode(mode), _locked(true) {
    if (_mode == Mode::Read) {
      _lock.unlockRead();
    } else {
      _lock.unlockWrite();
    }
    _locked = false;
  }

  ~SpinUnlocker() { acquire(); }

  // no move
  SpinUnlocker(SpinUnlocker&&) = delete;
  SpinUnlocker& operator=(SpinUnlocker&&) = delete;

  // no copy
  SpinUnlocker(SpinUnlocker const&) = delete;
  SpinUnlocker& operator=(SpinUnlocker const&) = delete;

  bool isLocked() const { return _locked; }

  void acquire() {
    if (!_locked) {
      if (_mode == Mode::Read) {
        _lock.lockRead();
      } else {
        _lock.lockWrite();
      }
      _locked = true;
    }
  }

 private:
  ReadWriteSpinLock& _lock;
  Mode _mode;
  bool _locked;
};

}  // namespace arangodb::basics

#endif
