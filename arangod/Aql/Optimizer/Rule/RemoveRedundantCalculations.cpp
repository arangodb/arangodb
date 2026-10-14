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
////////////////////////////////////////////////////////////////////////////////

#include "RemoveRedundantCalculations.h"

#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/VariableReplacer.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void removeRedundantCalculationsRule(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.size() < 2) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_map<VariableId, Variable const*> replacements;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      continue;
    }

    arangodb::aql::Variable const* outvar = nn->outVariable();
    AstNode const* reference = nn->expression()->node();

    std::vector<ExecutionNode*> stack;
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        auto const* other =
            ExecutionNode::castTo<CalculationNode const*>(current)
                ->expression()
                ->node();
        if (compareAstNodes<false>(other, reference, false) == 0) {
          auto target = ExecutionNode::castTo<CalculationNode const*>(current)
                            ->outVariable();
          while (target != nullptr) {
            auto it = replacements.find(target->id);

            if (it != replacements.end()) {
              target = (*it).second;
            } else {
              break;
            }
          }
          replacements.emplace(outvar->id, target);

          for (auto it = replacements.begin(); it != replacements.end(); ++it) {
            if ((*it).second == outvar) {
              (*it).second = target;
            }
          }
        }
      }

      if (current->getType() == EN::COLLECT) {
        if (ExecutionNode::castTo<CollectNode*>(current)->hasOutVariable()) {
          break;
        }
      }

      if (!current->hasDependency()) {
        break;
      }

      current->dependencies(stack);
    }
  }

  if (!replacements.empty()) {
    VariableReplacer finder(replacements);
    plan->root()->walk(finder);
  }

  opt->addPlan(std::move(plan), rule, !replacements.empty());
}
}  // namespace arangodb::aql