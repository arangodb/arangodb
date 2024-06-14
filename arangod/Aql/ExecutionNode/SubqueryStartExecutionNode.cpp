////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "SubqueryStartExecutionNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/SubqueryStartExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Iterator.h>

namespace arangodb::aql {

SubqueryStartNode::SubqueryStartNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base), _subqueryOutVariable(nullptr) {
  // On purpose exclude the _subqueryOutVariable
  // A query cannot be explained after nodes have been serialized and
  // deserialized
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

void SubqueryStartNode::doToVelocyPack(VPackBuilder& nodes,
                                       unsigned flags) const {
  // We need this for the Explainer
  if (_subqueryOutVariable != nullptr) {
    nodes.add(VPackValue("subqueryOutVariable"));
    _subqueryOutVariable->toVelocyPack(nodes);
  }
}

std::unique_ptr<ExecutionBlock> SubqueryStartNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});

  // On purpose exclude the _subqueryOutVariable
  return std::make_unique<ExecutionBlockImpl<SubqueryStartExecutor>>(
      &engine, this, registerInfos, std::move(registerInfos));
}

ExecutionNode* SubqueryStartNode::clone(ExecutionPlan* plan,
                                        bool withDependencies) const {
  // On purpose exclude the _subqueryOutVariable
  return cloneHelper(std::make_unique<SubqueryStartNode>(plan, _id, nullptr),
                     withDependencies);
}

bool SubqueryStartNode::isEqualTo(ExecutionNode const& other) const {
  // On purpose exclude the _subqueryOutVariable
  if (other.getType() != ExecutionNode::SUBQUERY_START) {
    return false;
  }
  return ExecutionNode::isEqualTo(other);
}

size_t SubqueryStartNode::getMemoryUsedBytes() const { return sizeof(*this); }

}  // namespace arangodb::aql
