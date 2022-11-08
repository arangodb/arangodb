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

#include "Aql/Optimizer2/PlanNodes/BaseNode.h"
#include "Aql/Optimizer2/PlanNodes/CollectionAcessingNode.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"
#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"
#include <Inspection/VPackWithErrorT.h>

namespace arangodb::aql::optimizer2::nodes {
/*
 * GraphDefinition
 */
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

/*
 * GraphNode (MainNode)
 * (above structs used as inner helpers here)
 */
struct GraphNode : optimizer2::nodes::BaseNode {
  // inner structs
  GraphDefinition graphDefinition;

  // main
  AttributeTypes::String database;
  optimizer2::GraphInfo graph;
  AttributeTypes::Numeric defaultDirection;

  // variables
  optimizer2::types::Variable vertexOutVariable;
  optimizer2::types::Variable edgeOutVariable;
  std::vector<optimizer2::types::Variable::VariableId> optimizedOutVariables;
  optimizer2::types::Variable tmpObjVariable;

  // nodes
  optimizer2::types::Expression tmpObjVarNode;
  optimizer2::types::Expression tmpIdNode;

  // ee bools
  bool isLocalGraphNode;
  bool isUsedAsSatellite;
  bool isSmart;
  bool isDisjoint;
  bool forceOneShardAttributeValue;

  // misc
  std::vector<AttributeTypes::Numeric> directions;
  std::vector<AttributeTypes::String> edgeCollections;
  std::vector<AttributeTypes::String> vertexCollections;
  std::unordered_map<AttributeTypes::String, AttributeTypes::String> collectionToShard;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphNode& x) {
  return f.object(x).fields(
      // structs
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.field("graphDefinition", x.graphDefinition),  // GraphDefinition
      // main
      f.field("database", x.database),
      f.field("graph", x.graph),
      f.field("defaultDirection", x.defaultDirection),
      // ee bools
      f.field("isLocalGraphNode", x.isLocalGraphNode),
      f.field("isUsedAsSatellite", x.isUsedAsSatellite),
      f.field("isSmart", x.isSmart), f.field("isDisjoint", x.isDisjoint),
      f.field("forceOneShardAttributeValue", x.forceOneShardAttributeValue),
      // various
      f.field("directions", x.directions),
      f.field("edgeCollections", x.edgeCollections),
      f.field("vertexCollections", x.vertexCollections),
      f.field("collectionToShard", x.collectionToShard));
}
}  // namespace arangodb::aql::optimizer2::nodes