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

#include "Aql/Ast.h"
#include "Aql/Expression.h"
#include "Cluster/ServerState.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/Optimizer/Optimizer.h"

#include "Aql/Optimizer/Utils/AccessesCollectionVariable.h"

#include "Containers/SmallVector.h"
#include "Containers/SmallUnorderedMap.h"

namespace arangodb::aql {

/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to
/// avoid redundant calculations in inner loops
void moveCalculationsUpRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                            OptimizerRule const& rule) {
  using EN = ExecutionNode;

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

    // we build a map of the top-most nodes of each subquery to the outer
    // subquery node
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
      // we will only move expressions up that cannot throw and that are
      // deterministic
      continue;
    }
    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (accessesCollectionVariable(plan.get(), nn, vars)) {
        isAccessCollection = true;
      }
    }
    // note: if it's a subquery node, it cannot move upwards if there's a
    // modification keyword in the subquery e.g.
    // INSERT would not be scope-limited by the outermost subqueries, so we
    // could end up inserting a smaller amount of documents than what's actually
    // proposed in the query.

    neededVars.clear();
    n->getVariablesUsedHere(neededVars);

    auto current = n->getFirstDependency();

    while (current != nullptr) {
      if (current->setsVariable(neededVars)) {
        // shared variable, cannot move up any more
        // done with optimizing this calculation node
        break;
      }

      auto dep = current->getFirstDependency();
      if (dep == nullptr) {
        auto it = subqueries.find(current);
        if (it != subqueries.end()) {
          // we reached the top of some subquery

          // first, unlink the calculation from the plan
          plan->unlinkNode(n);
          // and re-insert into before the subquery node
          plan->insertDependency(it->second, n);

          modified = true;
          current = n->getFirstDependency();
          continue;
        }

        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      if (current->getType() == EN::LIMIT) {
        if (!arangodb::ServerState::instance()->isCoordinator()) {
          // do not move calculations beyond a LIMIT on a single server,
          // as this would mean carrying out potentially unnecessary
          // calculations
          break;
        }

        // coordinator case
        // now check if the calculation uses data from any collection. if so,
        // we expect that it is cheaper to execute the calculation close to the
        // origin of data (e.g. IndexNode, EnumerateCollectionNode) on a DB
        // server than on a coordinator. though executing the calculation will
        // have the same costs on DB server and coordinator, the assumption is
        // that we can reduce the amount of data we need to transfer between the
        // two if we can execute the calculation on the DB server and only
        // transfer the calculation result to the coordinator instead of the
        // full documents

        if (!isAccessCollection) {
          // not accessing any collection data
          break;
        }
        // accessing collection data.
        // allow the calculation to be moved beyond the LIMIT,
        // in the hope that this reduces the amount of data we have
        // to transfer between the DB server and the coordinator
      }

      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(current, n);

      modified = true;
      current = dep;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
