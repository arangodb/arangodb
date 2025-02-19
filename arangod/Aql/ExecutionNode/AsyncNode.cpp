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

#include "AsyncNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/AsyncExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

AsyncNode::AsyncNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

AsyncNode::AsyncNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

/// @brief doToVelocyPack, for AsyncNode
void AsyncNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> AsyncNode::createBlock(
    ExecutionEngine& engine) const {
  return std::make_unique<ExecutionBlockImpl<AsyncExecutor>>(&engine, this);
}

/// @brief estimateCost
CostEstimate AsyncNode::estimateCost() const {
  if (_dependencies.empty()) {
    // we should always have dependency as we need input here...
    TRI_ASSERT(false);
    return aql::CostEstimate::empty();
  }
  aql::CostEstimate estimate = _dependencies[0]->getCost();
  return estimate;
}

ExecutionNode::NodeType AsyncNode::getType() const { return ASYNC; }

size_t AsyncNode::getMemoryUsedBytes() const { return sizeof(*this); }

ExecutionNode* AsyncNode::clone(ExecutionPlan* plan,
                                bool withDependencies) const {
  return cloneHelper(std::make_unique<AsyncNode>(plan, _id), withDependencies);
}
