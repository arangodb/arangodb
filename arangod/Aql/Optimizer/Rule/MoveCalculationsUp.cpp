////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "MoveCalculationsUp.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {
bool accessesCollectionVariable(ExecutionPlan const* plan,
                                ExecutionNode const* node, VarSet& vars) {
  if (node->getType() == EN::CALCULATION) {
    auto nn = EN::castTo<CalculationNode const*>(node);
    vars.clear();
    Ast::getReferencedVariables(nn->expression()->node(), vars);
  } else if (node->getType() == EN::SUBQUERY) {
    auto nn = EN::castTo<SubqueryNode const*>(node);
    vars.clear();
    nn->getVariablesUsedHere(vars);
  }

  for (auto const& it : vars) {
    auto setter = plan->getVarSetBy(it->id);
    if (setter == nullptr) {
      continue;
    }
    if (setter->getType() == EN::INDEX ||
        setter->getType() == EN::ENUMERATE_COLLECTION ||
        setter->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
        setter->getType() == EN::SUBQUERY ||
        setter->getType() == EN::TRAVERSAL ||
        setter->getType() == EN::ENUMERATE_PATHS ||
        setter->getType() == EN::SHORTEST_PATH) {
      return true;
    }
  }

  return false;
}
}  // namespace

void arangodb::aql::moveCalculationsUpRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  containers::SmallUnorderedMap<ExecutionNode*,
                                ExecutionNode*>::allocator_type::arena_type
      subqueriesArena;
  containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> subqueries{
      subqueriesArena};
  {
    containers::SmallVector<ExecutionNode*, 8> subs;
    plan->findNodesOfType(subs, ExecutionNode::SUBQUERY, true);

    for (auto& it : subs) {
      auto sub = ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery();
      while (sub->hasDependency()) {
        sub = sub->getFirstDependency();
      }
      subqueries.emplace(sub, it);
    }
  }

  bool modified = false;
  VarSet neededVars;
  VarSet vars;

  for (auto const& n : nodes) {
    bool isAccessCollection = false;
    if (!n->isDeterministic()) {
      continue;
    }
    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (::accessesCollectionVariable(plan.get(), nn, vars)) {
        isAccessCollection = true;
      }
    }

    neededVars.clear();
    n->getVariablesUsedHere(neededVars);

    auto current = n->getFirstDependency();

    while (current != nullptr) {
      if (current->setsVariable(neededVars)) {
        break;
      }

      auto dep = current->getFirstDependency();
      if (dep == nullptr) {
        auto it = subqueries.find(current);
        if (it != subqueries.end()) {
          plan->unlinkNode(n);
          plan->insertDependency(it->second, n);

          modified = true;
          current = n->getFirstDependency();
          continue;
        }

        break;
      }

      if (current->getType() == EN::LIMIT) {
        if (!arangodb::ServerState::instance()->isCoordinator()) {
          break;
        }

        if (!isAccessCollection) {
          break;
        }
      }

      plan->unlinkNode(n);
      plan->insertDependency(current, n);

      modified = true;
      current = dep;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
