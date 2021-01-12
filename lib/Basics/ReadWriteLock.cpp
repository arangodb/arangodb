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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ReadWriteLock.h"

#include "Basics/debugging.h"

using namespace arangodb::basics;

/// @brief locks for writing
void ReadWriteLock::lockWrite() {
  if (tryLockWrite()) {
    return;
  }

  // the lock is either hold by another writer or we have active readers
  // -> announce that we want to write
  _state.fetch_add(QUEUED_WRITER_INC, std::memory_order_relaxed);

  std::unique_lock<std::mutex> guard(_writer_mutex);
  while (true) {
    // it is intentional to reload _state after acquiring the mutex,
    // because in case we were blocked (because someone else was holding
    // the mutex), _state most likely has changed, causing the subsequent
    // CAS to fail. If we were not blocked, then the additional load will
    // most certainly hit the L1 cache and should therefore be very cheap.
    auto state = _state.load(std::memory_order_relaxed);
    // try to acquire write lock as long as no readers or writers are active,
    while ((state & ~QUEUED_WRITER_MASK) == 0) {
      // try to acquire lock and perform queued writer decrement in one step
      if (_state.compare_exchange_weak(state, (state - QUEUED_WRITER_INC) | WRITE_LOCK,
                                       std::memory_order_acquire)) {
        return;
      }
    }
    _writers_bell.wait(guard);
  }
}

/// @brief lock for writes with microsecond timeout
bool ReadWriteLock::lockWrite(std::chrono::microseconds timeout) {
  if (tryLockWrite()) {
    return true;
  }

  // the lock is either hold by another writer or we have active readers
  // -> announce that we want to write
  _state.fetch_add(QUEUED_WRITER_INC, std::memory_order_relaxed);

  std::chrono::time_point<std::chrono::steady_clock> end_time;
  end_time = std::chrono::steady_clock::now() + timeout;

  std::cv_status status(std::cv_status::no_timeout);
  {
    std::unique_lock<std::mutex> guard(_writer_mutex);
    while (std::cv_status::no_timeout == status) {
      auto state = _state.load(std::memory_order_relaxed);
      // try to acquire write lock as long as no readers or writers are active,
      while ((state & ~QUEUED_WRITER_MASK) == 0) {
        // try to acquire lock and perform queued writer decrement in one step
        if (_state.compare_exchange_weak(state, (state - QUEUED_WRITER_INC) | WRITE_LOCK,
                                         std::memory_order_acquire)) {
          return true;
        }
      }
      // TODO: it seems this may repeatedly wait for the timeout
      // it is not guaranteed to finish within timeout
      status = _writers_bell.wait_until(guard, end_time);
    }
  }

  // Undo the counting of us as queued writer:
  _state.fetch_sub(QUEUED_WRITER_INC, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> guard(_reader_mutex);
  }
  _readers_bell.notify_all();

  return false;
}

/// @brief locks for writing, but only tries
bool ReadWriteLock::tryLockWrite() noexcept {
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

/// @brief locks for reading
void ReadWriteLock::lockRead() {
  if (tryLockRead()) {
    return;
  }

  std::unique_lock<std::mutex> guard(_reader_mutex);
  while (true) {
    if (tryLockRead()) {
      return;
    }

    _readers_bell.wait(guard);
  }
}

/// @brief locks for reading, tries only
bool ReadWriteLock::tryLockRead() noexcept {
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

/// @brief releases the read-lock or write-lock
void ReadWriteLock::unlock() noexcept {
  if (_state.load(std::memory_order_relaxed) & WRITE_LOCK) {
    // we were holding the write-lock
    unlockWrite();
  } else {
    // we were holding a read-lock
    unlockRead();
  }
}

/// @brief releases the write-lock
/// Note that, theoretically, locking a mutex may throw. But in this case, we'd
/// be running into undefined behaviour, so we still want noexcept!
void ReadWriteLock::unlockWrite() noexcept {
  TRI_ASSERT((_state.load() & WRITE_LOCK) != 0);
  // clear the WRITE_LOCK flag
  auto state = _state.fetch_sub(WRITE_LOCK, std::memory_order_release);
  if ((state & QUEUED_WRITER_MASK) != 0) {
    // there are other writers waiting -> wake up one of them
    {
      std::lock_guard<std::mutex> guard(_writer_mutex);
    }
    _writers_bell.notify_one();
  } else {
    // no more writers -> wake up any waiting readings
    {
      std::lock_guard<std::mutex> guard(_reader_mutex);
    }
    _readers_bell.notify_all();
  }
}

/// @brief releases the read-lock
/// Note that, theoretically, locking a mutex may throw. But in this case, we'd
/// be running into undefined behaviour, so we still want noexcept!
void ReadWriteLock::unlockRead() noexcept {
  TRI_ASSERT((_state.load() & READER_MASK) != 0);
  auto state = _state.fetch_sub(READER_INC, std::memory_order_release) - READER_INC;
  if (state != 0 && (state & ~QUEUED_WRITER_MASK) == 0) {
    // we were the last reader and there are other writers waiting
    // -> wake up one of them
    {
      std::lock_guard<std::mutex> guard(_writer_mutex);
    }
    _writers_bell.notify_one();
  }
}

bool ReadWriteLock::isLocked() const noexcept {
  return (_state.load(std::memory_order_relaxed) & ~QUEUED_WRITER_MASK) != 0;
}

bool ReadWriteLock::isLockedRead() const noexcept {
  return (_state.load(std::memory_order_relaxed) & READER_MASK) > 0;
}

bool ReadWriteLock::isLockedWrite() const noexcept {
  return _state.load(std::memory_order_relaxed) & WRITE_LOCK;
}
