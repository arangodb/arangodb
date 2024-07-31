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

#include "FilterNode.h"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/FilterExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

FilterNode::FilterNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

/// @brief doToVelocyPack, for FilterNode
void FilterNode::doToVelocyPack(velocypack::Builder& nodes,
                                unsigned /*flags*/) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> FilterNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegisterId inputRegister = variableToRegisterId(_inVariable);

  auto registerInfos = createRegisterInfos(RegIdSet{inputRegister}, {});
  auto executorInfos = FilterExecutorInfos(inputRegister);
  return std::make_unique<ExecutionBlockImpl<FilterExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* FilterNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  return cloneHelper(std::make_unique<FilterNode>(plan, _id, _inVariable),
                     withDependencies);
}

/// @brief estimateCost
CostEstimate FilterNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  // We are pessimistic here by not reducing the nrItems. However, in the
  // worst case the filter does not reduce the items at all. Furthermore,
  // no optimizer rule introduces FilterNodes, thus it is not important
  // that they appear to lower the costs. Note that contrary to this,
  // an IndexNode does lower the costs, it also has a better idea
  // to what extent the number of items is reduced. On the other hand it
  // is important that a FilterNode produces additional costs, otherwise
  // the rule throwing away a FilterNode that is already covered by an
  // IndexNode cannot reduce the costs.
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility FilterNode::canUseAsyncPrefetching() const noexcept {
  return AsyncPrefetchEligibility::kEnableForNode;
}

FilterNode::FilterNode(ExecutionPlan* plan, ExecutionNodeId id,
                       Variable const* inVariable)
    : ExecutionNode(plan, id), _inVariable(inVariable) {
  TRI_ASSERT(_inVariable != nullptr);
}

ExecutionNode::NodeType FilterNode::getType() const { return FILTER; }

size_t FilterNode::getMemoryUsedBytes() const { return sizeof(*this); }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void FilterNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void FilterNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
}

Variable const* FilterNode::inVariable() const { return _inVariable; }
