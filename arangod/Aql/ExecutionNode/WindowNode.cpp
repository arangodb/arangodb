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
////////////////////////////////////////////////////////////////////////////////

#include "WindowNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/WindowBounds.h"
#include "Aql/Executor/CountCollectExecutor.h"
#include "Aql/Executor/DistinctCollectExecutor.h"
#include "Aql/Executor/WindowExecutor.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <chrono>

namespace arangodb::aql {

WindowNode::WindowNode(ExecutionPlan* plan, ExecutionNodeId id,
                       WindowBounds&& b, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, id),
      _bounds(std::move(b)),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::WindowNode(ExecutionPlan* plan, arangodb::velocypack::Slice base,
                       WindowBounds&& b, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, base),
      _bounds(std::move(b)),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::~WindowNode() = default;

void WindowNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  if (_rangeVariable) {
    nodes.add(VPackValue("rangeVariable"));
    _rangeVariable->toVelocyPack(nodes);
  }

  // aggregate variables
  nodes.add(VPackValue("aggregates"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& aggregateVariable : _aggregateVariables) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("outVariable"));
      aggregateVariable.outVar->toVelocyPack(nodes);
      if (aggregateVariable.inVar) {
        nodes.add(VPackValue("inVariable"));
        aggregateVariable.inVar->toVelocyPack(nodes);
      }
      nodes.add("type", VPackValue(aggregateVariable.type));
    }
  }

  _bounds.toVelocyPack(nodes);
}

void WindowNode::calcAggregateRegisters(
    std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
    RegIdSet& readableInputRegisters,
    RegIdSet& writeableOutputRegisters) const {
  for (auto const& p : _aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    RegisterId outReg = outputRegister(p.outVar);
    TRI_ASSERT(outReg.isValid());

    RegisterId inReg = RegisterPlan::MaxRegisterId;
    if (Aggregator::requiresInput(p.type)) {
      inReg = inputRegister(p.inVar);
      TRI_ASSERT(inReg.isValid());
      readableInputRegisters.insert(inReg);
    }
    // else: no input variable required

    aggregateRegisters.emplace_back(std::make_pair(outReg, inReg));
    writeableOutputRegisters.insert((outReg));
  }
  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> WindowNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegIdSet readableInputRegisters;
  RegIdSet writeableOutputRegisters;

  RegisterId rangeRegister = RegisterPlan::MaxRegisterId;
  if (_rangeVariable != nullptr) {
    rangeRegister = inputRegister(_rangeVariable);
    TRI_ASSERT(rangeRegister.isValid());
    readableInputRegisters.insert(rangeRegister);
  }

  // calculate the aggregate registers
  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  calcAggregateRegisters(aggregateRegisters, readableInputRegisters,
                         writeableOutputRegisters);

  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writeableOutputRegisters));

  std::vector<std::string> aggregateTypes;
  std::transform(_aggregateVariables.begin(), _aggregateVariables.end(),
                 std::back_inserter(aggregateTypes),
                 [](auto const& it) { return it.type; });
  TRI_ASSERT(aggregateTypes.size() == _aggregateVariables.size());

  auto executorInfos = WindowExecutorInfos(
      _bounds, rangeRegister, std::move(aggregateTypes),
      std::move(aggregateRegisters), engine.getQuery().warnings(),
      &_plan->getAst()->query().vpackOptions(),
      engine.getQuery().resourceMonitor());

  if (_rangeVariable == nullptr && _bounds.unboundedPreceding()) {
    return std::make_unique<ExecutionBlockImpl<AccuWindowExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
  return std::make_unique<ExecutionBlockImpl<WindowExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* WindowNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  return cloneHelper(
      std::make_unique<WindowNode>(plan, _id, WindowBounds(_bounds),
                                   _rangeVariable, _aggregateVariables),
      withDependencies);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void WindowNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _rangeVariable = Variable::replace(_rangeVariable, replacements);
  for (auto& variable : _aggregateVariables) {
    variable.inVar = Variable::replace(variable.inVar, replacements);
  }
}

/// @brief getVariablesUsedHere, modifying the set in-place
void WindowNode::getVariablesUsedHere(VarSet& vars) const {
  if (_rangeVariable) {
    vars.emplace(_rangeVariable);
  }
  for (auto const& p : _aggregateVariables) {
    if (p.inVar) {
      vars.emplace(p.inVar);
    }
  }
}

void WindowNode::setAggregateVariables(
    std::vector<AggregateVarInfo> const& aggregateVariables) {
  _aggregateVariables = aggregateVariables;
}

/// @brief estimateCost
CostEstimate WindowNode::estimateCost() const {
  // we never return more rows than above
  CostEstimate estimate = _dependencies.at(0)->getCost();
  if (_rangeVariable == nullptr) {
    uint64_t numRows = 1;
    if (_bounds.unboundedPreceding()) {
      numRows += estimate.estimatedNrItems;
    } else {
      numRows += std::min<uint64_t>(estimate.estimatedNrItems,
                                    _bounds.numPrecedingRows());
    }
    numRows += _bounds.numFollowingRows();
    estimate.estimatedCost +=
        double(numRows) * double(numRows) * _aggregateVariables.size();
  } else {  // guestimate
    estimate.estimatedCost += 4 * _aggregateVariables.size();
  }
  return estimate;
}

ExecutionNode::NodeType WindowNode::getType() const {
  return ExecutionNode::WINDOW;
}

size_t WindowNode::getMemoryUsedBytes() const { return sizeof(*this); }

std::vector<Variable const*> WindowNode::getVariablesSetHere() const {
  std::vector<Variable const*> v;
  v.reserve(_aggregateVariables.size());
  for (auto const& p : _aggregateVariables) {
    v.emplace_back(p.outVar);
  }
  return v;
}

// does this WINDOW need to look at rows following the current one
bool WindowNode::needsFollowingRows() const {
  return _bounds.needsFollowingRows();
}

}  // namespace arangodb::aql
