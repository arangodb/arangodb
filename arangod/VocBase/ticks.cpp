////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <atomic>

#include "ticks.h"

#include "Basics/HybridLogicalClock.h"
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::basics;

/// @brief current tick identifier (48 bit)
static std::atomic<uint64_t> CurrentTick(0);

/// @brief a hybrid logical clock
static HybridLogicalClock hybridLogicalClock;

/// @brief create a new tick, using a hybrid logical clock
TRI_voc_tick_t TRI_HybridLogicalClock() {
  return hybridLogicalClock.getTimeStamp();
}

/// @brief create a new tick, using a hybrid logical clock, this variant
/// is supposed to be called when a time stamp is received in network
/// communications.
TRI_voc_tick_t TRI_HybridLogicalClock(TRI_voc_tick_t received) {
  return hybridLogicalClock.getTimeStamp(received);
}

/// @brief create a new tick
TRI_voc_tick_t TRI_NewTickServer() { return ++CurrentTick; }

/// @brief updates the tick counter, with lock
void TRI_UpdateTickServer(TRI_voc_tick_t tick) {
  TRI_voc_tick_t t = tick;

  auto expected = CurrentTick.load(std::memory_order_relaxed);

  // only update global tick if less than the specified value...
  while (expected < t && !CurrentTick.compare_exchange_weak(expected, t, std::memory_order_release,
                                                            std::memory_order_relaxed)) {
    expected = CurrentTick.load(std::memory_order_relaxed);
  }
}

/// @brief returns the current tick counter
TRI_voc_tick_t TRI_CurrentTickServer() { return CurrentTick; }

/// @brief generates a new tick which also encodes this server's id
TRI_voc_tick_t TRI_NewServerSpecificTick() {
  static constexpr uint64_t LowerMask{0x000000FFFFFFFFFF};
  static constexpr uint64_t UpperMask{0xFFFFFF0000000000};
  static constexpr size_t UpperShift{40};

  uint64_t lower = TRI_NewTickServer() & LowerMask;
  uint64_t upper =
      (static_cast<uint64_t>(ServerState::instance()->getShortId()) << UpperShift) & UpperMask;
  uint64_t tick = (upper | lower);
  return static_cast<TRI_voc_tick_t>(tick);
}

/// @brief generates a new tick which also encodes this server's id, and is
/// congruent to 0 modulo 4
TRI_voc_tick_t TRI_NewServerSpecificTickMod4() {
  static constexpr uint64_t LowerMask{0x000000FFFFFFFFFC};
  static constexpr uint64_t UpperMask{0xFFFFFF0000000000};
  static constexpr size_t LowerShift{2};
  static constexpr size_t UpperShift{40};

  const uint64_t lower = (TRI_NewTickServer() << LowerShift) & LowerMask;
  const uint64_t upper =
      (static_cast<uint64_t>(ServerState::instance()->getShortId()) << UpperShift) & UpperMask;
  return static_cast<TRI_voc_tick_t>(upper | lower);
}

/// @brief extracts the server id from a server-specific tick
uint32_t TRI_ExtractServerIdFromTick(TRI_voc_tick_t tick) {
  static constexpr uint64_t Mask{0x0000000000FFFFFF};
  static constexpr size_t Shift{40};

  uint32_t shortId = static_cast<uint32_t>((tick >> Shift) & Mask);
  return shortId;
}
