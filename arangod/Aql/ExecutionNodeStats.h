////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace arangodb::aql {

/// @brief statistics per ExecutionNode
struct ExecutionNodeStats {
  // number of calls, total
  uint64_t calls = 0;
  // number of parallel calls
  uint64_t parallel = 0;
  // number of items produced
  uint64_t items = 0;
  // filtered is only populated by some nodes
  uint64_t filtered = 0;
  // runtime, including time spent in fetchers
  double runtime = 0.0;
  // time spent in fetchers
  double fetching = 0.0;

  ExecutionNodeStats& operator+=(ExecutionNodeStats const& other) noexcept {
    calls += other.calls;
    parallel += other.parallel;
    items += other.items;
    filtered += other.filtered;
    runtime += other.runtime;
    fetching += other.fetching;
    return *this;
  }
};

}  // namespace arangodb::aql
