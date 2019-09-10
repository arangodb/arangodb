////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/NodeFinder.h"
#include "Aql/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/SubqueryEndExecutionNode.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryEndNode::SubqueryEndNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _subquery(nullptr),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

/// @brief toVelocyPack, for SubqueryEndNode
void SubqueryEndNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add(VPackValue("subquery"));
  _subquery->toVelocyPack(nodes, flags, /*keepTopLevelOpen*/ false);
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("isConst", VPackValue(const_cast<SubqueryEndNode*>(this)->isConst()));

  // And add it:
  nodes.close();
}

/// @brief invalidate the cost estimation for the node and its dependencies
void SubqueryEndNode::invalidateCost() {
  ExecutionNode::invalidateCost();
  // pass invalidation call to subquery too
  getSubquery()->invalidateCost();
}

bool SubqueryEndNode::isConst() {
  if (isModificationSubquery() || !isDeterministic()) {
    return false;
  }

  if (mayAccessCollections() && _plan->getAst()->query()->isModificationQuery()) {
    // a subquery that accesses data from a collection may not be const,
    // even if itself does not modify any data. it is possible that the
    // subquery is embedded into some outer loop that is modifying data
    return false;
  }

  arangodb::HashSet<Variable const*> vars;
  getVariablesUsedHere(vars);
  for (auto const& v : vars) {
    auto setter = _plan->getVarSetBy(v->id);

    if (setter == nullptr || setter->getType() != CALCULATION) {
      return false;
    }

    auto expression = ExecutionNode::castTo<CalculationNode const*>(setter)->expression();

    if (expression == nullptr) {
      return false;
    }
    if (!expression->isConstant()) {
      return false;
    }
  }

  return true;
}

bool SubqueryEndNode::mayAccessCollections() {
  if (_plan->getAst()->functionsMayAccessDocuments()) {
    // if the query contains any calls to functions that MAY access any
    // document, then we count this as a "yes"
    return true;
  }

  TRI_ASSERT(_subquery != nullptr);

  // if the subquery contains any of these nodes, it may access data from
  // a collection
  std::vector<ExecutionNode::NodeType> const types = {ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
                                                      ExecutionNode::ENUMERATE_COLLECTION,
                                                      ExecutionNode::INDEX,
                                                      ExecutionNode::INSERT,
                                                      ExecutionNode::UPDATE,
                                                      ExecutionNode::REPLACE,
                                                      ExecutionNode::REMOVE,
                                                      ExecutionNode::UPSERT,
                                                      ExecutionNode::TRAVERSAL,
                                                      ExecutionNode::SHORTEST_PATH,
                                                      ExecutionNode::K_SHORTEST_PATHS};

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  NodeFinder<std::vector<ExecutionNode::NodeType>> finder(types, nodes, true);
  _subquery->walk(finder);

  if (!nodes.empty()) {
    return true;
  }

  return false;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SubqueryEndNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  //   // TODO: Fill me in
  TRI_ASSERT(false);

  return nullptr;
}

ExecutionNode* SubqueryEndNode::clone(ExecutionPlan* plan, bool withDependencies,
                                   bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }
  auto c = std::make_unique<SubqueryEndNode>(plan, _id, _subquery->clone(plan, true, withProperties),
                                          outVariable);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief whether or not the subquery is a data-modification operation
bool SubqueryEndNode::isModificationSubquery() const {
  std::vector<ExecutionNode*> stack({_subquery});

  while (!stack.empty()) {
    ExecutionNode* current = stack.back();

    if (current->isModificationNode()) {
      return true;
    }

    stack.pop_back();

    current->dependencies(stack);
  }

  return false;
}

/// @brief replace the out variable, so we can adjust the name.
void SubqueryEndNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

/// @brief estimateCost
CostEstimate SubqueryEndNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate subEstimate = _subquery->getCost();

  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems * subEstimate.estimatedCost;
  return estimate;
}

/// @brief helper struct to find all (outer) variables used in a SubqueryEndNode
struct SubqueryVarUsageFinder final : public WalkerWorker<ExecutionNode> {
  arangodb::HashSet<Variable const*> _usedLater;
  arangodb::HashSet<Variable const*> _valid;

  SubqueryVarUsageFinder() {}

  ~SubqueryVarUsageFinder() {}

  bool before(ExecutionNode* en) override final {
    // Add variables used here to _usedLater:
    en->getVariablesUsedHere(_usedLater);
    return false;
  }

  void after(ExecutionNode* en) override final {
    // Add variables set here to _valid:
    for (auto& v : en->getVariablesSetHere()) {
      _valid.insert(v);
    }
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) override final {
    SubqueryVarUsageFinder subfinder;
    sub->walk(subfinder);

    // keep track of all variables used by a (dependent) subquery
    // this is, all variables in the subqueries _usedLater that are not in
    // _valid

    // create the set difference. note: cannot use std::set_difference as our
    // sets are NOT sorted
    for (auto var : subfinder._usedLater) {
      if (_valid.find(var) != _valid.end()) {
        _usedLater.insert(var);
      }
    }
    return false;
  }
};

/// @brief getVariablesUsedHere, modifying the set in-place
void SubqueryEndNode::getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const {
  SubqueryVarUsageFinder finder;
  _subquery->walk(finder);

  for (auto var : finder._usedLater) {
    if (finder._valid.find(var) == finder._valid.end()) {
      vars.insert(var);
    }
  }
}

/// @brief is the node determistic?
struct DeterministicFinder final : public WalkerWorker<ExecutionNode> {
  bool _isDeterministic = true;

  DeterministicFinder() : _isDeterministic(true) {}
  ~DeterministicFinder() {}

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if (!node->isDeterministic()) {
      _isDeterministic = false;
      return true;
    }
    return false;
  }
};

bool SubqueryEndNode::isDeterministic() {
  DeterministicFinder finder;
  _subquery->walk(finder);
  return finder._isDeterministic;
}

} // namespace aql
} // namespace arangodb
