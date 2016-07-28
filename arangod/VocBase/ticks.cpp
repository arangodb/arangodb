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

#include "ticks.h"
#include "Basics/HybridLogicalClock.h"

using namespace arangodb;
using namespace arangodb::basics;

/// @brief current tick identifier (48 bit)
static std::atomic<uint64_t> CurrentTick(0);

/// @brief a hybrid logical clock
static HybridLogicalClock hybridLogicalClock;

/// @brief create a new tick, using a hybrid logical clock
TRI_voc_tick_t TRI_HybridLogicalClock(void) {
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
  while (expected < t &&
         !CurrentTick.compare_exchange_weak(expected, t,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
    expected = CurrentTick.load(std::memory_order_relaxed);
  }
}

/// @brief returns the current tick counter
TRI_voc_tick_t TRI_CurrentTickServer() { return CurrentTick; }
