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
/// @author  Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/PlanNodes/BaseNode.h"
#include "Aql/Optimizer2/PlanNodes/DocumentProducingNode.h"
#include "Aql/Optimizer2/PlanNodes/CollectionAcessingNode.h"

#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"

namespace arangodb::aql::optimizer2::nodes {

struct IndexOperatorOptions {
  bool allCoveredByOneIndex;
  bool sorted;
  bool ascending;
  bool reverse;
  bool evalFCalls;
  bool waitForSync;
};

template<typename Inspector>
auto inspect(Inspector& f, IndexOperatorOptions& x) {
  return f.object(x).fields(
      f.field("allCoveredByOneIndex", x.allCoveredByOneIndex),
      f.field("sorted", x.sorted), f.field("ascending", x.ascending),
      f.field("reverse", x.reverse), f.field("evalFCalls", x.evalFCalls),
      f.field("waitForSync", x.waitForSync));
};

struct IndexNode : optimizer2::nodes::BaseNode,
                   optimizer2::nodes::DocumentProducingNode,
                   optimizer2::nodes::CollectionAccessingNode,
                   IndexOperatorOptions {
  // optionals
  std::optional<optimizer2::types::Variable> outVariable;

  // TODO: Implement types
  std::optional<VPackBuilder> projections;
  std::optional<VPackBuilder> filterProjections;
  std::optional<VPackBuilder> indexes;
  std::optional<VPackBuilder> condition;

  // Boolean values
  bool needsGatherNodeSort;
  bool indexCoversProjections;
  AttributeTypes::Numeric limit;
  AttributeTypes::Numeric lookahead;

  // in case: "isLateMaterialized()"
  std::optional<optimizer2::types::Variable> outNonMaterializedDocId;
  std::optional<AttributeTypes::Numeric>
      indexIdOfVars;  // This is BaseType aka. DocumentId
  std::optional<std::vector<optimizer2::types::Variable>> indexValuesVars;
};

template<typename Inspector>
auto inspect(Inspector& f, IndexNode& x) {
  return f.object(x).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(x)),
      f.embedFields(static_cast<optimizer2::nodes::DocumentProducingNode&>(x)),
      f.embedFields(
          static_cast<optimizer2::nodes::CollectionAccessingNode&>(x)),
      f.embedFields(static_cast<IndexOperatorOptions&>(x)),
      // optionals
      f.field("outVariable", x.outVariable),
      // bools
      f.field("needsGatherNodeSort", x.needsGatherNodeSort),
      f.field("indexCoversProjections", x.indexCoversProjections),
      f.field("limit", x.limit), f.field("lookahead", x.lookahead),
      // TODO: Implement proper types
      f.field("indexes", x.indexes),
      // Optionals
      f.field("condition", x.condition),
      f.field("outNonMaterializedDocId", x.outNonMaterializedDocId),
      f.field("indexIdOfVars", x.indexIdOfVars),
      f.field("indexValuesVars", x.indexValuesVars));
}

}  // namespace arangodb::aql::optimizer2::nodes
