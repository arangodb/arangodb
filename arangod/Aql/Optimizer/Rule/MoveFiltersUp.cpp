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

#include "MoveFiltersUp.h"

#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void moveFiltersUpRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  std::vector<ExecutionNode*> stack;
  bool modified = false;

  for (auto const& n : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto inVar = fn->inVariable();

    stack.clear();
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::LIMIT || current->getType() == EN::WINDOW) {
        break;
      }

      if (!current->isDeterministic()) {
        break;
      }

      if (current->isModificationNode()) {
        break;
      }

      if (current->getType() == EN::CALCULATION) {
        auto calculation =
            ExecutionNode::castTo<CalculationNode const*>(current);
        if (!calculation->expression()->isDeterministic()) {
          break;
        }
      }

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        if (inVar == v) {
          found = true;
          break;
        }
      }

      if (found) {
        break;
      }

      if (!current->hasDependency()) {
        break;
      }

      current->dependencies(stack);

      plan->unlinkNode(n);
      plan->insertDependency(current, n);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
