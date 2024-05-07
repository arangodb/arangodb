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

#include <atomic>
#include <cstdint>

#include "Cache/BucketState.h"
#include "Basics/cpu-relax.h"
#include "Basics/debugging.h"

namespace arangodb::cache {

BucketState::BucketState() noexcept : _state(0) {}

BucketState::BucketState(BucketState const& other) noexcept
    : _state(other._state.load()) {}

BucketState& BucketState::operator=(BucketState const& other) noexcept {
  if (this != &other) {
    _state = other._state.load();
  }

  return *this;
}

bool BucketState::isLocked() const noexcept {
  return (_state.load() & static_cast<FlagType>(Flag::locked)) != 0;
}

bool BucketState::lock(std::uint64_t maxTries) noexcept {
  uint64_t attempts = 0;
  do {
    // expect unlocked, but need to preserve migrating status
    FlagType current = _state.load(std::memory_order_relaxed);
    FlagType expected =
        static_cast<FlagType>(current & (~static_cast<FlagType>(Flag::locked)));
    if (current == expected) {
      FlagType desired =
          static_cast<FlagType>(expected | static_cast<FlagType>(Flag::locked));
      // try to lock
      bool success = _state.compare_exchange_strong(expected, desired,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_relaxed);
      if (success) {
        return true;
      }
    }
    basics::cpu_relax();
    // TODO: exponential back-off for failure?
  } while (++attempts < maxTries);

  return false;
}

void BucketState::unlock() noexcept {
  TRI_ASSERT(isLocked());
  _state.fetch_and(static_cast<FlagType>(~static_cast<FlagType>(Flag::locked)),
                   std::memory_order_release);
}

bool BucketState::isSet(BucketState::Flag flag) const noexcept {
  TRI_ASSERT(isLocked());
  return static_cast<FlagType>(_state.load() & static_cast<FlagType>(flag)) !=
         0;
}

void BucketState::toggleFlag(BucketState::Flag flag) noexcept {
  TRI_ASSERT(isLocked());
  _state ^= static_cast<FlagType>(flag);
}

void BucketState::clear() noexcept {
  TRI_ASSERT(isLocked());
  _state = static_cast<FlagType>(Flag::locked);
}
}  // namespace arangodb::cache
