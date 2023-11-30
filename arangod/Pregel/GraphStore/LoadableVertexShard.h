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

#include <vector>

#include "Pregel/DatabaseTypes.h"
#include "Pregel/GraphStore/PregelShard.h"

namespace arangodb::pregel {
struct LoadableVertexShard {
  PregelShard pregelShard;
  PregelShardID vertexShard;
  ServerID responsibleServer;
  CollectionName collectionName;
  std::vector<PregelShardID> edgeShards;
};
template<typename Inspector>
auto inspect(Inspector& f, LoadableVertexShard& x) {
  return f.object(x).fields(f.field("pregelShard", x.pregelShard),
                            f.field("vertexShard", x.vertexShard),
                            f.field("responsibleServer", x.responsibleServer),
                            f.field("collectionName", x.collectionName),
                            f.field("edgeShards", x.edgeShards));
}
}  // namespace arangodb::pregel
