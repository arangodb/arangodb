////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewNode.h"
#include "Views/ViewIterator.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Aql/Query.h"

namespace arangodb {
namespace iresearch {

IResearchViewNode::IResearchViewNode(
    arangodb::aql::ExecutionPlan* plan,
    arangodb::velocypack::Slice const& base)
  : aql::ExecutionNode(plan, base),
    _vocbase(plan->getAst()->query()->vocbase()),
    _view(_vocbase->lookupView(base.get("view").copyString())),
    _outVariable(aql::Variable::varFromVPack(plan->getAst(), base, "outVariable")),
    _condition(aql::Condition::fromVPack(plan, base.get("condition"))),
    _sortCondition(aql::SortCondition::fromVelocyPack(plan, base, "sortCondition")) {
}

IResearchViewNode::~IResearchViewNode() {
  delete _condition;
}

bool IResearchViewNode::volatile_state() const {
  if (_condition && isInInnerLoop()) {
    auto const* conditionRoot = _condition->root();

    if (!conditionRoot->isDeterministic()) {
      return true;
    }

    std::unordered_set<aql::Variable const*> vars;
    aql::Ast::getReferencedVariables(conditionRoot, vars);
    vars.erase(this->outVariable()); // remove "our" variable

    for (auto const* var : vars) {
      auto* setter = this->plan()->getVarSetBy(var->id);

      if (!setter) {
        // unable to find setter
        continue;
      }

      if (!setter->isDeterministic()) {
        // found nondeterministic setter
        return true;
      }

      switch (setter->getType()) {
        case aql::ExecutionNode::ENUMERATE_COLLECTION:
        case aql::ExecutionNode::ENUMERATE_LIST:
        case aql::ExecutionNode::SUBQUERY:
        case aql::ExecutionNode::COLLECT:
        case aql::ExecutionNode::TRAVERSAL:
        case aql::ExecutionNode::INDEX:
        case aql::ExecutionNode::SHORTEST_PATH:
        case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
          // we're in the loop with dependent context
          return true;
        default:
          break;
      }
    }
  }

  return false;
}

/// @brief toVelocyPack, for EnumerateViewNode
void IResearchViewNode::toVelocyPackHelper(
    VPackBuilder& nodes,
    bool verbose
) const {

  // call base class method
  aql::ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("view", VPackValue(_view->name()));

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add(VPackValue("condition"));
  if (_condition) {
    _condition->toVelocyPack(nodes, verbose);
  } else {
    nodes.openObject();
    nodes.close();
  }

  nodes.add(VPackValue("sortCondition"));
  if (_sortCondition) {
    _sortCondition->toVelocyPackHelper(nodes, verbose);
  } else {
    nodes.openObject();
    nodes.close();
  }

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
aql::ExecutionNode* IResearchViewNode::clone(
    aql::ExecutionPlan* plan,
    bool withDependencies,
    bool withProperties
) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new IResearchViewNode(
    plan, _id, _vocbase, _view, outVariable, _condition, _sortCondition
  );
  // FIXME clone condition and sortCondition?

  cloneHelper(c, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief the cost of an enumerate view node
double IResearchViewNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);

  // For the time being, we assume 100
  size_t length = 100; // TODO: get a better guess from view

  nrItems = length * incoming;
  return depCost + static_cast<double>(length) * incoming;
}

std::unique_ptr<arangodb::ViewIterator> IResearchViewNode::iterator(
    transaction::Methods& trx,
    aql::ExpressionContext& ctx
) const {
  return std::unique_ptr<arangodb::ViewIterator>(_view->iteratorForCondition(
    &trx,
    const_cast<aql::ExecutionPlan*>(plan()), // FIXME `Expression` requires non-const pointer
    &ctx,
    _condition ? _condition->root() : nullptr,  // no filter means 'RETURN *'
    _outVariable,
    _sortCondition.get()
  ));
}

std::vector<aql::Variable const*> IResearchViewNode::getVariablesUsedHere() const {
  std::unordered_set<aql::Variable const*> vars;
  getVariablesUsedHere(vars);
  return { vars.begin(), vars.end() };
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IResearchViewNode::getVariablesUsedHere(
    std::unordered_set<aql::Variable const*>& vars
) const {
  if (_condition) {
    aql::Ast::getReferencedVariables(_condition->root(), vars);
  }
}

} // iresearch
} // arangodb
