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

#include "SubqueryNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/NodeFinder.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Exceptions.h"

#include <initializer_list>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

/// @brief helper struct to find all (outer) variables used in a SubqueryNode
struct SubqueryVarUsageFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::Unique> {
  VarSet _usedLater;
  VarSet _valid;

  ~SubqueryVarUsageFinder() = default;

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

/// @brief is the node determistic?
struct DeterministicFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::Unique> {
  bool _isDeterministic = true;

  DeterministicFinder() : _isDeterministic(true) {}
  ~DeterministicFinder() = default;

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

}  // namespace

SubqueryNode::SubqueryNode(ExecutionPlan* plan, ExecutionNodeId id,
                           ExecutionNode* subquery, Variable const* outVariable)
    : ExecutionNode(plan, id), _subquery(subquery), _outVariable(outVariable) {
  TRI_ASSERT(_subquery != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

SubqueryNode::SubqueryNode(ExecutionPlan* plan,
                           arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _subquery(nullptr),
      _outVariable(
          Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

/// @brief doToVelocyPack, for SubqueryNode
void SubqueryNode::doToVelocyPack(velocypack::Builder& nodes,
                                  unsigned flags) const {
  // Since we have spliced subqueries this should never be called.
  // However, we still keep the old implementation around in case it is needed
  // again at some point (e.g., if we want to serialize nodes during
  // optimization)
  TRI_ASSERT(false);
  {
    VPackObjectBuilder guard(&nodes, "subquery");
    nodes.add(VPackValue("nodes"));
    _subquery->allToVelocyPack(nodes, flags);
  }
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("isConst", VPackValue(const_cast<SubqueryNode*>(this)->isConst()));
}

/// @brief invalidate the cost estimation for the node and its dependencies
void SubqueryNode::invalidateCost() {
  ExecutionNode::invalidateCost();
  // pass invalidation call to subquery too
  getSubquery()->invalidateCost();
}

bool SubqueryNode::isConst() {
  if (isModificationNode() || !isDeterministic()) {
    return false;
  }

  if (mayAccessCollections() && _plan->getAst()->containsModificationNode()) {
    // a subquery that accesses data from a collection may not be const,
    // even if itself does not modify any data. it is possible that the
    // subquery is embedded into some outer loop that is modifying data
    return false;
  }

  VarSet vars;
  getVariablesUsedHere(vars);
  for (auto const& v : vars) {
    auto setter = _plan->getVarSetBy(v->id);

    if (setter == nullptr || setter->getType() != CALCULATION) {
      return false;
    }

    auto expression =
        ExecutionNode::castTo<CalculationNode const*>(setter)->expression();

    if (expression == nullptr) {
      return false;
    }
    if (!expression->isConstant()) {
      return false;
    }
  }

  return true;
}

bool SubqueryNode::mayAccessCollections() {
  if (_plan->getAst()->functionsMayAccessDocuments()) {
    // if the query contains any calls to functions that MAY access any
    // document, then we count this as a "yes"
    return true;
  }

  TRI_ASSERT(_subquery != nullptr);

  // if the subquery contains any of these nodes, it may access data from
  // a collection
  static constexpr std::initializer_list<ExecutionNode::NodeType> types{
      ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
      ExecutionNode::ENUMERATE_COLLECTION,
      ExecutionNode::INDEX,
      ExecutionNode::JOIN,
      ExecutionNode::INSERT,
      ExecutionNode::UPDATE,
      ExecutionNode::REPLACE,
      ExecutionNode::REMOVE,
      ExecutionNode::UPSERT,
      ExecutionNode::TRAVERSAL,
      ExecutionNode::SHORTEST_PATH,
      ExecutionNode::ENUMERATE_PATHS};

  containers::SmallVector<ExecutionNode*, 8> nodes;

  NodeFinder<std::initializer_list<ExecutionNode::NodeType>,
             WalkerUniqueness::Unique>
      finder(types, nodes, true);
  _subquery->walk(finder);

  if (!nodes.empty()) {
    return true;
  }

  return false;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SubqueryNode::createBlock(
    ExecutionEngine& /*engine*/) const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot instantiate SubqueryExecutor");
}

ExecutionNode* SubqueryNode::clone(ExecutionPlan* plan,
                                   bool withDependencies) const {
  return cloneHelper(std::make_unique<SubqueryNode>(
                         plan, _id, _subquery->clone(plan, true), _outVariable),
                     withDependencies);
}

/// @brief whether or not the subquery is a data-modification operation
bool SubqueryNode::isModificationNode() const {
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
void SubqueryNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

/// @brief estimateCost
CostEstimate SubqueryNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate subEstimate = _subquery->getCost();

  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost +=
      estimate.estimatedNrItems * subEstimate.estimatedCost;
  return estimate;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void SubqueryNode::getVariablesUsedHere(VarSet& vars) const {
  SubqueryVarUsageFinder finder;
  _subquery->walk(finder);

  for (auto var : finder._usedLater) {
    if (finder._valid.find(var) == finder._valid.end()) {
      vars.insert(var);
    }
  }
}

bool SubqueryNode::isDeterministic() {
  DeterministicFinder finder;
  _subquery->walk(finder);
  return finder._isDeterministic;
}

ExecutionNode::NodeType SubqueryNode::getType() const { return SUBQUERY; }

size_t SubqueryNode::getMemoryUsedBytes() const { return sizeof(*this); }

Variable const* SubqueryNode::outVariable() const { return _outVariable; }

ExecutionNode* SubqueryNode::getSubquery() const { return _subquery; }

void SubqueryNode::setSubquery(ExecutionNode* subquery, bool forceOverwrite) {
  TRI_ASSERT(subquery != nullptr);
  TRI_ASSERT((forceOverwrite && _subquery != nullptr) ||
             (!forceOverwrite && _subquery == nullptr));
  _subquery = subquery;
}

std::vector<Variable const*> SubqueryNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}
