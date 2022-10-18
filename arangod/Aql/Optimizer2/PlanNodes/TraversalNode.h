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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/PlanNodes/BaseNode.h"
#include "Aql/Optimizer2/PlanNodes/CollectionAcessingNode.h"
#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"

#include "Inspection/VPackInspection.h"

namespace arangodb::aql::optimizer2::nodes {

struct GraphDefinition {
  std::vector<AttributeTypes::String> vertexCollectionNames;
  std::vector<AttributeTypes::String> edgeCollectionNames;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphDefinition& x) {
  return f.object(x).fields(
      f.field("vertexCollectionNames", x.vertexCollectionNames),
      f.field("edgeCollectionNames", x.edgeCollectionNames));
}

struct TraversalNode : optimizer2::nodes::BaseNode,
                       optimizer2::nodes::CollectionAccessingNode {
  bool isLocalGraphNode;
  bool isUsedAsSatellite;
  GraphDefinition graphDefinition;
  AttributeTypes::Numeric defaultDirection;
  std::vector<AttributeTypes::Numeric> directions;
  std::vector<AttributeTypes::String> edgeCollections;
  std::vector<AttributeTypes::String> vertexCollections;

  optimizer2::types::Variable vertexOutVariable;
  optimizer2::types::Variable edgeOutVariable;
};

template<typename Inspector>
auto inspect(Inspector& f, TraversalNode& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.embedFields(
          static_cast<optimizer2::nodes::CollectionAccessingNode&>(x)),
      f.field("isLocalGraphNode", x.isLocalGraphNode),
      f.field("isUsedAsSatellite", x.isUsedAsSatellite),
      f.field("graphDefinition", x.graphDefinition),
      f.field("defaultDirection", x.defaultDirection),
      f.field("vertexOutVariable", x.vertexOutVariable),
      f.field("edgeOutVariable", x.edgeOutVariable));
}

}  // namespace arangodb::aql::optimizer2::nodes
