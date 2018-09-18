////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_FILTER_STATS_H
#define ARANGOD_AQL_FILTER_STATS_H

#include <cstddef>
#include "ExecutionStats.h"


namespace arangodb {
namespace aql {

class FilterStats {
 public:
  FilterStats() noexcept : _filtered(0) {}

  void setFiltered(std::size_t filtered_) noexcept { _filtered = filtered_; }

  void addFiltered(std::size_t filtered_) noexcept { _filtered += filtered_; }

  constexpr std::size_t getFiltered() const noexcept { return _filtered; }

 private:
  std::size_t _filtered;
};

ExecutionStats& operator+=(ExecutionStats& executionStats,
                           FilterStats const& filterStats) noexcept {
  executionStats.filtered += filterStats.getFiltered();
}

}
}

#endif // ARANGOD_AQL_FILTER_STATS_H
