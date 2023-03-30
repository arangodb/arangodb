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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include "Pregel/DatabaseTypes.h"

namespace arangodb::pregel::conductor {

struct CollectionLookup {
  virtual ~CollectionLookup() = default;

  using CollectionPlanIDMapping = std::unordered_map<CollectionID, std::string>;
  using ServerMapping =
      std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>;
  using ShardsMapping = std::vector<ShardID>;

  // vertices related methods:
  [[nodiscard]] virtual auto getServerMapVertices() const -> ServerMapping = 0;
  // edges related methods:
  [[nodiscard]] virtual auto getServerMapEdges() const -> ServerMapping = 0;
  // both combined (vertices, edges) related methods:
  [[nodiscard]] virtual auto getAllShards() const -> ShardsMapping = 0;
  [[nodiscard]] virtual auto getCollectionPlanIdMapAll() const
      -> CollectionPlanIDMapping = 0;
};

}  // namespace arangodb::pregel::conductor
