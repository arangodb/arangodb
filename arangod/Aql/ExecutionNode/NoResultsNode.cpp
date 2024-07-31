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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "NoResultsNode.h"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/EmptyExecutorInfos.h"
#include "Aql/Executor/NoResultsExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Basics/VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::aql;

NoResultsNode::NoResultsNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

NoResultsNode::NoResultsNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

/// @brief doToVelocyPack, for NoResultsNode
void NoResultsNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> NoResultsNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});
  return std::make_unique<ExecutionBlockImpl<NoResultsExecutor>>(
      &engine, this, std::move(registerInfos), EmptyExecutorInfos{});
}

/// @brief estimateCost, the cost of a NoResults is nearly 0
CostEstimate NoResultsNode::estimateCost() const {
  // we have trigger cost estimation for parent nodes because this node could
  // be spliced into a subquery.
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems = 0;
  estimate.estimatedCost = 0.5;  // just to make it non-zero
  return estimate;
}

ExecutionNode::NodeType NoResultsNode::getType() const { return NORESULTS; }

ExecutionNode* NoResultsNode::clone(ExecutionPlan* plan,
                                    bool withDependencies) const {
  return cloneHelper(std::make_unique<NoResultsNode>(plan, _id),
                     withDependencies);
}

AsyncPrefetchEligibility NoResultsNode::canUseAsyncPrefetching()
    const noexcept {
  // NoResultsNodes could make use of async prefetching, but the gain
  // is too little to justify it.
  return AsyncPrefetchEligibility::kDisableForNode;
}

size_t NoResultsNode::getMemoryUsedBytes() const { return sizeof(*this); }
