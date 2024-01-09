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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <map>
#include "Pregel/DatabaseTypes.h"
#include "Inspection/Format.h"

namespace arangodb::pregel {

struct CollectionSpecifications {
  std::map<CollectionID, std::vector<PregelShardID>> vertexShards;
  std::map<CollectionID, std::vector<PregelShardID>> edgeShards;
  std::unordered_map<CollectionID, std::string> collectionPlanIds;
  std::vector<PregelShardID> allShards;
};
template<typename Inspector>
auto inspect(Inspector& f, CollectionSpecifications& x) {
  return f.object(x).fields(f.field("vertexShards", x.vertexShards),
                            f.field("edgeShards", x.edgeShards),
                            f.field("collectionPlanIds", x.collectionPlanIds),
                            f.field("shards", x.allShards));
}

}  // namespace arangodb::pregel

template<>
struct fmt::formatter<arangodb::pregel::CollectionSpecifications>
    : arangodb::inspection::inspection_formatter {};
