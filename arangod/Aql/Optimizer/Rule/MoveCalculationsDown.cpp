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

#include "MoveCalculationsDown.h"

#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/AccessesCollectionVariable.h"
#include "Aql/Variable.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void moveCalculationsDownRule(
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

    Variable const* variable = nullptr;

    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (!nn->expression()->isDeterministic()) {
        continue;
      }
      variable = nn->outVariable();
    } else {
      TRI_ASSERT(n->getType() == EN::SUBQUERY);
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);
      if (!nn->isDeterministic() || nn->isModificationNode()) {
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
        current = ExecutionNode::castTo<SubqueryNode*>(current)->getSubquery();
        while (current->hasDependency()) {
          current = current->getFirstDependency();
        }
        lastNode = current;
      } else {
        if (varUsedHere) {
          break;
        }

        if (currentType == EN::FILTER || currentType == EN::SORT ||
            currentType == EN::LIMIT || currentType == EN::SINGLETON ||
            (currentType == EN::SUBQUERY && n->getType() != EN::SUBQUERY)) {
          if (currentType == EN::LIMIT &&
              arangodb::ServerState::instance()->isCoordinator()) {
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
          break;
        }
      }

      if (!current->hasParent()) {
        break;
      }

      current->parents(stack);
    }

    if (lastNode != nullptr && lastNode->getFirstParent() != nullptr) {
      plan->unlinkNode(n);
      plan->insertDependency(lastNode->getFirstParent(), n);
      modified = true;

      if (!isLastVariable) {
        plan->clearVarUsageComputed();
        plan->findVarUsage();
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql