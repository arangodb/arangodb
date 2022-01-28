////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <variant>
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"

namespace arangodb::pregel3 {

struct GraphSpecification {
  using GraphName = std::string;

  struct GraphSpecificationByCollections {
    std::vector<std::string> vertexCollectionNames;
    std::vector<std::string> edgeCollectionNames;

    GraphSpecificationByCollections() = default;

    GraphSpecificationByCollections(
        std::vector<std::string>&& vertexCollectionNames,
        std::vector<std::string>&& edgeCollectionNames)
        : vertexCollectionNames(vertexCollectionNames),
          edgeCollectionNames(edgeCollectionNames) {}

    GraphSpecificationByCollections(
        GraphSpecificationByCollections&& gSBC) noexcept {
      vertexCollectionNames = std::move(gSBC.vertexCollectionNames);
      edgeCollectionNames = std::move(gSBC.edgeCollectionNames);
    }

    GraphSpecificationByCollections(
        GraphSpecificationByCollections const& other) = default;

    GraphSpecificationByCollections& operator=(
        const GraphSpecificationByCollections& other) = default;
  };

  std::variant<GraphName, GraphSpecificationByCollections> _graphSpec;
  explicit GraphSpecification(GraphName graphName) : _graphSpec(graphName) {}
  explicit GraphSpecification(
      GraphSpecificationByCollections&& graphSpecByCollections)
      : _graphSpec(std::move(graphSpecByCollections)) {}

  /**
   * Translate the given slice to _graphSpec. Check that slice has the right
   * format.
   * @param slice either String (the graph name) or
   *               { "vertexCollNames": [Strings], {"edgeCollNames": [Strings] }
   */
  static auto fromVelocyPack(VPackSlice slice) -> GraphSpecification;

  static void toVelocyPack(VPackBuilder&);
};
}  // namespace arangodb::pregel3