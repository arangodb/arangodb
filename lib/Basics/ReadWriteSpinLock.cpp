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
/// @author Daniel H. Larkin
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ReadWriteSpinLock.h"

#include "Basics/cpu-relax.h"
#include "Basics/debugging.h"

using namespace arangodb;
using namespace arangodb::basics;

ReadWriteSpinLock::ReadWriteSpinLock(ReadWriteSpinLock&& other) noexcept {
  auto val = other._state.load(std::memory_order_relaxed);
  TRI_ASSERT(val == 0);
  _state.store(val, std::memory_order_relaxed);
}

ReadWriteSpinLock& ReadWriteSpinLock::operator=(ReadWriteSpinLock&& other) noexcept {
  auto val = other._state.load(std::memory_order_relaxed);
  TRI_ASSERT(val == 0);
  val = _state.exchange(val, std::memory_order_relaxed);
  TRI_ASSERT(val == 0);
  return *this;
}

bool ReadWriteSpinLock::tryWriteLock() noexcept {
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

void ReadWriteSpinLock::writeLock() noexcept {
  if (tryWriteLock()) {
    return;
  }

  // the lock is either hold by another writer or we have active readers
  // -> announce that we want to write
  auto state = _state.fetch_add(QUEUED_WRITER_INC, std::memory_order_relaxed);
  for (;;) {
    while ((state & ~QUEUED_WRITER_MASK) == 0) {
      // try to acquire lock and perform queued writer decrement in one step
      if (_state.compare_exchange_weak(state, (state - QUEUED_WRITER_INC) | WRITE_LOCK,
                                       std::memory_order_acquire)) {
        return;
      }
    }
    cpu_relax();
    state = _state.load(std::memory_order_relaxed);
  }
}

bool ReadWriteSpinLock::writeLock(uint64_t maxAttempts) noexcept {
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

bool ReadWriteSpinLock::tryReadLock() noexcept {
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

void ReadWriteSpinLock::readLock() noexcept {
  for (;;) {
    if (tryReadLock()) {
      return;
    }
    cpu_relax();
  }
}

bool ReadWriteSpinLock::readLock(uint64_t maxAttempts) noexcept {
  uint64_t attempts = 0;
  while (attempts++ < maxAttempts) {
    if (tryReadLock()) {
      return true;
    }
    cpu_relax();
  }
  return false;
}

void ReadWriteSpinLock::readUnlock() noexcept { unlockRead(); }
void ReadWriteSpinLock::unlockRead() noexcept {
  TRI_ASSERT(isReadLocked());
  _state.fetch_sub(READER_INC, std::memory_order_release);
}

void ReadWriteSpinLock::writeUnlock() noexcept { unlockWrite(); }
void ReadWriteSpinLock::unlockWrite() noexcept {
  TRI_ASSERT(isWriteLocked());
  _state.fetch_sub(WRITE_LOCK, std::memory_order_release);
}

bool ReadWriteSpinLock::isLocked() const noexcept {
  return (_state.load(std::memory_order_relaxed) & ~QUEUED_WRITER_MASK) != 0;
}

bool ReadWriteSpinLock::isReadLocked() const noexcept {
  return (_state.load(std::memory_order_relaxed) & READER_MASK) > 0;
}

bool ReadWriteSpinLock::isWriteLocked() const noexcept {
  return _state.load(std::memory_order_relaxed) & WRITE_LOCK;
}
