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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_STATS_H
#define ARANGOD_AQL_STATS_H 1

#include "Aql/ExecutionStats.h"

namespace arangodb {
namespace aql {

// no-op statistics for all Executors that don't have custom stats.
class NoStats {};

inline ExecutionStats& operator+=(ExecutionStats& stats, NoStats const&) {
  return stats;
}


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


class FilterStats {
 public:
  FilterStats() noexcept : _filtered(0) {}

  void setFiltered(std::size_t filtered) noexcept { _filtered = filtered; }

  void addFiltered(std::size_t filtered) noexcept { _filtered += filtered; }

  void incrFiltered() noexcept { _filtered++; }

  std::size_t getFiltered() const noexcept { return _filtered; }

 private:
  std::size_t _filtered;
};

class EnumerateCollectionStats {
 public:
  EnumerateCollectionStats() noexcept : _filtered(0) {}

  void incrFiltered() noexcept { _filtered++; }

  std::size_t getFiltered() const noexcept { return _filtered; }

 private:
  std::size_t _filtered;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                           FilterStats const& filterStats) noexcept {
  executionStats.filtered += filterStats.getFiltered();
  return executionStats;
}

}  // namespace aql
}  // namespace arangodb
#endif
