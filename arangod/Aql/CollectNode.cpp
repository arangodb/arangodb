////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CollectNode.h"

#include "Aql/Ast.h"
#include "Aql/CountCollectExecutor.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortedCollectExecutor.h"
#include "Aql/VariableGenerator.h"
#include "Aql/WalkerWorker.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

CollectNode::CollectNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
    Variable const* expressionVariable, Variable const* outVariable,
    std::vector<Variable const*> const& keepVariables,
    std::unordered_map<VariableId, std::string const> const& variableMap,
    std::vector<std::pair<Variable const*, Variable const*>> const& groupVariables,
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
    bool count, bool isDistinctCommand)
    : ExecutionNode(plan, base),
      _options(base),
      _groupVariables(groupVariables),
      _aggregateVariables(aggregateVariables),
      _expressionVariable(expressionVariable),
      _outVariable(outVariable),
      _keepVariables(keepVariables),
      _variableMap(variableMap),
      _count(count),
      _isDistinctCommand(isDistinctCommand),
      _specialized(false) {}

CollectNode::~CollectNode() = default;

/// @brief toVelocyPack, for CollectNode
void CollectNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                     std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  // group variables
  nodes.add(VPackValue("groups"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& groupVariable : _groupVariables) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("outVariable"));
      groupVariable.first->toVelocyPack(nodes);
      nodes.add(VPackValue("inVariable"));
      groupVariable.second->toVelocyPack(nodes);
    }
  }

  // aggregate variables
  nodes.add(VPackValue("aggregates"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& aggregateVariable : _aggregateVariables) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("outVariable"));
      aggregateVariable.first->toVelocyPack(nodes);
      nodes.add(VPackValue("inVariable"));
      aggregateVariable.second.first->toVelocyPack(nodes);
      nodes.add("type", VPackValue(aggregateVariable.second.second));
    }
  }

  // expression variable might be empty
  if (_expressionVariable != nullptr) {
    nodes.add(VPackValue("expressionVariable"));
    _expressionVariable->toVelocyPack(nodes);
  }

  // output variable might be empty
  if (_outVariable != nullptr) {
    nodes.add(VPackValue("outVariable"));
    _outVariable->toVelocyPack(nodes);
  }

  if (!_keepVariables.empty()) {
    nodes.add(VPackValue("keepVariables"));
    {
      VPackArrayBuilder guard(&nodes);
      for (auto const& it : _keepVariables) {
        VPackObjectBuilder obj(&nodes);
        nodes.add(VPackValue("variable"));
        it->toVelocyPack(nodes);
      }
    }
  }

  nodes.add("count", VPackValue(_count));
  nodes.add("isDistinctCommand", VPackValue(_isDistinctCommand));
  nodes.add("specialized", VPackValue(_specialized));
  nodes.add(VPackValue("collectOptions"));
  _options.toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

void CollectNode::calcExpressionRegister(
    arangodb::aql::RegisterId& expressionRegister,
    std::unordered_set<arangodb::aql::RegisterId>& readableInputRegisters) const {
  if (_expressionVariable != nullptr) {
    auto it = getRegisterPlan()->varInfo.find(_expressionVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    expressionRegister = (*it).second.registerId;
    readableInputRegisters.insert((*it).second.registerId);
  }
}

void CollectNode::calcCollectRegister(arangodb::aql::RegisterId& collectRegister,
                                      std::unordered_set<arangodb::aql::RegisterId>& writeableOutputRegisters) const {
  if (_outVariable != nullptr) {
    auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    collectRegister = (*it).second.registerId;
    TRI_ASSERT(collectRegister > 0 && collectRegister < RegisterPlan::MaxRegisterId);
    writeableOutputRegisters.insert((*it).second.registerId);
  }
}

void CollectNode::calcGroupRegisters(
    std::vector<std::pair<arangodb::aql::RegisterId, arangodb::aql::RegisterId>>& groupRegisters,
    std::unordered_set<arangodb::aql::RegisterId>& readableInputRegisters,
    std::unordered_set<arangodb::aql::RegisterId>& writeableOutputRegisters) const {
  for (auto const& p : _groupVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != getRegisterPlan()->varInfo.end());

    auto itIn = getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());

    RegisterId inReg = itIn->second.registerId;
    RegisterId outReg = itOut->second.registerId;
    TRI_ASSERT(inReg < RegisterPlan::MaxRegisterId);
    TRI_ASSERT(outReg < RegisterPlan::MaxRegisterId);
    groupRegisters.emplace_back(outReg, inReg);
    writeableOutputRegisters.insert(outReg);
    readableInputRegisters.insert(inReg);
  }
}

