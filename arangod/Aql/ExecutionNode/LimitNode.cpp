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

#include "LimitNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/LimitExecutor.h"
#include "Aql/RegisterPlan.h"

using namespace arangodb;
using namespace arangodb::aql;

LimitNode::LimitNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _offset(base.get("offset").getNumericValue<decltype(_offset)>()),
      _limit(base.get("limit").getNumericValue<decltype(_limit)>()),
      _fullCount(base.get("fullCount").getBoolean()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> LimitNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});

  auto executorInfos = LimitExecutorInfos(_offset, _limit, _fullCount);
  return std::make_unique<ExecutionBlockImpl<LimitExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

// @brief doToVelocyPack, for LimitNode
void LimitNode::doToVelocyPack(velocypack::Builder& nodes,
                               unsigned /*flags*/) const {
  nodes.add("offset", VPackValue(_offset));
  nodes.add("limit", VPackValue(_limit));
  nodes.add("fullCount", VPackValue(_fullCount));
}

/// @brief estimateCost
CostEstimate LimitNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();

  // arbitrary cost value for skipping a single document
  // skipping over a document is not fully free, because in the RocksDB
  // case, we need to move iterarors forward, invoke the comparator etc.
  double const skipCost = 0.000001;

  size_t estimatedNrItems = estimate.estimatedNrItems;
  if (estimatedNrItems >= _offset) {
    estimate.estimatedCost += _offset * skipCost;
    estimatedNrItems -= _offset;
  } else {
    estimate.estimatedCost += estimatedNrItems * skipCost;
    estimatedNrItems = 0;
  }
  estimate.estimatedNrItems = (std::min)(_limit, estimatedNrItems);
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

LimitNode::LimitNode(ExecutionPlan* plan, ExecutionNodeId id, size_t offset,
                     size_t limit)
    : ExecutionNode(plan, id),
      _offset(offset),
      _limit(limit),
      _fullCount(false) {}

ExecutionNode::NodeType LimitNode::getType() const { return LIMIT; }

size_t LimitNode::getMemoryUsedBytes() const { return sizeof(*this); }

ExecutionNode* LimitNode::clone(ExecutionPlan* plan,
                                bool withDependencies) const {
  auto c = std::make_unique<LimitNode>(plan, _id, _offset, _limit);

  if (_fullCount) {
    c->setFullCount();
  }

  return cloneHelper(std::move(c), withDependencies);
}

AsyncPrefetchEligibility LimitNode::canUseAsyncPrefetching() const noexcept {
  // LimitNodes do not support async prefetching.
  return AsyncPrefetchEligibility::kDisableForNodeAndDependencies;
}

void LimitNode::setFullCount(bool enable) { _fullCount = enable; }

bool LimitNode::fullCount() const noexcept { return _fullCount; }

size_t LimitNode::offset() const { return _offset; }

size_t LimitNode::limit() const { return _limit; }
