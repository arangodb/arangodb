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
 * GraphOptions
 */

struct GraphOptions {
  AttributeTypes::Numeric parallelism;
  bool refactor;
  bool produceVertices;
  bool neighbors;
  bool producePathsVertices;
  bool producePathsEdges;
  bool producePathsWeights;

  AttributeTypes::Numeric maxProjections;
  AttributeTypes::Numeric minDepth;
  AttributeTypes::Numeric maxDepth;
  AttributeTypes::Numeric defaultWeight;

  AttributeTypes::String uniqueVertices;
  AttributeTypes::String uniqueEdges;
  AttributeTypes::String order;
  AttributeTypes::String weightAttribute;
  AttributeTypes::String type;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphOptions& x) {
  return f.object(x).fields(
      f.field("parallelism", x.parallelism), f.field("refactor", x.refactor),
      f.field("produceVertices", x.produceVertices),
      f.field("neighbors", x.neighbors),
      f.field("producePathsVertices", x.producePathsVertices),
      f.field("producePathsEdges", x.producePathsEdges),
      f.field("producePathsWeights", x.producePathsWeights),

      f.field("maxProjections", x.maxProjections),
      f.field("minDepth", x.minDepth), f.field("maxDepth", x.maxDepth),
      f.field("defaultWeight", x.defaultWeight),

      f.field("uniqueVertices", x.uniqueVertices),
      f.field("uniqueEdges", x.uniqueEdges), f.field("order", x.order),
      f.field("weightAttribute", x.weightAttribute), f.field("type", x.type));
}

/*
 * GraphIndex's inner used "Base"
 */

struct GraphIndexBase {
  AttributeTypes::String id;
  AttributeTypes::String type;
  AttributeTypes::String name;
  std::vector<AttributeTypes::String> fields;
  AttributeTypes::Numeric selectivityEstimate;
  bool unique;
  bool sparse;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphIndexBase& x) {
  return f.object(x).fields(
      f.field("id", x.id), f.field("type", x.type), f.field("name", x.name),
      f.field("fields", x.fields),
      f.field("selectivityEstimate", x.selectivityEstimate),
      f.field("unique", x.unique), f.field("sparse", x.sparse));
}

/*
 * GraphIndex
 */

struct GraphIndex {
  std::vector<GraphIndexBase> base;
  VPackBuilder levels;  // TODO
};

template<typename Inspector>
auto inspect(Inspector& f, GraphIndex& x) {
  return f.object(x).fields(f.field("base", x.base),
                            f.field("levels", x.levels));
}

/*
 * TraversalNode (MainNode)
 * (above structs used as inner helpers here)
 */

// TODO: Also implement general GraphNode + inheritance

struct TraversalNode : optimizer2::nodes::BaseNode,
                       optimizer2::nodes::CollectionAccessingNode {
  // inner structs
  GraphOptions options;
  GraphDefinition graphDefinition;
  GraphIndex graphIndex;

  // main
  AttributeTypes::String graph;
  AttributeTypes::Numeric defaultDirection;
  AttributeTypes::String vertexId;

  // ee bools
  bool isLocalGraphNode;
  bool isUsedAsSatellite;
  bool isSmart;
  bool isDisjoint;
  bool forceOneShardAttributeValue;

  // various
  std::vector<AttributeTypes::Numeric> directions;
  std::vector<AttributeTypes::String> edgeCollections;
  std::vector<AttributeTypes::String> vertexCollections;
  VPackBuilder collectionToShard;

  // conditions
  std::optional<optimizer2::types::Expression> condition;
  std::optional<std::vector<optimizer2::types::Expression>>
      globalEdgeConditions;
  std::optional<std::vector<optimizer2::types::Expression>>
      globalVertexConditions;
  std::optional<std::vector<optimizer2::types::Variable>> conditionVariables;
  optimizer2::types::Expression fromCondition;
  optimizer2::types::Expression toCondition;

  // originally uses "TraversalEdgeConditionBuilder"
  std::optional<std::map<std::string, optimizer2::types::Expression>>
      edgeConditions;
  // originally uses "AstNodes*"
  std::optional<std::map<std::string, optimizer2::types::Expression>>
      vertexConditions;

  // prune
  std::optional<optimizer2::types::Expression> expression;
  std::optional<std::vector<optimizer2::types::Variable>> pruneVariables;

  // filter
  // TODO HINT: It seems that we have "expression" overlapping between
  // postFilter and pruneExpression, see TraversalNode.cpp L488 & L505
  // std::optional<optimizer2::types::Expression> expression;
  std::optional<std::vector<optimizer2::types::Variable>> variables;

  // variables
  optimizer2::types::Variable tmpObjVariable;
  std::optional<optimizer2::types::Variable> inVariable;

  // nodes
  optimizer2::types::Expression tmpObjVarNode;
  optimizer2::types::Expression tmpIdNode;

  // => [v,e,p]
  optimizer2::types::Variable vertexOutVariable;
  optimizer2::types::Variable edgeOutVariable;
  optimizer2::types::Variable pathOutVariable;
};

template<typename Inspector>
auto inspect(Inspector& f, TraversalNode& x) {
  return f.object(x).fields(
      // structs
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.embedFields(
          static_cast<optimizer2::nodes::CollectionAccessingNode&>(x)),
      f.field("options", x.options),                  // GraphOptions
      f.field("graphDefinition", x.graphDefinition),  // GraphDefinition
      f.field("indexes", x.graphIndex),               // GraphIndex(es)
      // main
      f.field("graph", x.graph),
      f.field("defaultDirection", x.defaultDirection),
      f.field("vertexId", x.vertexId),
      // ee bools
      f.field("isLocalGraphNode", x.isLocalGraphNode),
      f.field("isUsedAsSatellite", x.isUsedAsSatellite),
      f.field("isSmart", x.isSmart), f.field("isDisjoint", x.isDisjoint),
      f.field("forceOneShardAttributeValue", x.forceOneShardAttributeValue),
      // various
      f.field("directions", x.directions),
      f.field("edgeCollections", x.edgeCollections),
      f.field("vertexCollections", x.vertexCollections),
      f.field("collectionToShard", x.collectionToShard),
      // conditions
      f.field("condition", x.condition),
      f.field("globalEdgeConditions", x.globalEdgeConditions),
      f.field("globalVertexConditions", x.globalVertexConditions),
      f.field("fromCondition", x.fromCondition),
      f.field("toCondition", x.toCondition),
      f.field("vertexConditions", x.vertexConditions),
      f.field("conditionVariables", x.conditionVariables),
      f.field("edgeConditions", x.edgeConditions),
      // prune
      f.field("expression", x.expression),
      f.field("pruneVariables", x.pruneVariables),
      // filter
      f.field("variables", x.variables),
      // variables
      f.field("tmpObjVariable", x.tmpObjVariable),
      f.field("inVariable", x.inVariable),
      // nodes
      f.field("tmpIdNode", x.tmpIdNode),
      f.field("tmpObjVarNode", x.tmpObjVarNode),
      // [v,e,p]
      f.field("vertexOutVariable", x.vertexOutVariable),
      f.field("edgeOutVariable", x.edgeOutVariable),
      f.field("pathOutVariable", x.pathOutVariable));
}

}  // namespace arangodb::aql::optimizer2::nodes