void CollectNode::calcAggregateRegisters(
    std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
    std::unordered_set<arangodb::aql::RegisterId>& readableInputRegisters,
    std::unordered_set<arangodb::aql::RegisterId>& writeableOutputRegisters) const {
  for (auto const& p : _aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != getRegisterPlan()->varInfo.end());
    RegisterId outReg = itOut->second.registerId;
    TRI_ASSERT(outReg < RegisterPlan::MaxRegisterId);

    RegisterId inReg = RegisterPlan::MaxRegisterId;
    if (Aggregator::requiresInput(p.second.second)) {
      auto itIn = getRegisterPlan()->varInfo.find(p.second.first->id);
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

void CollectNode::calcAggregateTypes(std::vector<std::unique_ptr<Aggregator>>& aggregateTypes) const {
  for (auto const& p : _aggregateVariables) {
    aggregateTypes.emplace_back(
        Aggregator::fromTypeString(_plan->getAst()->query()->trx(), p.second.second));
  }
}

void CollectNode::calcVariableNames(std::vector<std::pair<std::string, RegisterId>>& variableNames) const {
  if (_outVariable != nullptr) {
    auto const& registerPlan = getRegisterPlan()->varInfo;
    auto it = registerPlan.find(_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());

    // iterate over all our variables
    if (_keepVariables.empty()) {
      auto usedVariableIds(getVariableIdsUsedHere());

      for (auto const& vi : registerPlan) {
        if (vi.second.depth > 0 || getDepth() == 1) {
          // Do not keep variables from depth 0, unless we are depth 1 ourselves
          // (which means no FOR in which we are contained)

          if (usedVariableIds.find(vi.first) == usedVariableIds.end()) {
            // variable is not visible to the CollectBlock
            continue;
          }

          // find variable in the global variable map
          auto itVar = _variableMap.find(vi.first);

          if (itVar != _variableMap.end()) {
            variableNames.emplace_back(std::make_pair((*itVar).second, vi.second.registerId));
          }
        }
      }
    } else {
      for (auto const& x : _keepVariables) {
        auto it = registerPlan.find(x->id);

        if (it != registerPlan.end()) {
          variableNames.emplace_back(std::make_pair(x->name, (*it).second.registerId));
        }
      }
    }
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> CollectNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  switch (aggregationMethod()) {
    case CollectOptions::CollectMethod::HASH: {
      ExecutionNode const* previousNode = getFirstDependency();
      TRI_ASSERT(previousNode != nullptr);

      std::unordered_set<RegisterId> readableInputRegisters;
      std::unordered_set<RegisterId> writeableOutputRegisters;

      RegisterId collectRegister = RegisterPlan::MaxRegisterId;
      calcCollectRegister(collectRegister, writeableOutputRegisters);

      // calculate the group registers
      std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
      calcGroupRegisters(groupRegisters, readableInputRegisters, writeableOutputRegisters);

      // calculate the aggregate registers
      std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
      calcAggregateRegisters(aggregateRegisters, readableInputRegisters, writeableOutputRegisters);

      TRI_ASSERT(groupRegisters.size() == _groupVariables.size());
      TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());

      std::vector<std::string> aggregateTypes;
      std::transform(aggregateVariables().begin(), aggregateVariables().end(),
                     std::back_inserter(aggregateTypes),
                     [](auto& it) { return it.second.second; });
      TRI_ASSERT(aggregateTypes.size() == _aggregateVariables.size());

      transaction::Methods* trxPtr = _plan->getAst()->query()->trx();
      HashedCollectExecutorInfos infos(
          getRegisterPlan()->nrRegs[previousNode->getDepth()],
          getRegisterPlan()->nrRegs[getDepth()], getRegsToClear(), calcRegsToKeep(),
          std::move(readableInputRegisters), std::move(writeableOutputRegisters),
          std::move(groupRegisters), collectRegister, std::move(aggregateTypes),
          std::move(aggregateRegisters), trxPtr, _plan->getAst()->query()->resourceMonitor(),
          _count);

      return std::make_unique<ExecutionBlockImpl<HashedCollectExecutor>>(&engine, this,
                                                                         std::move(infos));
    }
    case CollectOptions::CollectMethod::SORTED: {
      ExecutionNode const* previousNode = getFirstDependency();
      TRI_ASSERT(previousNode != nullptr);

      std::unordered_set<RegisterId> readableInputRegisters;
      std::unordered_set<RegisterId> writeableOutputRegisters;

      RegisterId collectRegister = RegisterPlan::MaxRegisterId;
      calcCollectRegister(collectRegister, writeableOutputRegisters);

      RegisterId expressionRegister = RegisterPlan::MaxRegisterId;
      calcExpressionRegister(expressionRegister, readableInputRegisters);

      // calculate the group registers
      std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
      calcGroupRegisters(groupRegisters, readableInputRegisters, writeableOutputRegisters);

      // calculate the aggregate registers
      std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
      calcAggregateRegisters(aggregateRegisters, readableInputRegisters, writeableOutputRegisters);

      // calculate the aggregate type // TODO refactor nicely
      std::vector<std::unique_ptr<Aggregator>> aggregateValues;
      calcAggregateTypes(aggregateValues);

      // calculate the variable names
      std::vector<std::pair<std::string, RegisterId>> variables;
      calcVariableNames(variables);

      TRI_ASSERT(groupRegisters.size() == _groupVariables.size());
      TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());

      std::vector<std::string> aggregateTypes;
      std::transform(aggregateVariables().begin(), aggregateVariables().end(),
                     std::back_inserter(aggregateTypes),
                     [](auto& it) { return it.second.second; });
      TRI_ASSERT(aggregateTypes.size() == _aggregateVariables.size());

      transaction::Methods* trxPtr = _plan->getAst()->query()->trx();
      SortedCollectExecutorInfos infos(
          getRegisterPlan()->nrRegs[previousNode->getDepth()],
          getRegisterPlan()->nrRegs[getDepth()], getRegsToClear(), calcRegsToKeep(),
          std::move(readableInputRegisters), std::move(writeableOutputRegisters),
          std::move(groupRegisters), collectRegister, expressionRegister,
          _expressionVariable, std::move(aggregateTypes), std::move(variables),
          std::move(aggregateRegisters), trxPtr, _count);

      return std::make_unique<ExecutionBlockImpl<SortedCollectExecutor>>(&engine, this,
                                                                         std::move(infos));
    }
    case CollectOptions::CollectMethod::COUNT: {
      ExecutionNode const* previousNode = getFirstDependency();
      TRI_ASSERT(previousNode != nullptr);

      auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
      TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
      RegisterId collectRegister = (*it).second.registerId;

      CountCollectExecutorInfos infos(collectRegister,
                                      getRegisterPlan()->nrRegs[previousNode->getDepth()],
                                      getRegisterPlan()->nrRegs[getDepth()],
                                      getRegsToClear(), calcRegsToKeep());

      return std::make_unique<ExecutionBlockImpl<CountCollectExecutor>>(&engine, this,
                                                                        std::move(infos));
    }
    case CollectOptions::CollectMethod::DISTINCT: {
      ExecutionNode const* previousNode = getFirstDependency();
      TRI_ASSERT(previousNode != nullptr);

      std::unordered_set<RegisterId> readableInputRegisters;
      std::unordered_set<RegisterId> writeableOutputRegisters;

      std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
      // calculate the group registers
      calcGroupRegisters(groupRegisters, readableInputRegisters, writeableOutputRegisters);

      transaction::Methods* trxPtr = _plan->getAst()->query()->trx();
      DistinctCollectExecutorInfos infos(getRegisterPlan()->nrRegs[previousNode->getDepth()],
                                         getRegisterPlan()->nrRegs[getDepth()],
                                         getRegsToClear(), calcRegsToKeep(),
                                         std::move(readableInputRegisters),
                                         std::move(writeableOutputRegisters),
                                         std::move(groupRegisters), trxPtr,
                                         _plan->getAst()->query()->resourceMonitor());

      return std::make_unique<ExecutionBlockImpl<DistinctCollectExecutor>>(&engine, this,
                                                                           std::move(infos));
    }
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot instantiate CollectBlock with "
                                     "undetermined aggregation method");
  }
}

