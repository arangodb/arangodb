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

#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/Expression.h"
#include "Aql/Ast.h"
#include "Aql/Optimizer/Utils/AccessesCollectionVariable.h"

#include "Aql/Optimizer/Rules.h"

namespace arangodb::aql {

/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move calculations as far down in the plan as possible, beyond
/// FILTER and LIMIT operations
void arangodb::aql::moveCalculationsDownRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, {EN::CALCULATION, EN::SUBQUERY}, true);

  std::vector<ExecutionNode*> stack;
  VarSet vars;
  VarSet usedHere;
  bool modified = false;

  size_t i = 0;
  for (auto const& n : nodes) {
    bool const isLastVariable = ++i == nodes.size();

    // this is the variable that the calculation will set
    Variable const* variable = nullptr;

    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (!nn->expression()->isDeterministic()) {
        // we will only move expressions down that cannot throw and that are
        // deterministic
        continue;
      }
      variable = nn->outVariable();
    } else {
      TRI_ASSERT(n->getType() == EN::SUBQUERY);
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);
      if (!nn->isDeterministic() || nn->isModificationNode()) {
        // we will only move subqueries down that are deterministic and are not
        // modification subqueries
        continue;
      }
      variable = nn->outVariable();
    }

    stack.clear();
    n->parents(stack);

    ExecutionNode* lastNode = nullptr;

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      auto const currentType = current->getType();

      usedHere.clear();
      current->getVariablesUsedHere(usedHere);

      bool varUsedHere = std::find(usedHere.begin(), usedHere.end(),
                                   variable) != usedHere.end();

      if (n->getType() == EN::CALCULATION && currentType == EN::SUBQUERY &&
          varUsedHere && !current->isVarUsedLater(variable)) {
        // move calculations into subqueries if they are required by the
        // subquery and not used later
        current = ExecutionNode::castTo<SubqueryNode*>(current)->getSubquery();
        while (current->hasDependency()) {
          current = current->getFirstDependency();
        }
        lastNode = current;
      } else {
        if (varUsedHere) {
          // the node we're looking at needs the variable we're setting.
          // can't push further!
          break;
        }

        if (currentType == EN::FILTER || currentType == EN::SORT ||
            currentType == EN::LIMIT || currentType == EN::SINGLETON ||
            // do not move a subquery past another unrelated subquery
            (currentType == EN::SUBQUERY && n->getType() != EN::SUBQUERY)) {
          // we found something interesting that justifies moving our node down
          if (currentType == EN::LIMIT &&
              arangodb::ServerState::instance()->isCoordinator()) {
            // in a cluster, we do not want to move the calculations as far down
            // as possible, because this will mean we may need to transfer a lot
            // more data between DB servers and the coordinator

            // assume first that we want to move the node past the LIMIT

            // however, if our calculation uses any data from a
            // collection/index/view, it probably makes sense to not move it,
            // because the result set may be huge
            if (accessesCollectionVariable(plan.get(), n, vars)) {
              break;
            }
          }

          lastNode = current;
        } else if (currentType == EN::INDEX ||
                   currentType == EN::ENUMERATE_COLLECTION ||
                   currentType == EN::ENUMERATE_IRESEARCH_VIEW ||
                   currentType == EN::ENUMERATE_LIST ||
                   currentType == EN::TRAVERSAL ||
                   currentType == EN::SHORTEST_PATH ||
                   currentType == EN::ENUMERATE_PATHS ||
                   currentType == EN::COLLECT || currentType == EN::NORESULTS) {
          // we will not push further down than such nodes
          break;
        }
      }

      if (!current->hasParent()) {
        break;
      }

      current->parents(stack);
    }

    if (lastNode != nullptr && lastNode->getFirstParent() != nullptr) {
      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into after the last "good" node
      plan->insertDependency(lastNode->getFirstParent(), n);
      modified = true;

      // any changes done here may affect the following iterations
      // of this optimizer rule, so we need to recalculate the
      // variable usage here.
      if (!isLastVariable) {
        plan->clearVarUsageComputed();
        plan->findVarUsage();
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
