////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "WindowNode.h"

#include "Aql/Ast.h"
#include "Aql/CountCollectExecutor.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortedCollectExecutor.h"
#include "Aql/VariableGenerator.h"
#include "Aql/WindowExecutor.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

WindowRange::WindowRange()
    : preceding(AqlValue(AqlValueHintInt(0))),
      following(AqlValue(AqlValueHintInt(0))) {}

WindowRange::~WindowRange() {
  preceding.destroy();
  following.destroy();
}

void WindowRange::toVelocyPack(VPackBuilder& b) const {
  b.add(VPackValue("preceding"));
  preceding.toVelocyPack(b.options, b, true, true);
  b.add(VPackValue("following"));
  following.toVelocyPack(b.options, b, true, true);
}

void WindowRange::fromVelocyPack(velocypack::Slice slice) {
  following = AqlValue(slice.get("following"));
  preceding = AqlValue(slice.get("preceding"));
}

WindowNode::WindowNode(ExecutionPlan* plan, ExecutionNodeId id,
                       WindowRange&& range, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, id),
      _range(range),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::WindowNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                       WindowRange&& range, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, base),
      _range(range),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::~WindowNode() = default;

/// @brief toVelocyPack, for CollectNode
void WindowNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                    std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

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
      nodes.add(VPackValue("inVariable"));
      aggregateVariable.inVar->toVelocyPack(nodes);
      nodes.add("type", VPackValue(aggregateVariable.type));
    }
  }

  _range.toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

void WindowNode::calcAggregateRegisters(std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
                                        RegIdSet& readableInputRegisters,
                                        RegIdSet& writeableOutputRegisters) const {
  for (auto const& p : _aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = getRegisterPlan()->varInfo.find(p.outVar->id);
    TRI_ASSERT(itOut != getRegisterPlan()->varInfo.end());
    RegisterId outReg = itOut->second.registerId;
    TRI_ASSERT(outReg < RegisterPlan::MaxRegisterId);

    RegisterId inReg = RegisterPlan::MaxRegisterId;
    if (Aggregator::requiresInput(p.type)) {
      auto itIn = getRegisterPlan()->varInfo.find(p.inVar->id);
      TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());
      inReg = itIn->second.registerId;
      TRI_ASSERT(inReg < RegisterPlan::MaxRegisterId);
      readableInputRegisters.insert(inReg);
    }
    // else: no input variable required

    aggregateRegisters.emplace_back(std::make_pair(outReg, inReg));
    writeableOutputRegisters.insert((outReg));
  }
  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());
}

void WindowNode::calcAggregateTypes(std::vector<std::unique_ptr<Aggregator>>& aggregateTypes) const {
  for (auto const& p : _aggregateVariables) {
    aggregateTypes.emplace_back(
        Aggregator::fromTypeString(&_plan->getAst()->query().vpackOptions(), p.type));
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> WindowNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegIdSet readableInputRegisters;
  RegIdSet writeableOutputRegisters;

  RegisterId rangeRegister = RegisterPlan::MaxRegisterId;
  if (_rangeVariable != nullptr) {
    auto itIn = getRegisterPlan()->varInfo.find(_rangeVariable->id);
    TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());
    rangeRegister = itIn->second.registerId;
    TRI_ASSERT(rangeRegister < RegisterPlan::MaxRegisterId);
    readableInputRegisters.insert(rangeRegister);
  }

  // calculate the aggregate registers
  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  calcAggregateRegisters(aggregateRegisters, readableInputRegisters, writeableOutputRegisters);

  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writeableOutputRegisters));

  std::vector<std::string> aggregateTypes;
  std::transform(aggregateVariables().begin(), aggregateVariables().end(),
                 std::back_inserter(aggregateTypes),
                 [](auto& it) { return it.type; });
  TRI_ASSERT(aggregateTypes.size() == _aggregateVariables.size());

  auto executorInfos =
      WindowExecutorInfos(_range, rangeRegister, std::move(aggregateTypes),
                          std::move(aggregateRegisters),
                          &_plan->getAst()->query().vpackOptions());

  return std::make_unique<ExecutionBlockImpl<WindowExecutor>>(&engine, this,
                                                              std::move(registerInfos),
                                                              std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* WindowNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto aggregateVariables = _aggregateVariables;

  if (withProperties) {
    // need to re-create all variables

    aggregateVariables.clear();

    for (auto& it : _aggregateVariables) {
      auto out = plan->getAst()->variables()->createVariable(it.outVar);
      auto in = plan->getAst()->variables()->createVariable(it.inVar);
      aggregateVariables.emplace_back(AggregateVarInfo{out, in, it.type});
    }
  }

  auto c = std::make_unique<WindowNode>(plan, _id, WindowRange(_range), _rangeVariable, aggregateVariables);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief getVariablesUsedHere, modifying the set in-place
void WindowNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto const& p : _aggregateVariables) {
    vars.emplace(p.inVar);
  }
}

void WindowNode::setAggregateVariables(std::vector<AggregateVarInfo> const& aggregateVariables) {
  _aggregateVariables = aggregateVariables;
}

/// @brief estimateCost
CostEstimate WindowNode::estimateCost() const {
  // FIXME fix
  return _dependencies.at(0)->getCost();
}

ExecutionNode::NodeType WindowNode::getType() const {
  return ExecutionNode::WINDOW;
}

void WindowNode::clearAggregates(std::function<bool(AggregateVarInfo const&)> cb) {
  for (auto it = _aggregateVariables.begin(); it != _aggregateVariables.end();
       /* no hoisting */) {
    if (cb(*it)) {
      it = _aggregateVariables.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<AggregateVarInfo> const& WindowNode::aggregateVariables() const {
  return _aggregateVariables;
}

std::vector<AggregateVarInfo>& WindowNode::aggregateVariables() {
  return _aggregateVariables;
}

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
  if (_range.following.isNumber()) {
    return _range.following.toInt64() > 0;
  }
  return true;
}
