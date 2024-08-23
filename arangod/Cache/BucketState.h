////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace arangodb::cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple state class with a small footprint.
///
/// Underlying store is simply a std::atomic<uint16_t>, and each bit corresponds
/// to a flag that can be set. The lowest bit is special and is designated as
/// the locking flag. Any access (read or modify) to the state must occur when
/// the state is already locked; the two exceptions are to check whether the
/// state is locked and, of course, to lock it. Any flags besides the lock flag
/// are treated uniformly, and can be checked or toggled. Each flag is defined
/// via an enum and must correspond to exactly one set bit.
////////////////////////////////////////////////////////////////////////////////
struct BucketState {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Flags which can be queried or toggled to reflect state.
  ///
  /// Each flag must have exactly one bit set, fit in uint16_t. The 'locked'
  /// flag is special and should remain the least-significant bit. When other
  /// flags are added,they should be kept in alphabetical order for readability,
  /// and all flag values should be adjusted to keep bit-significance in
  /// ascending order.
  //////////////////////////////////////////////////////////////////////////////
  enum class Flag : std::uint16_t {
    locked = 0x0001,
    banished = 0x0002,
    migrated = 0x0004,
  };

  using FlagType = std::underlying_type<Flag>::type;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state with no flags set and unlocked
  //////////////////////////////////////////////////////////////////////////////
  BucketState() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state to match another
  //////////////////////////////////////////////////////////////////////////////
  BucketState(BucketState const& other) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes state to match another
  //////////////////////////////////////////////////////////////////////////////
  BucketState& operator=(BucketState const& other) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if state is locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isLocked() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Used to lock state. Returns true if locked, false otherwise.
  ///
  /// By default, it will try as many times as necessary to acquire the lock.
  /// The number of tries may be limited to any positive integer value to avoid
  /// excessively long waits. The return value indicates whether the state was
  /// locked or not. The optional second parameter is a function which will be
  /// called upon successfully locking the state.
  //////////////////////////////////////////////////////////////////////////////
  bool lock(std::uint64_t maxTries =
                std::numeric_limits<std::uint64_t>::max()) noexcept;

  template<typename F>
  bool lock(std::uint64_t maxTries, F&& cb) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, F>);
    bool success = this->lock(maxTries);
    if (success) {
      std::invoke(std::forward<F>(cb));
    }
    return success;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unlocks the state. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void unlock() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether the given flag is set. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isSet(BucketState::Flag flag) const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Toggles the given flag. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void toggleFlag(BucketState::Flag flag) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unsets all flags besides Flag::locked. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void clear() noexcept;

 private:
  std::atomic<FlagType> _state;
};

// ensure that state is exactly the size of uint16_t
static_assert(sizeof(BucketState) == sizeof(std::uint16_t));

};  // end namespace arangodb::cache
