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

#ifndef ARANGODB_CACHE_STATE_H
#define ARANGODB_CACHE_STATE_H

#include "Basics/Common.h"
#include "Basics/ReadWriteSpinLock.h"

#include <atomic>
#include <cstdint>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple state class with a small footprint.
///
/// Underlying store is simply a std::atomic<uint32_t>, and each bit corresponds
/// to a flag that can be set. The lowest bit is special and is designated as
/// the locking flag. Any access (read or modify) to the state must occur when
/// the state is already locked; the two exceptions are to check whether the
/// state is locked and, of course, to lock it. Any flags besides the lock flag
/// are treated uniformly, and can be checked or toggled. Each flag is defined
/// via an enum and must correspond to exactly one set bit.
////////////////////////////////////////////////////////////////////////////////
struct State {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Flags which can be queried or toggled to reflect state.
  ///
  /// Each flag must have exactly one bit set, fit in uint32_t. The 'locked'
  /// flag is special and should remain the least-significant bit. When other
  /// flags are added,they should be kept in alphabetical order for readability,
  /// and all flag values should be adjusted to keep bit-significance in
  /// ascending order.
  //////////////////////////////////////////////////////////////////////////////
  enum class Flag : uint32_t {
    blacklisted = 0x00000100,
    disabled = 0x00000200,
    evictions = 0x00000400,
    migrated = 0x00000800,
    migrating = 0x00001000,
    rebalancing = 0x00002000,
    resizing = 0x00004000,
    shutdown = 0x00008000,
    shuttingDown = 0x00010000,
  };

  enum class LockMask : uint32_t {
    read = 0x0fffffff,
    write = 0x10000000,
    any = 0x1fffffff,
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state with no flags set and unlocked
  //////////////////////////////////////////////////////////////////////////////
  State();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state to match another
  //////////////////////////////////////////////////////////////////////////////
  State(State const& other);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state to match another
  //////////////////////////////////////////////////////////////////////////////
  State& operator=(State const& other);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Virtual destructor for compiler compliance.
  //////////////////////////////////////////////////////////////////////////////
  virtual ~State();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if state is locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isLocked() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if state is locked for writing.
  //////////////////////////////////////////////////////////////////////////////
  bool isWriteLocked() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Used to lock state. Returns true if locked, false otherwise.
  ///
  /// By default, it will try as many times as necessary to acquire the lock.
  /// The number of tries may be limited to any positive integer value to avoid
  /// excessively long waits. The return value indicates whether the state was
  /// locked or not. The optional second parameter is a function which will be
  /// called upon successfully locking the state.
  //////////////////////////////////////////////////////////////////////////////
  virtual bool readLock(int64_t maxTries = -1LL);
  virtual bool writeLock(int64_t maxTries = -1LL);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unlocks the state. Requires state to be read-locked.
  //////////////////////////////////////////////////////////////////////////////
  virtual void readUnlock();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unlocks the state. Requires state to be write-locked.
  //////////////////////////////////////////////////////////////////////////////
  virtual void writeUnlock();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether the given flag is set. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isSet(State::Flag flag) const;
  bool isSet(State::Flag flag1, State::Flag flag2) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Toggles the given flag. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void toggleFlag(State::Flag flag);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unsets all flags besides Flag::locked. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void clear();

 protected:
  uint32_t _state;
  arangodb::basics::ReadWriteSpinLock<> _lock;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
