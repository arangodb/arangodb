////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/SubqueryStartExecutionNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SubqueryStartExecutor.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryStartNode::SubqueryStartNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base), _subqueryOutVariable(nullptr) {
  // On purpose exclude the _subqueryOutVariable
  // A query cannot be explained after nodes have been serialized and deserialized
}

CostEstimate SubqueryStartNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  CostEstimate estimate = _dependencies.at(0)->getCost();

  // Save the nrItems going into the subquery to restore later at the
  // corresponding SubqueryEndNode.
  estimate.saveEstimatedNrItems();

  estimate.estimatedCost += estimate.estimatedNrItems;

  return estimate;
}

void SubqueryStartNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                           std::unordered_set<ExecutionNode const*>& seen) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);
  // We need this for the Explainer
  if (_subqueryOutVariable != nullptr) {
    nodes.add(VPackValue("subqueryOutVariable"));
    _subqueryOutVariable->toVelocyPack(nodes);
  }
  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryStartNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>();

  auto registerInfos = createRegisterInfos({}, {});

  // On purpose exclude the _subqueryOutVariable
  return std::make_unique<ExecutionBlockImpl<SubqueryStartExecutor>>(&engine, this, registerInfos,
                                                                     RegisterInfos{registerInfos});
}

ExecutionNode* SubqueryStartNode::clone(ExecutionPlan* plan, bool withDependencies,
                                        bool withProperties) const {
  // On purpose exclude the _subqueryOutVariable
  auto c = std::make_unique<SubqueryStartNode>(plan, _id, nullptr);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

bool SubqueryStartNode::isEqualTo(ExecutionNode const& other) const {
  // On purpose exclude the _subqueryOutVariable
  if (other.getType() != ExecutionNode::SUBQUERY_START) {
    return false;
  }
  return ExecutionNode::isEqualTo(other);
}

}  // namespace aql
}  // namespace arangodb
