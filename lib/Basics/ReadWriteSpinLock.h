////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
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

class ReadWriteSpinLock {
 public:
  ReadWriteSpinLock() : _state(0) {}

  // only needed for cache::Metadata
  ReadWriteSpinLock(ReadWriteSpinLock&& other) {
    auto val = other._state.load(std::memory_order_relaxed);
    TRI_ASSERT(val == 0);
    _state.store(val, std::memory_order_relaxed);
  }
  ReadWriteSpinLock& operator=(ReadWriteSpinLock&& other) {
    auto val = other._state.load(std::memory_order_relaxed);
    TRI_ASSERT(val == 0);
    val = _state.exchange(val, std::memory_order_relaxed);
    TRI_ASSERT(val == 0);
    return *this;
  }

  bool tryWriteLock() {
    // order_relaxed is an optimization, cmpxchg will synchronize side-effects
    auto state = _state.load(std::memory_order_relaxed);
    // try to acquire write lock as long as no readers or writers are active,
    // we might "overtake" other queued writers though.
    while ((state & ~QUEUED_WRITER_MASK) == 0) {
      if (_state.compare_exchange_weak(state, state | WRITE_LOCK, std::memory_order_acquire)) {
        return true;  // we successfully acquired the write lock!
      }
    }
    return false;
  }

  bool writeLock(uint64_t maxAttempts = UINT64_MAX) {
    if (tryWriteLock()) {
      return true;
    }

    uint64_t attempts = 0;

    // the lock is either hold by another writer or we have active readers
    // -> announce that we want to write
    auto state = _state.fetch_add(QUEUED_WRITER_INC, std::memory_order_relaxed);
    while (++attempts < maxAttempts) {
      while ((state & ~QUEUED_WRITER_MASK) == 0) {
        // try to acquire lock and perform queued writer decrement in one step
        if (_state.compare_exchange_weak(state, (state - QUEUED_WRITER_INC) | WRITE_LOCK,
                                         std::memory_order_acquire)) {
          return true;
        }
        if (++attempts > maxAttempts) {
          return false;
        }
      }
      cpu_relax();
      state = _state.load(std::memory_order_relaxed);
    }
    return false;
  }

  bool tryReadLock() {
    // order_relaxed is an optimization, cmpxchg will synchronize side-effects
    auto state = _state.load(std::memory_order_relaxed);
    // try to acquire read lock as long as no writers are active or queued
    while ((state & ~READER_MASK) == 0) {
      if (_state.compare_exchange_weak(state, state + READER_INC, std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  bool readLock(uint64_t maxAttempts = UINT64_MAX) {
    uint64_t attempts = 0;
    while (attempts++ < maxAttempts) {
      if (tryReadLock()) {
        return true;
      }
      cpu_relax();
    }
    return false;
  }

  void readUnlock() { unlockRead(); }
  void unlockRead() {
    TRI_ASSERT(isReadLocked());
    _state.fetch_sub(READER_INC, std::memory_order_release);
  }

  void writeUnlock() { unlockWrite(); }
  void unlockWrite() {
    TRI_ASSERT(isWriteLocked());
    _state.fetch_sub(WRITE_LOCK, std::memory_order_release);
  }

  bool isLocked() const {
    return (_state.load(std::memory_order_relaxed) & ~QUEUED_WRITER_MASK) != 0;
  }
  bool isReadLocked() const {
    return (_state.load(std::memory_order_relaxed) & READER_MASK) > 0;
  }
  bool isWriteLocked() const {
    return _state.load(std::memory_order_relaxed) & WRITE_LOCK;
  }

 private:
  /// @brief _state, lowest bit is write_lock, the next 15 bits is the number of
  /// queued writers, the last 16 bits the number of active readers.
  std::atomic<uint32_t> _state;

  static constexpr uint32_t WRITE_LOCK = 1;

  static constexpr uint32_t READER_INC = 1 << 16;
  static constexpr uint32_t READER_MASK = ~(READER_INC - 1);

  static constexpr uint32_t QUEUED_WRITER_INC = 1 << 1;
  static constexpr uint32_t QUEUED_WRITER_MASK = (READER_INC - 1) & ~WRITE_LOCK;

  static_assert((READER_MASK & WRITE_LOCK) == 0,
                "READER_MASK and WRITE_LOCK conflict");
  static_assert((READER_MASK & QUEUED_WRITER_MASK) == 0,
                "READER_MASK and QUEUED_WRITER_MASK conflict");
  static_assert((QUEUED_WRITER_MASK & WRITE_LOCK) == 0,
                "QUEUED_WRITER_MASK and WRITE_LOCK conflict");

  static_assert((READER_MASK & READER_INC) != 0 && (READER_MASK & (READER_INC >> 1)) == 0,
                "READER_INC must be first bit in READER_MASK");
  static_assert((QUEUED_WRITER_MASK & QUEUED_WRITER_INC) != 0 &&
                    (QUEUED_WRITER_MASK & (QUEUED_WRITER_INC >> 1)) == 0,
                "QUEUED_WRITER_INC must be first bit in QUEUED_WRITER_MASK");
};
}  // namespace basics
}  // namespace arangodb

#endif
