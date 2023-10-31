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

#include <memory>
#include <functional>
#include <unordered_map>

namespace arangodb::pregel {

struct GraphByCollections {
  std::vector<std::string> vertexCollections;
  std::vector<std::string> edgeCollections;
  std::unordered_map<std::string, std::vector<std::string>>
      edgeCollectionRestrictions;
  std::string shardKeyAttribute;

  [[nodiscard]] auto isRestricted(ShardID vertexCollection,
                                  ShardID edgeCollection) const -> bool {
    auto maybeEdgeRestrictions =
        edgeCollectionRestrictions.find(vertexCollection);

    if (maybeEdgeRestrictions == std::end(edgeCollectionRestrictions)) {
      return false;
    } else {
      auto const& [_, edgeRestrictions] = *maybeEdgeRestrictions;
      if (edgeRestrictions.empty()) {
        return false;
      } else {
        return std::find(std::begin(edgeRestrictions),
                         std::end(edgeRestrictions),
                         edgeCollection) == std::end(edgeRestrictions);
      }
    }
  }
};

template<typename Inspector>
auto inspect(Inspector& f, GraphByCollections& x) {
  return f.object(x).fields(
      f.field("vertexCollections", x.vertexCollections),
      f.field("edgeCollections", x.edgeCollections),
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions),
      f.field("shardKeyAttribute", x.shardKeyAttribute));
}

}  // namespace arangodb::pregel
