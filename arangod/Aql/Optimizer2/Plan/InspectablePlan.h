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
/// @author Heiko  Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Optimizer2/PlanNodes/EnumerateCollectionNode.h"
#include "Aql/Optimizer2/PlanNodes/EnumerateListNode.h"
#include "Aql/Optimizer2/PlanNodes/CalculationNode.h"
#include "Aql/Optimizer2/PlanNodes/CollectNode.h"
#include "Aql/Optimizer2/PlanNodes/FilterNode.h"
#include "Aql/Optimizer2/PlanNodes/LimitNode.h"
#include "Aql/Optimizer2/PlanNodes/ReturnNode.h"
#include "Aql/Optimizer2/PlanNodes/SingletonNode.h"

#include <vector>

namespace arangodb::aql::optimizer2::plan {

typedef std::variant<
    optimizer2::nodes::CalculationNode, optimizer2::nodes::CollectNode,
    optimizer2::nodes::EnumerateCollectionNode,
    optimizer2::nodes::EnumerateListNode, optimizer2::nodes::FilterNode,
    optimizer2::nodes::LimitNode, optimizer2::nodes::ReturnNode,
    optimizer2::nodes::SingletonNode>
    PlanNodeType;

typedef std::vector<PlanNodeType> PlanNodesVector;

struct InspectablePlan {
  PlanNodesVector _plan{};
  size_t _parseErrors = 0;

  void insert(PlanNodeType&& node) { _plan.emplace_back(node); };
  void increaseErrorCounter() { _parseErrors++; };
  bool success() {
    // In-development use to verify that we actually could transform
    // every ExecutionNode to a PlanNode. This will be > 0 in case errors
    // appeared. See terminal (LOG_DEVEL) for details then.
    return _parseErrors == 0 ? true : false;
  }

  size_t amountOfNodes() const { return _plan.size(); };
};

}  // namespace arangodb::aql::optimizer2::plan
