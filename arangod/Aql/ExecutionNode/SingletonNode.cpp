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

#include "SingletonNode.h"

#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/RegisterPlan.h"

using namespace arangodb;
using namespace arangodb::aql;

SingletonNode::SingletonNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

SingletonNode::SingletonNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SingletonNode::createBlock(
    ExecutionEngine& engine) const {
  auto registerInfos = createRegisterInfos({}, {});
  IdExecutorInfos infos(false);

  auto res = std::make_unique<ExecutionBlockImpl<IdExecutor<ConstFetcher>>>(
      &engine, this, std::move(registerInfos), std::move(infos));
  std::ignore =
      res->initializeCursor(InputAqlItemRow{CreateInvalidInputRowHint{}});
  return res;
}

/// @brief clone ExecutionNode recursively
ExecutionNode* SingletonNode::clone(ExecutionPlan* plan,
                                    bool withDependencies) const {
  return cloneHelper(std::make_unique<SingletonNode>(plan, _id),
                     withDependencies);
}

/// @brief doToVelocyPack, for SingletonNode
void SingletonNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief the cost of a singleton is 1, it produces one item only
CostEstimate SingletonNode::estimateCost() const {
  CostEstimate estimate = CostEstimate::empty();
  estimate.estimatedNrItems = 1;
  estimate.estimatedCost = 1.0;
  return estimate;
}

AsyncPrefetchEligibility SingletonNode::canUseAsyncPrefetching()
    const noexcept {
  // a singleton node has no predecessors. so prefetching isn't
  // necessary.
  return AsyncPrefetchEligibility::kDisableForNode;
}

ExecutionNode::NodeType SingletonNode::getType() const { return SINGLETON; }

size_t SingletonNode::getMemoryUsedBytes() const { return sizeof(*this); }
