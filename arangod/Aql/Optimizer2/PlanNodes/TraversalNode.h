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

#include "Aql/Optimizer2/PlanNodes/GraphNode.h"
#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"
#include "Aql/Optimizer2/PlanNodeTypes/GraphOptions.h"

#include "Inspection/VPackWithErrorT.h"
#include "Aql/Optimizer2/PlanNodeTypes/PostFilter.h"

namespace arangodb::aql::optimizer2::nodes {
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
struct TraversalNode : optimizer2::nodes::GraphNode {
  // inner structs
  optimizer2::types::TraverserOptions options;
  //  GraphIndex graphIndex;

  // main
  std::optional<AttributeTypes::String> vertexId;

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

  std::optional<optimizer2::types::PostFilter> postFilter;

  // variables
  std::optional<optimizer2::types::Variable> inVariable;
  std::optional<optimizer2::types::Variable> pathOutVariable;

  // => [v,e,p]
  optimizer2::types::Variable pathOutVariable;
};

template<typename Inspector>
auto inspect(Inspector& f, TraversalNode& x) {
  return f.object(x).fields(
      // structs
      f.embedFields(static_cast<optimizer2::nodes::GraphNode&>(x)),
      f.field("options", x.options),
      //      f.field("indexes", x.graphIndex),
      // main
      f.field("vertexId", x.vertexId),
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
