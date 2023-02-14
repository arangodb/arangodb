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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <limits>

namespace arangodb::pregel::algos {

// Label propagation
struct LPValue {
  /// The desired partition the vertex want to migrate to.
  uint64_t currentCommunity = 0;
  /// The actual partition.
  uint64_t lastCommunity = std::numeric_limits<uint64_t>::max();
  /// Iterations since last migration.
  uint64_t stabilizationRounds = 0;
};

template<typename Inspector>
auto inspect(Inspector& f, LPValue& v) {
  return f.object(v).fields(f.field("currentCommunity", v.currentCommunity),
                            f.field("lastCommunity", v.lastCommunity),
                            f.field("stabilizationRounds", v.stabilizationRounds));
}


}  // namespace arangodb::pregel::algos
