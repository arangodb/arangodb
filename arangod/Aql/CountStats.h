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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COUNT_STATS_H
#define ARANGOD_AQL_COUNT_STATS_H

#include <cstddef>
#include "ExecutionStats.h"


namespace arangodb {
namespace aql {

class CountStats {
 public:
  CountStats() noexcept : _counted(0) {}

  void setCounted(std::size_t counted) noexcept { _counted = counted; }

  void addCounted(std::size_t counted) noexcept { _counted += counted; }

  void incrCounted() noexcept { _counted++; }

  std::size_t getCounted() const noexcept { return _counted; }

 private:
  std::size_t _counted;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                           CountStats const& filterStats) noexcept {
  executionStats.count += filterStats.getCounted();
  return executionStats;
}

}
}

#endif // ARANGOD_AQL_FILTER_STATS_H
