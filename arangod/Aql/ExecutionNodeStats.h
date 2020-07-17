////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_EXECUTION_NODE_STATS_H
#define ARANGOD_AQL_EXECUTION_NODE_STATS_H 1

#include <cstdint>

/// @brief statistics per ExecutionNode
struct ExecutionNodeStats {
  size_t calls = 0;
  size_t items = 0;
  double runtime = 0.0;

  ExecutionNodeStats& operator+=(ExecutionNodeStats const& other) {
    calls += other.calls;
    items += other.items;
    runtime += other.runtime;
    return *this;
  }
};

#endif
