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

#include "ReturnNode.h"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Executor/ReturnExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Basics/VelocyPackHelper.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

ReturnNode::ReturnNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")),
      _count(basics::VelocyPackHelper::getBooleanValue(base, "count", false)) {}

/// @brief doToVelocyPack, for ReturnNode
void ReturnNode::doToVelocyPack(velocypack::Builder& nodes,
                                unsigned /*flags*/) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);
  nodes.add("count", VPackValue(_count));
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ReturnNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inputRegister = variableToRegisterId(_inVariable);

  // This is an important performance improvement:
  // If we have inherited results, we do move the block through
  // and do not modify it in any way.
  // In the other case it is important to shrink the matrix to exactly
  // one register that is stored within the DOCVEC.
  if (returnInheritedResults()) {
    auto executorInfos = IdExecutorInfos(_count, inputRegister);
    auto registerInfos = createRegisterInfos({}, {});
    return std::make_unique<ExecutionBlockImpl<
        IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    TRI_ASSERT(!returnInheritedResults());
    // TODO We should be able to remove this special case when the new
    //      register planning is ready.
    // The ReturnExecutor only writes to register 0.
    constexpr auto outputRegister = RegisterId{0};

    auto registerInfos =
        createRegisterInfos(RegIdSet{inputRegister}, RegIdSet{outputRegister});
    auto executorInfos = ReturnExecutorInfos(inputRegister, _count);

    return std::make_unique<ExecutionBlockImpl<ReturnExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

bool ReturnNode::returnInheritedResults() const {
  bool const isRoot = plan()->root() == this;
  return isRoot;
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ReturnNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  auto c = std::make_unique<ReturnNode>(plan, _id, _inVariable);

  if (_count) {
    c->setCount();
  }

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief estimateCost
CostEstimate ReturnNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility ReturnNode::canUseAsyncPrefetching() const noexcept {
  // cannot enable async prefetching for return nodes right now,
  // as it will produce assertion failures regarding the reference
  // count of the result block
  return AsyncPrefetchEligibility::kDisableForNode;
}

ReturnNode::ReturnNode(ExecutionPlan* plan, ExecutionNodeId id,
                       Variable const* inVariable)
    : ExecutionNode(plan, id), _inVariable(inVariable), _count(false) {
  TRI_ASSERT(_inVariable != nullptr);
}

ExecutionNode::NodeType ReturnNode::getType() const { return RETURN; }

size_t ReturnNode::getMemoryUsedBytes() const { return sizeof(*this); }

void ReturnNode::setCount() { _count = true; }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void ReturnNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void ReturnNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
}

Variable const* ReturnNode::inVariable() const { return _inVariable; }

void ReturnNode::inVariable(Variable const* v) { _inVariable = v; }
