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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_READ_WRITE_SPIN_LOCK_H
#define ARANGO_READ_WRITE_SPIN_LOCK_H 1

#include <atomic>
#include <cstdint>

namespace arangodb::basics {

class ReadWriteSpinLock {
 public:
  ReadWriteSpinLock() = default;

  // only needed for cache::Metadata
  ReadWriteSpinLock(ReadWriteSpinLock&& other) noexcept;

  ReadWriteSpinLock& operator=(ReadWriteSpinLock&& other) noexcept;

  [[nodiscard]] bool tryLockWrite() noexcept;
  void lockWrite() noexcept;
  [[nodiscard]] bool lockWrite(std::size_t maxAttempts) noexcept;

  [[nodiscard]] bool tryLockRead() noexcept;
  void lockRead() noexcept;
  [[nodiscard]] bool lockRead(std::size_t maxAttempts) noexcept;

  void unlock() noexcept;
  void unlockRead() noexcept;
  void unlockWrite() noexcept;

  [[nodiscard]] bool isLocked() const noexcept;
  [[nodiscard]] bool isLockedRead() const noexcept;
  [[nodiscard]] bool isLockedWrite() const noexcept;

 private:
  /// @brief _state, lowest bit is write_lock, the next 15 bits is the number of
  /// queued writers, the last 16 bits the number of active readers.
  std::atomic<std::uint32_t> _state = 0;
};

}  // namespace arangodb::basics

#endif
