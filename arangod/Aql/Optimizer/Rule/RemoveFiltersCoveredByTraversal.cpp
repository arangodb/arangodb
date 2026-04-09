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

#include "RemoveFiltersCoveredByTraversal.h"

#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

// remove filter nodes already covered by a traversal
void removeFiltersCoveredByTraversal(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> fNodes;
  plan->findNodesOfType(fNodes, EN::FILTER, true);
  if (fNodes.empty()) {
    // no filters present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& node : fNodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::TRAVERSAL) {
        auto traversalNode =
            ExecutionNode::castTo<TraversalNode const*>(current);

        // found a traversal node, now check if the expression
        // is covered by the traversal
        auto traversalCondition = traversalNode->condition();

        if (traversalCondition != nullptr && !traversalCondition->isEmpty()) {
          VarSet varsUsedByCondition;
          Ast::getReferencedVariables(condition.root(), varsUsedByCondition);

          auto remover = [&](Variable const* outVariable,
                             bool isPathCondition) -> bool {
            if (outVariable == nullptr) {
              return false;
            }
            if (varsUsedByCondition.find(outVariable) ==
                varsUsedByCondition.end()) {
              return false;
            }

            auto newNode = condition.removeTraversalCondition(
                plan.get(), outVariable, traversalCondition->root(),
                isPathCondition);
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it
              // is still used by other nodes in the plan
              return true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              auto* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              return true;
            }

            return false;
          };

          std::array<std::pair<Variable const*, bool>, 3> vars = {
              std::make_pair(traversalNode->pathOutVariable(),
                             /*isPathCondition*/ true),
              std::make_pair(traversalNode->vertexOutVariable(),
                             /*isPathCondition*/ false),
              std::make_pair(traversalNode->edgeOutVariable(),
                             /*isPathCondition*/ false)};

          for (auto [v, isPathCondition] : vars) {
            if (remover(v, isPathCondition)) {
              modified = true;
              handled = true;
              break;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }
      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
