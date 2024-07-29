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

#include "EnumerateListNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/EnumerateListExecutor.h"
#include "Aql/Expression.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"

using namespace arangodb;
using namespace arangodb::aql;

EnumerateListNode::EnumerateListNode(ExecutionPlan* plan, ExecutionNodeId id,
                                     Variable const* inVariable,
                                     Variable const* outVariable)
    : ExecutionNode(plan, id),
      _inVariable(inVariable),
      _outVariable(outVariable) {
  TRI_ASSERT(_inVariable != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

EnumerateListNode::EnumerateListNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")),
      _outVariable(
          Variable::varFromVPack(plan->getAst(), base, "outVariable")) {
  if (VPackSlice p = base.get(StaticStrings::Filter); !p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    setFilter(std::make_unique<Expression>(ast, ast->createNode(p)));
  }
}

/// @brief doToVelocyPack, for EnumerateListNode
void EnumerateListNode::doToVelocyPack(velocypack::Builder& nodes,
                                       unsigned flags) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  if (_filter != nullptr) {
    nodes.add(VPackValue(StaticStrings::Filter));
    _filter->toVelocyPack(nodes, flags);
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> EnumerateListNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegisterId inputRegister = variableToRegisterId(_inVariable);
  RegisterId outRegister = variableToRegisterId(_outVariable);
  auto registerInfos =
      createRegisterInfos(RegIdSet{inputRegister}, RegIdSet{outRegister});

  std::vector<std::pair<VariableId, RegisterId>> varsToRegs;
  if (hasFilter()) {
    VarSet inVars;
    _filter->variables(inVars);

    for (auto const& var : inVars) {
      if (var->id != _outVariable->id) {
        auto regId = variableToRegisterId(var);
        varsToRegs.emplace_back(var->id, regId);
      }
    }
  }
  auto executorInfos = EnumerateListExecutorInfos(
      inputRegister, outRegister, engine.getQuery(), filter(), _outVariable->id,
      std::move(varsToRegs));
  return std::make_unique<ExecutionBlockImpl<EnumerateListExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateListNode::clone(ExecutionPlan* plan,
                                        bool withDependencies) const {
  auto c =
      std::make_unique<EnumerateListNode>(plan, _id, _inVariable, _outVariable);

  if (hasFilter()) {
    c->setFilter(_filter->clone(plan->getAst(), true));
  }
  return cloneHelper(std::move(c), withDependencies);
}

/// @brief the cost of an enumerate list node
CostEstimate EnumerateListNode::estimateCost() const {
  size_t length = estimateListLength(_plan, _inVariable);

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= length;
  estimate.estimatedCost +=
      estimate.estimatedNrItems * (hasFilter() ? 1.25 : 1.0);
  return estimate;
}

AsyncPrefetchEligibility EnumerateListNode::canUseAsyncPrefetching()
    const noexcept {
  return AsyncPrefetchEligibility::kEnableForNode;
}

ExecutionNode::NodeType EnumerateListNode::getType() const {
  return ENUMERATE_LIST;
}

size_t EnumerateListNode::getMemoryUsedBytes() const { return sizeof(*this); }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void EnumerateListNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
  if (hasFilter()) {
    filter()->replaceVariables(replacements);
  }
}

void EnumerateListNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t index) {
  if (hasFilter() && self != this) {
    filter()->replaceAttributeAccess(searchVariable, attribute,
                                     replaceVariable);
  }
}

void EnumerateListNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
    // need to unset the output variable that we produce ourselves,
    // otherwise the register planning runs into trouble. the register
    // planning's assumption is that all variables that are used in a
    // node must also be used later.
    vars.erase(outVariable());
  }
}

std::vector<Variable const*> EnumerateListNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

/// @brief remember the condition to execute for early filtering
void EnumerateListNode::setFilter(std::unique_ptr<Expression> filter) {
  _filter = std::move(filter);
}

Variable const* EnumerateListNode::inVariable() const { return _inVariable; }

Variable const* EnumerateListNode::outVariable() const { return _outVariable; }
