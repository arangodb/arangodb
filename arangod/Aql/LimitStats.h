////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_LIMIT_STATS_H
#define ARANGOD_AQL_LIMIT_STATS_H

#include "ExecutionStats.h"

#include <cstddef>

namespace arangodb::aql {

class LimitStats {
 public:
  LimitStats() noexcept = default;
  LimitStats(LimitStats const&) = default;
  // It is relied upon that other._fullcount is zero after the move!
  LimitStats(LimitStats&& other) noexcept;

  auto operator=(LimitStats const&) -> LimitStats& = default;
  auto operator=(LimitStats&& other) noexcept -> LimitStats&;

  void incrFullCount() noexcept;
  void incrFullCountBy(size_t amount) noexcept;

  [[nodiscard]] auto getFullCount() const noexcept -> std::size_t;
  
  auto operator+=(LimitStats const& other) noexcept -> void {
    incrFullCountBy(other.getFullCount());
  }

 private:
  std::size_t _fullCount{0};
  // Don't forget to update operator== when adding new members!
};

auto operator+=(ExecutionStats& executionStats, LimitStats const& limitStats) noexcept
    -> ExecutionStats&;

auto operator==(LimitStats const&, LimitStats const&) noexcept -> bool;

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_LIMIT_STATS_H
