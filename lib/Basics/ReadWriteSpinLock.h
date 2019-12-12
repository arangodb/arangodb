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

#include <atomic>

namespace arangodb::basics {

class ReadWriteSpinLock {
 public:
  ReadWriteSpinLock() = default;

  // only needed for cache::Metadata
  ReadWriteSpinLock(ReadWriteSpinLock&& other) noexcept;

  ReadWriteSpinLock& operator=(ReadWriteSpinLock&& other) noexcept;

  [[nodiscard]] bool tryWriteLock() noexcept;
  void writeLock() noexcept;
  [[nodiscard]] bool writeLock(uint64_t maxAttempts) noexcept;

  [[nodiscard]] bool tryReadLock() noexcept;
  void readLock() noexcept;
  [[nodiscard]] bool readLock(uint64_t maxAttempts) noexcept;

  void readUnlock() noexcept;
  void unlockRead() noexcept;

  void writeUnlock() noexcept;
  void unlockWrite() noexcept;

  [[nodiscard]] bool isLocked() const noexcept;
  [[nodiscard]] bool isReadLocked() const noexcept;
  [[nodiscard]] bool isWriteLocked() const noexcept;

 private:
  /// @brief _state, lowest bit is write_lock, the next 15 bits is the number of
  /// queued writers, the last 16 bits the number of active readers.
  std::atomic<uint32_t> _state = 0;

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
