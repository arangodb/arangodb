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
/// @author Dan Larkin-York
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ReadWriteSpinLock.h"

#include "Basics/cpu-relax.h"
#include "Basics/debugging.h"

namespace {
static constexpr std::uint32_t WriteLock{1};

static constexpr std::uint32_t ReaderIncrement{static_cast<std::uint32_t>(1) << 16};
static constexpr std::uint32_t ReaderMask{~(::ReaderIncrement - 1)};

static constexpr std::uint32_t QueuedWriterIncrement{static_cast<std::uint32_t>(1) << 1};
static constexpr std::uint32_t QueuedWriterMask{(::ReaderIncrement - 1) & ~::WriteLock};

static_assert((::ReaderMask & ::WriteLock) == 0,
              "::ReaderMask and ::WriteLock conflict");
static_assert((::ReaderMask & ::QueuedWriterMask) == 0,
              "::ReaderMask and ::QueuedWriterMask conflict");
static_assert((::QueuedWriterMask & ::WriteLock) == 0,
              "::QueuedWriterMask and ::WriteLock conflict");

static_assert((::ReaderMask & ::ReaderIncrement) != 0 && (::ReaderMask & (::ReaderIncrement >> 1)) == 0,
              "::ReaderIncrement must be first bit in ::ReaderMask");
static_assert((::QueuedWriterMask & ::QueuedWriterIncrement) != 0 &&
                  (::QueuedWriterMask & (::QueuedWriterIncrement >> 1)) == 0,
              "::QueuedWriterIncrement must be first bit in ::QueuedWriterMask");
}

namespace arangodb::basics {

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

bool ReadWriteSpinLock::tryLockWrite() noexcept {
  // order_relaxed is an optimization, cmpxchg will synchronize side-effects
  auto state = _state.load(std::memory_order_relaxed);
  // try to acquire write lock as long as no readers or writers are active,
  // we might "overtake" other queued writers though.
  while ((state & ~::QueuedWriterMask) == 0) {
    if (_state.compare_exchange_weak(state, state | ::WriteLock, std::memory_order_acquire)) {
      return true;  // we successfully acquired the write lock!
    }
  }
  return false;
}

void ReadWriteSpinLock::lockWrite() noexcept {
  if (tryLockWrite()) {
    return;
  }

  // the lock is either hold by another writer or we have active readers
  // -> announce that we want to write
  auto state = _state.fetch_add(::QueuedWriterIncrement, std::memory_order_relaxed);
  for (;;) {
    while ((state & ~::QueuedWriterMask) == 0) {
      // try to acquire lock and perform queued writer decrement in one step
      if (_state.compare_exchange_weak(state, (state - ::QueuedWriterIncrement) | ::WriteLock,
                                       std::memory_order_acquire)) {
        return;
      }
    }
    cpu_relax();
    state = _state.load(std::memory_order_relaxed);
  }
}

bool ReadWriteSpinLock::lockWrite(std::size_t maxAttempts) noexcept {
  if (tryLockWrite()) {
    return true;
  }

  uint64_t attempts = 0;

  // the lock is either hold by another writer or we have active readers
  // -> announce that we want to write
  auto state = _state.fetch_add(::QueuedWriterIncrement, std::memory_order_relaxed);
  while (++attempts < maxAttempts) {
    while ((state & ~::QueuedWriterMask) == 0) {
      // try to acquire lock and perform queued writer decrement in one step
      if (_state.compare_exchange_weak(state, (state - ::QueuedWriterIncrement) | ::WriteLock,
                                       std::memory_order_acquire)) {
        return true;
      }
      if (++attempts > maxAttempts) {
        // Undo the counting of us as queued writer:
        _state.fetch_sub(::QueuedWriterIncrement, std::memory_order_release);
        return false;
      }
    }
    cpu_relax();
    state = _state.load(std::memory_order_relaxed);
  }
        
  // Undo the counting of us as queued writer:
  _state.fetch_sub(::QueuedWriterIncrement, std::memory_order_release);
  return false;
}

bool ReadWriteSpinLock::tryLockRead() noexcept {
  // order_relaxed is an optimization, cmpxchg will synchronize side-effects
  auto state = _state.load(std::memory_order_relaxed);
  // try to acquire read lock as long as no writers are active or queued
  while ((state & ~::ReaderMask) == 0) {
    if (_state.compare_exchange_weak(state, state + ::ReaderIncrement, std::memory_order_acquire)) {
      return true;
    }
  }
  return false;
}

void ReadWriteSpinLock::lockRead() noexcept {
  for (;;) {
    if (tryLockRead()) {
      return;
    }
    cpu_relax();
  }
}

bool ReadWriteSpinLock::lockRead(std::size_t maxAttempts) noexcept {
  uint64_t attempts = 0;
  while (attempts++ < maxAttempts) {
    if (tryLockRead()) {
      return true;
    }
    cpu_relax();
  }
  return false;
}

void ReadWriteSpinLock::unlock() noexcept {
  if (isLockedWrite()) {
    unlockWrite();
  } else {
    TRI_ASSERT(isLockedRead());
    unlockRead();
  }
}

void ReadWriteSpinLock::unlockRead() noexcept {
  TRI_ASSERT(isLockedRead());
  _state.fetch_sub(::ReaderIncrement, std::memory_order_release);
}

void ReadWriteSpinLock::unlockWrite() noexcept {
  TRI_ASSERT(isLockedWrite());
  _state.fetch_sub(::WriteLock, std::memory_order_release);
}

bool ReadWriteSpinLock::isLocked() const noexcept {
  return (_state.load(std::memory_order_relaxed) & ~::QueuedWriterMask) != 0;
}

bool ReadWriteSpinLock::isLockedRead() const noexcept {
  return (_state.load(std::memory_order_relaxed) & ::ReaderMask) > 0;
}

bool ReadWriteSpinLock::isLockedWrite() const noexcept {
  return _state.load(std::memory_order_relaxed) & ::WriteLock;
}

}