/// @brief clone ExecutionNode recursively
ExecutionNode* CollectNode::clone(ExecutionPlan* plan, bool withDependencies,
                                  bool withProperties) const {
  auto outVariable = _outVariable;
  auto expressionVariable = _expressionVariable;
  auto groupVariables = _groupVariables;
  auto aggregateVariables = _aggregateVariables;

  if (withProperties) {
    if (expressionVariable != nullptr) {
      expressionVariable = plan->getAst()->variables()->createVariable(expressionVariable);
    }

    if (outVariable != nullptr) {
      outVariable = plan->getAst()->variables()->createVariable(outVariable);
    }

    // need to re-create all variables
    groupVariables.clear();

    for (auto& it : _groupVariables) {
      auto out = plan->getAst()->variables()->createVariable(it.first);
      auto in = plan->getAst()->variables()->createVariable(it.second);
      groupVariables.emplace_back(std::make_pair(out, in));
    }

    aggregateVariables.clear();

    for (auto& it : _aggregateVariables) {
      auto out = plan->getAst()->variables()->createVariable(it.first);
      auto in = plan->getAst()->variables()->createVariable(it.second.first);
      aggregateVariables.emplace_back(
          std::make_pair(out, std::make_pair(in, it.second.second)));
    }
  }

  auto c = std::make_unique<CollectNode>(plan, _id, _options, groupVariables,
                                         aggregateVariables, expressionVariable,
                                         outVariable, _keepVariables, _variableMap,
                                         _count, _isDistinctCommand);

  // specialize the cloned node
  if (isSpecialized()) {
    c->specialized();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

auto isStartNode(ExecutionNode const& node) -> bool {
  switch (node.getType()) {
    case ExecutionNode::SINGLETON:
    case ExecutionNode::SUBQUERY_START:
      return true;
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::CALCULATION:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::SORT:
    case ExecutionNode::COLLECT:
    case ExecutionNode::SCATTER:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::RETURN:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::UPSERT:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::INDEX:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::MATERIALIZE:
      return false;
    case ExecutionNode::MAX_NODE_TYPE_VALUE:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
}

auto isVariableInvalidatingNode(ExecutionNode const& node) -> bool {
  switch (node.getType()) {
    case ExecutionNode::SINGLETON:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::COLLECT:
      return true;
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::CALCULATION:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::SORT:
    case ExecutionNode::SCATTER:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::RETURN:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::UPSERT:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::INDEX:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::MATERIALIZE:
      return false;
    case ExecutionNode::MAX_NODE_TYPE_VALUE:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
}

auto isLoop(ExecutionNode const& node) -> bool {
  switch (node.getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::COLLECT:
      return true;
    case ExecutionNode::SINGLETON:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::CALCULATION:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::SORT:
    case ExecutionNode::SCATTER:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::RETURN:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::UPSERT:
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::MATERIALIZE:
      return false;
    case ExecutionNode::MAX_NODE_TYPE_VALUE:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
}

// Get all variables that should be collected "INTO" the group variable.
// Returns whether we are at the top level.
// Gets passed whether we did encounter a loop "on the way" from the collect node.
auto getGroupVariables(ExecutionNode const& node, std::vector<Variable const*>& groupVariables,
                       bool const encounteredLoop = false, int const subqueryDepth = 0) -> bool {
  TRI_ASSERT(subqueryDepth >= 0);
  auto const recSubqueryDepth = [&]() {
    if (node.getType() == ExecutionNode::SUBQUERY_END) {
      return subqueryDepth + 1;
    } else if (node.getType() == ExecutionNode::SUBQUERY_START) {
      return subqueryDepth - 1;
    }
    return subqueryDepth;
  }();

  auto const dep = node.getFirstDependency();

  // Skip nodes inside a subquery, except for SUBQUERY_END!
  if (subqueryDepth > 0) {
    if (dep != nullptr) {
      return getGroupVariables(*dep, groupVariables, encounteredLoop, recSubqueryDepth);
    } else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                     "Unexpected end of plan inside subquery");
    }
  }

  bool const depIsTopLevel = [&]() {
    // Abort recursion on invalidating nodes
    if (dep != nullptr && !isVariableInvalidatingNode(node)) {
      return getGroupVariables(*dep, groupVariables,
                               encounteredLoop || isLoop(node), recSubqueryDepth);
    } else {
      return isStartNode(node);
    }
  }();

  // when we encounter a loop, we're no longer on the top level.
  bool const isTopLevel = depIsTopLevel && !isLoop(node);

  // top level variables aren't added, unless the collect node itself is on the
  // top level, which is true when there aren't any loops on the way.
  bool const addVariables = !isTopLevel || !encounteredLoop;

  if (addVariables) {
    // Add all variables of the current node
    for (auto const& v : node.getVariablesSetHere()) {
      if (v->isUserDefined()) {
        groupVariables.emplace_back(v);
      }
    }
  }

  return isTopLevel;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void CollectNode::getVariablesUsedHere(::arangodb::containers::HashSet<Variable const*>& vars) const {
  for (auto const& p : _groupVariables) {
    vars.emplace(p.second);
  }
  for (auto const& p : _aggregateVariables) {
    vars.emplace(p.second.first);
  }

  if (_expressionVariable != nullptr) {
    vars.emplace(_expressionVariable);
  }

  if (_outVariable != nullptr && !_count) {
    if (_keepVariables.empty()) {
      // Here we have to find all user defined variables in this query
      // amongst our dependencies:

      auto const dep = getFirstDependency();
      TRI_ASSERT(dep != nullptr);
      std::vector<Variable const*> userVars;
      getGroupVariables(*dep, userVars);

      for (auto& x : userVars) {
        vars.emplace(x);
      }
    } else {
      for (auto& x : _keepVariables) {
        vars.emplace(x);
      }
    }
  }
}

void CollectNode::setAggregateVariables(
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables) {
  _aggregateVariables = aggregateVariables;
}

/// @brief estimateCost
CostEstimate CollectNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();

  // As in the FilterNode case, we are pessimistic here by not reducing the
  // nrItems much, since the worst case for COLLECT is to return as many items
  // as there are input items. In any case, we have to look at all incoming
  // items, and in particular in the COLLECT ... INTO ... case, we have
  // to actually hand on all data anyway, albeit not as separate items.
  // Nevertheless, the optimizer does not do much with CollectNodes
  // and thus this potential overestimation does not really matter.

  if (_groupVariables.empty()) {
    // we are known to only produce a single output row
    estimate.estimatedNrItems = 1;
  } else {
    // we do not know how many rows the COLLECT with produce...
    // the worst case is that there will be as many output rows as input rows
    if (estimate.estimatedNrItems >= 10) {
      // we assume that the collect will reduce the number of results at least
      // somewhat
      estimate.estimatedNrItems = static_cast<size_t>(estimate.estimatedNrItems * 0.8);
    }
  }
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

CollectNode::CollectNode(
    ExecutionPlan* plan, size_t id, CollectOptions const& options,
    std::vector<std::pair<Variable const*, Variable const*>> const& groupVariables,
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
    Variable const* expressionVariable, Variable const* outVariable,
    std::vector<Variable const*> const& keepVariables,
    std::unordered_map<VariableId, std::string const> const& variableMap,
    bool count, bool isDistinctCommand)
    : ExecutionNode(plan, id),
      _options(options),
      _groupVariables(groupVariables),
      _aggregateVariables(aggregateVariables),
      _expressionVariable(expressionVariable),
      _outVariable(outVariable),
      _keepVariables(keepVariables),
      _variableMap(variableMap),
      _count(count),
      _isDistinctCommand(isDistinctCommand),
      _specialized(false) {
  // outVariable can be a nullptr, but only if _count is not set
  TRI_ASSERT(!_count || _outVariable != nullptr);
}

ExecutionNode::NodeType CollectNode::getType() const { return COLLECT; }

bool CollectNode::isDistinctCommand() const { return _isDistinctCommand; }

bool CollectNode::isSpecialized() const { return _specialized; }

void CollectNode::specialized() { _specialized = true; }

CollectOptions::CollectMethod CollectNode::aggregationMethod() const {
  return _options.method;
}

void CollectNode::aggregationMethod(CollectOptions::CollectMethod method) {
  _options.method = method;
}

CollectOptions& CollectNode::getOptions() { return _options; }

bool CollectNode::count() const { return _count; }

void CollectNode::count(bool value) { _count = value; }

bool CollectNode::hasOutVariableButNoCount() const {
  return (_outVariable != nullptr && !_count);
}

bool CollectNode::hasOutVariable() const { return _outVariable != nullptr; }

Variable const* CollectNode::outVariable() const { return _outVariable; }

void CollectNode::clearOutVariable() {
  TRI_ASSERT(_outVariable != nullptr);
  _outVariable = nullptr;
  _count = false;
}

void CollectNode::clearAggregates(
    std::function<bool(std::pair<Variable const*, std::pair<Variable const*, std::string>> const&)> cb) {
  for (auto it = _aggregateVariables.begin(); it != _aggregateVariables.end();
       /* no hoisting */) {
    if (cb(*it)) {
      it = _aggregateVariables.erase(it);
    } else {
      ++it;
    }
  }
}

bool CollectNode::hasExpressionVariable() const {
  return _expressionVariable != nullptr;
}

void CollectNode::expressionVariable(Variable const* variable) {
  TRI_ASSERT(!hasExpressionVariable());
  _expressionVariable = variable;
}

bool CollectNode::hasKeepVariables() const { return !_keepVariables.empty(); }

std::vector<Variable const*> const& CollectNode::keepVariables() const {
  return _keepVariables;
}

void CollectNode::setKeepVariables(std::vector<Variable const*>&& variables) {
  _keepVariables = std::move(variables);
}

std::unordered_map<VariableId, std::string const> const& CollectNode::variableMap() const {
  return _variableMap;
}

std::vector<std::pair<Variable const*, Variable const*>> const& CollectNode::groupVariables() const {
  return _groupVariables;
}

void CollectNode::groupVariables(std::vector<std::pair<Variable const*, Variable const*>> const& vars) {
  _groupVariables = vars;
}

std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const&
CollectNode::aggregateVariables() const {
  return _aggregateVariables;
}

std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>>& CollectNode::aggregateVariables() {
  return _aggregateVariables;
}

std::vector<Variable const*> CollectNode::getVariablesSetHere() const {
  std::vector<Variable const*> v;
  v.reserve(_groupVariables.size() + _aggregateVariables.size() +
            (_outVariable == nullptr ? 0 : 1));

  for (auto const& p : _groupVariables) {
    v.emplace_back(p.first);
  }
  for (auto const& p : _aggregateVariables) {
    v.emplace_back(p.first);
  }
  if (_outVariable != nullptr) {
    v.emplace_back(_outVariable);
  }
  return v;
}
