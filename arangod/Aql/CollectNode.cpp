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
#include "Aql/ExecutionPlan.h"
#include "Aql/WalkerWorker.h"

using namespace arangodb::aql;

CollectNode::CollectNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
    Variable const* expressionVariable, Variable const* outVariable,
    std::vector<Variable const*> const& keepVariables,
    std::unordered_map<VariableId, std::string const> const& variableMap,
    std::vector<std::pair<Variable const*, Variable const*>> const&
        groupVariables,
    std::vector<std::pair<Variable const*,
                          std::pair<Variable const*, std::string>>> const&
        aggregateVariables,
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
void CollectNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

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

/// @brief clone ExecutionNode recursively
ExecutionNode* CollectNode::clone(ExecutionPlan* plan, bool withDependencies,
                                  bool withProperties) const {
  auto outVariable = _outVariable;
  auto expressionVariable = _expressionVariable;
  auto groupVariables = _groupVariables;
  auto aggregateVariables = _aggregateVariables;

  if (withProperties) {
    if (expressionVariable != nullptr) {
      expressionVariable =
          plan->getAst()->variables()->createVariable(expressionVariable);
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

  auto c =
      new CollectNode(plan, _id, _options, groupVariables, aggregateVariables,
                      expressionVariable, outVariable, _keepVariables,
                      _variableMap, _count, _isDistinctCommand);

  // specialize the cloned node
  if (isSpecialized()) {
    c->specialized();
  }

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
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

/// @brief getVariablesUsedHere, returning a vector
std::vector<Variable const*> CollectNode::getVariablesUsedHere() const {
  std::unordered_set<Variable const*> v;
  // actual work is done by that method
  getVariablesUsedHere(v);

  // copy result into vector
  std::vector<Variable const*> vv;
  vv.insert(vv.begin(), v.begin(), v.end());
  return vv;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void CollectNode::getVariablesUsedHere(
    std::unordered_set<Variable const*>& vars) const {
  for (auto const& p : _groupVariables) {
    vars.emplace(p.second);
  }
  for (auto const& p : _aggregateVariables) {
    if (Aggregator::requiresInput(p.second.second)) {
      vars.emplace(p.second.first);
    }
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
      myselfAsNonConst->walk(&finder);
      if (finder.depth == 1) {
        // we are top level, let's run again with mindepth = 0
        finder.userVars.clear();
        finder.mindepth = 0;
        finder.depth = -1;
        finder.reset();
        myselfAsNonConst->walk(&finder);
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

/// @brief estimateCost
double CollectNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);

  // As in the FilterNode case, we are pessimistic here by not reducing the
  // nrItems much, since the worst case for COLLECT is to return as many items
  // as there are input items. In any case, we have to look at all incoming
  // items, and in particular in the COLLECT ... INTO ... case, we have
  // to actually hand on all data anyway, albeit not as separate items.
  // Nevertheless, the optimizer does not do much with CollectNodes
  // and thus this potential overestimation does not really matter.

  if (_count && _groupVariables.empty()) {
    // we are known to only produce a single output row
    nrItems = 1;
  } else {
    // we do not know how many rows the COLLECT with produce...
    // the worst case is that there will be as many output rows as input rows
    if (nrItems >= 10) {
      // we assume that the collect will reduce the number of results at least
      // somewhat
      nrItems = static_cast<size_t>(nrItems * 0.80);
    }
  }

  return depCost + nrItems;
}
