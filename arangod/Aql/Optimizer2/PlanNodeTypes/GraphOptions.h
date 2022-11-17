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
/// @author Aditya Mukhopadhyay, Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Optimizer2/Types/Types.h"

#include "Inspection/VPackInspection.h"
#include "Projections.h"

namespace arangodb::aql::optimizer2::types {
struct GraphOptions {
  AttributeTypes::Numeric parallelism;
  bool refactor;
  bool produceVertices;
  AttributeTypes::Numeric maxProjections;
  std::optional<Projections> vertexProjections;
  std::optional<Projections> edgeProjections;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphOptions& x) {
  auto res = f.object(x).fields(
      f.field("parallelism", x.parallelism), f.field("refactor", x.refactor),
      f.field("produceVertices", x.produceVertices),
      f.field("maxProjections", x.maxProjections),
      f.field("vertexProjections", x.vertexProjections),
      f.field("edgeProjections", x.edgeProjections));

  return res;
}

struct TraverserOptions : GraphOptions {
  bool neighbors;
  bool producePathsVertices;
  bool producePathsEdges;
  bool producePathsWeights;

  AttributeTypes::Numeric minDepth;
  AttributeTypes::Numeric maxDepth;
  AttributeTypes::Double defaultWeight;

  AttributeTypes::String uniqueVertices;
  AttributeTypes::String uniqueEdges;
  AttributeTypes::String order;
  AttributeTypes::String weightAttribute;

  std::optional<std::vector<AttributeTypes::String>> vertexCollections;
  std::optional<std::vector<AttributeTypes::String>> edgeCollections;
};

template<typename Inspector>
auto inspect(Inspector& f, TraverserOptions& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<GraphOptions&>(x)),
      f.field("neighbors", x.neighbors),
      f.field("producePathsVertices", x.producePathsVertices),
      f.field("producePathsEdges", x.producePathsEdges),
      f.field("producePathsWeights", x.producePathsWeights),

      f.field("minDepth", x.minDepth), f.field("maxDepth", x.maxDepth),
      f.field("defaultWeight", x.defaultWeight),

      f.field("uniqueVertices", x.uniqueVertices),
      f.field("uniqueEdges", x.uniqueEdges), f.field("order", x.order),
      f.field("weightAttribute", x.weightAttribute),
      f.field("vertexCollections", x.vertexCollections),
      f.field("edgeCollections", x.edgeCollections));
}
}  // namespace arangodb::aql::optimizer2::types
