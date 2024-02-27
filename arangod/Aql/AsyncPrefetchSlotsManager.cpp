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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AsyncPrefetchSlotsManager.h"
#include "Assertions/Assert.h"

#include <utility>

namespace arangodb::aql {

AsyncPrefetchSlotsReservation::AsyncPrefetchSlotsReservation() noexcept
    : _manager(nullptr), _slotsReserved(0) {}

AsyncPrefetchSlotsReservation::AsyncPrefetchSlotsReservation(
    AsyncPrefetchSlotsManager* manager, size_t slotsReserved) noexcept
    : _manager(manager), _slotsReserved(slotsReserved) {
  // if number of slots is > 0, we must have a manager
  TRI_ASSERT(_slotsReserved == 0 || _manager != nullptr);
}

AsyncPrefetchSlotsReservation& AsyncPrefetchSlotsReservation::operator=(
    AsyncPrefetchSlotsReservation&& other) noexcept {
  if (this != &other) {
    returnSlots();
    if (other._manager != nullptr) {
      _manager = other._manager;
    }
    _slotsReserved = std::exchange(other._slotsReserved, 0);
  }
  return *this;
}

AsyncPrefetchSlotsReservation::~AsyncPrefetchSlotsReservation() {
  returnSlots();
}

void AsyncPrefetchSlotsReservation::returnSlots() noexcept {
  if (_manager != nullptr && _slotsReserved > 0) {
    _manager->returnSlots(_slotsReserved);
    _slotsReserved = 0;
    // leave _manager in place
  }
  TRI_ASSERT(_slotsReserved == 0);
}

AsyncPrefetchSlotsManager::AsyncPrefetchSlotsManager(size_t maxSlotsTotal,
                                                     size_t maxSlotsPerQuery)
    : _maxSlotsTotal(maxSlotsTotal),
      _maxSlotsPerQuery(maxSlotsPerQuery),
      _slotsUsed(0) {}

AsyncPrefetchSlotsReservation AsyncPrefetchSlotsManager::leaseSlots(
    size_t value) noexcept {
  // reduce value to the allowed per-query maximum
  value = std::min(_maxSlotsPerQuery, value);

  if (value > 0) {
    // check if global (per-server) total is below the configured total maximum
    size_t expected = _slotsUsed.load(std::memory_order_relaxed);
    TRI_ASSERT(expected <= _maxSlotsTotal);
    // as long as we have slots left, try to allocate some
    while (expected < _maxSlotsTotal) {
      size_t target = std::min(_maxSlotsTotal, expected + value);
      TRI_ASSERT(target > expected);
      if (_slotsUsed.compare_exchange_weak(expected, target,
                                           std::memory_order_relaxed)) {
        return {this, target - expected};
      }
      TRI_ASSERT(expected <= _maxSlotsTotal);
    }
  }
  return {};
}

void AsyncPrefetchSlotsManager::returnSlots(size_t value) noexcept {
  if (value == 0) {
    return;
  }
  [[maybe_unused]] size_t previous =
      _slotsUsed.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(previous >= value);
}

}  // namespace arangodb::aql
