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
#include "Aql/SortedCollectExecutor.h"
#include "Aql/VariableGenerator.h"
#include "Aql/WalkerWorker.h"

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

CollectNode::~CollectNode() {}

/// @brief toVelocyPack, for CollectNode
void CollectNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

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
    TRI_ASSERT(collectRegister > 0 && collectRegister < ExecutionNode::MaxRegisterId);
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
    TRI_ASSERT(inReg < ExecutionNode::MaxRegisterId);
    TRI_ASSERT(outReg < ExecutionNode::MaxRegisterId);
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
    TRI_ASSERT(outReg < ExecutionNode::MaxRegisterId);

    RegisterId inReg = ExecutionNode::MaxRegisterId;
    if (Aggregator::requiresInput(p.second.second)) {
      auto itIn = getRegisterPlan()->varInfo.find(p.second.first->id);
      TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());
      inReg = itIn->second.registerId;
      TRI_ASSERT(inReg < ExecutionNode::MaxRegisterId);
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

      RegisterId collectRegister = ExecutionNode::MaxRegisterId;
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
          std::move(aggregateRegisters), trxPtr, _count);

      return std::make_unique<ExecutionBlockImpl<HashedCollectExecutor>>(&engine, this,
                                                                         std::move(infos));
    }
    case CollectOptions::CollectMethod::SORTED: {
      ExecutionNode const* previousNode = getFirstDependency();
      TRI_ASSERT(previousNode != nullptr);

      std::unordered_set<RegisterId> readableInputRegisters;
      std::unordered_set<RegisterId> writeableOutputRegisters;

      RegisterId collectRegister = ExecutionNode::MaxRegisterId;
      calcCollectRegister(collectRegister, writeableOutputRegisters);

      RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
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
          _expressionVariable, std::move(aggregateTypes),
          std::move(variables), std::move(aggregateRegisters), trxPtr, _count);

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
                                         std::move(groupRegisters), trxPtr);

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

/// @brief helper struct for finding variables
struct UserVarFinder final : public WalkerWorker<ExecutionNode> {
  explicit UserVarFinder(int mindepth) : mindepth(mindepth), depth(-1) {}

  ~UserVarFinder() {}

  std::vector<Variable const*> userVars;
  int mindepth;  // minimal depth to consider
  int depth;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  void after(ExecutionNode* en) override final {
    if (en->getType() == ExecutionNode::SINGLETON) {
      depth = 0;
    } else if (en->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
               en->getType() == ExecutionNode::INDEX ||
               en->getType() == ExecutionNode::ENUMERATE_LIST ||
               en->getType() == ExecutionNode::TRAVERSAL ||
               en->getType() == ExecutionNode::SHORTEST_PATH ||
               en->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW ||
               en->getType() == ExecutionNode::COLLECT) {
      depth += 1;
    }
    // Now depth is set correct for this node.
    if (depth >= mindepth) {
      for (auto const& v : en->getVariablesSetHere()) {
        if (v->isUserDefined()) {
          userVars.emplace_back(v);
        }
      }
    }
  }
};

/// @brief getVariablesUsedHere, modifying the set in-place
void CollectNode::getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const {
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
      UserVarFinder finder(1);
      auto myselfAsNonConst = const_cast<CollectNode*>(this);
      myselfAsNonConst->walk(finder);
      if (finder.depth == 1) {
        // we are top level, let's run again with mindepth = 0
        finder.userVars.clear();
        finder.mindepth = 0;
        finder.depth = -1;
        finder.reset();
        myselfAsNonConst->walk(finder);
      }
      for (auto& x : finder.userVars) {
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
