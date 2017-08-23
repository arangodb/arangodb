////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_READ_WRITE_SPIN_LOCK_H
#define ARANGO_READ_WRITE_SPIN_LOCK_H 1

#include "Basics/Common.h"
#include "Basics/SharedAtomic.h"
#include "Basics/SharedCounter.h"
#include "Basics/cpu-relax.h"

#include <atomic>

namespace arangodb {
namespace basics {

template <uint64_t stripes = 64>
struct ReadWriteSpinLock {
  typedef std::function<uint64_t()> IdFunc;

  ReadWriteSpinLock() : _writer(false), _readers(SharedCounter<stripes>::DefaultIdFunc) {}
  ReadWriteSpinLock(IdFunc f) : _writer(false), _readers(f) {}
  ReadWriteSpinLock(ReadWriteSpinLock const& other) {
    if (this != &other) {
      _writer = other._writer;
      _readers = other._readers;
    }
  }

  bool writeLock(uint64_t maxTries = UINT64_MAX) {
    uint64_t attempts = 0;

    while (attempts < maxTries) {
      if (!_writer.load(std::memory_order_relaxed)) {
        // attempt to get read lock
        bool expected = false;
        bool success = _writer.compare_exchange_weak(expected, true,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed);

        if (success) {
          // write lock acquired, wait for readers to finish
          while (attempts++ < maxTries && _readers.nonZero(std::memory_order_acquire)) {
            cpu_relax();
          }
          if (attempts >= maxTries) {
            // timed out waiting for readers, release write lock
            _writer.store(false, std::memory_order_release);
            return false;
          }
          // locked!
          return true;
        }
      }

      attempts++;
      cpu_relax();
    } // too many attempts

    return false;
  }

  void writeUnlock() { _writer.store(false, std::memory_order_release); }

  bool readLock(uint64_t maxTries = UINT64_MAX) {
    uint64_t attempts = 0;

    while (attempts++ < maxTries) {
      if (!_writer.load(std::memory_order_relaxed)) {
        _readers.add(1, std::memory_order_acq_rel); // read locked

        // double check writer hasn't stepped in
        if (_writer.load(std::memory_order_acquire)) {
          // writer got the lock, go back to waiting
          _readers.sub(1, std::memory_order_release);
        } else {
          // locked!
          return true;
        }
      }

      cpu_relax();
    } // too many attempts

    return false;
  }

  void readUnlock() { _readers.sub(1, std::memory_order_release); }

  bool isLocked() const {
    return (_readers.nonZero() || _writer.load());
  }

  bool isWriteLocked() const { return _writer.load(); }

 private:
  SharedAtomic<bool> _writer;
  SharedCounter<stripes, true> _readers;
};

}  // namespace basics
}  // namespace arangodb

#endif
