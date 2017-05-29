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
  typedef std::function<void()> CallbackType;

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
    locked = 0x00000001,
    blacklisted = 0x00000002,
    disabled = 0x00000004,
    evictions = 0x00000008,
    migrated = 0x00000010,
    migrating = 0x00000020,
    rebalancing = 0x00000040,
    resizing = 0x00000080,
    shutdown = 0x00000100,
    shuttingDown = 0x00000200,
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
  /// @brief Checks if state is locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isLocked() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Used to lock state. Returns true if locked, false otherwise.
  ///
  /// By default, it will try as many times as necessary to acquire the lock.
  /// The number of tries may be limited to any positive integer value to avoid
  /// excessively long waits. The return value indicates whether the state was
  /// locked or not. The optional second parameter is a function which will be
  /// called upon successfully locking the state.
  //////////////////////////////////////////////////////////////////////////////
  bool lock(int64_t maxTries = -1LL, State::CallbackType cb = []() -> void {});

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unlocks the state. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void unlock();

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

 private:
  std::atomic<uint32_t> _state;
};

// ensure that state is exactly the size of uint32_t
static_assert(sizeof(State) == sizeof(uint32_t),
              "Expected sizeof(State) == sizeof(uint32_t).");

};  // end namespace cache
};  // end namespace arangodb

#endif
