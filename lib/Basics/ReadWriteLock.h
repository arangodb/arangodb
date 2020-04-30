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
/// @author Max Neunhoeffer
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_READ_WRITE_LOCK_H
#define ARANGODB_BASICS_READ_WRITE_LOCK_H 1

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace arangodb::basics {

/// @brief read-write lock, slow but just using CPP11
/// This class has two other advantages:
///  (1) it is possible that a thread tries to acquire a lock even if it
///      has it already. This is important when we are running a thread
///      pool that works on task groups and a task group needs to acquire
///      a lock across multiple (non-concurrent) tasks. This must work,
///      even if tasks from different groups that fight for a lock are
///      actually executed by the same thread! POSIX RW-locks do not have
///      this property.
///  (2) write locks have a preference over read locks: as long as a task
///      wants to get a write lock, no other task can get a (new) read lock.
///      This is necessary to avoid starvation of writers by many readers.
///      The current implementation can starve readers, though.
class ReadWriteLock {
 public:
  ReadWriteLock() : _state(0) {}

  /// @brief locks for writing
  void lockWrite();

  /// @brief locks for writing within microsecond timeout
  [[nodiscard]] bool writeLock(uint64_t timeout) {
    std::chrono::microseconds ms(timeout);
    return writeLock(ms);
  }

  [[nodiscard]] bool writeLock(std::chrono::microseconds timeout);

  /// @brief locks for writing, but only tries
  [[nodiscard]] bool tryLockWrite();

  /// @brief locks for reading
  void lockRead();

  /// @brief locks for reading, tries only
  [[nodiscard]] bool tryLockRead();

  /// @brief releases the read-lock or write-lock
  void unlock() noexcept;

  /// @brief releases the read-lock
  void unlockRead() noexcept;

  /// @brief releases the write-lock
  void unlockWrite() noexcept;
  
  [[nodiscard]] bool isLocked() const noexcept;
  [[nodiscard]] bool isLockedRead() const noexcept;
  [[nodiscard]] bool isLockedWrite() const noexcept;

 private:
  /// @brief mutex for _readers_bell cv
  std::mutex _reader_mutex;

  /// @brief a condition variable to wake up all reader threads
  std::condition_variable _readers_bell;

  /// @brief mutex for _writers_bell cv
  std::mutex _writer_mutex;

  /// @brief a condition variable to wake up one writer thread
  std::condition_variable _writers_bell;

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
}  // namespace arangodb::basics

#endif
