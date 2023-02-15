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

namespace arangodb::pregel::algos {

// Speaker-listerner Label propagation
struct SLPAValue {
  // our own initialized id
  uint64_t nodeId = 0;
  // number of received communities
  uint64_t numCommunities = 0;
  /// Memory used to hold the labelId and the count
  // used for memorizing communities
  std::map<uint64_t, uint64_t> memory;
};

template<typename Inspector>
auto inspect(Inspector& f, SLPAValue& v) {
  return f.object(v).fields(f.field("nodeId", v.nodeId),
                            f.field("numCommunities", v.numCommunities),
                            f.field("memory", v.memory));
}
}  // namespace arangodb::pregel::algos
