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

#pragma once

#include <atomic>
#include <cstddef>

namespace arangodb::aql {
class AsyncPrefetchSlotsManager;

class AsyncPrefetchSlotsReservation {
 public:
  AsyncPrefetchSlotsReservation(AsyncPrefetchSlotsReservation const&) = delete;
  AsyncPrefetchSlotsReservation& operator=(
      AsyncPrefetchSlotsReservation const&) = delete;

  AsyncPrefetchSlotsReservation(AsyncPrefetchSlotsManager& manager,
                                size_t slotsReserved) noexcept;
  AsyncPrefetchSlotsReservation& operator=(
      AsyncPrefetchSlotsReservation&&) noexcept;
  ~AsyncPrefetchSlotsReservation();

  size_t value() const noexcept { return _slotsReserved; }

 private:
  void returnSlots() noexcept;

  AsyncPrefetchSlotsManager& _manager;
  size_t _slotsReserved;
};

class AsyncPrefetchSlotsManager {
  friend class AsyncPrefetchSlotsReservation;

 public:
  AsyncPrefetchSlotsManager(AsyncPrefetchSlotsManager const&) = delete;
  AsyncPrefetchSlotsManager& operator=(AsyncPrefetchSlotsManager const&) =
      delete;

  AsyncPrefetchSlotsManager() noexcept;
  AsyncPrefetchSlotsManager(size_t maxSlotsTotal,
                            size_t maxSlotsPerQuery) noexcept;

  void configure(size_t maxSlotsTotal, size_t maxSlotsPerQuery) noexcept;

  AsyncPrefetchSlotsReservation leaseSlots(size_t value) noexcept;

 private:
  void returnSlots(size_t value) noexcept;

  size_t _maxSlotsTotal;
  size_t _maxSlotsPerQuery;

  std::atomic<size_t> _slotsUsed;
};

}  // namespace arangodb::aql
