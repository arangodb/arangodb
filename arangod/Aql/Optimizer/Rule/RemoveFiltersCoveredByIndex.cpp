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

#include "RemoveFiltersCoveredByIndex.h"

#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ConditionCoverage.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"

namespace arangodb::aql {
using EN = ExecutionNode;

AstNode* removeIndexCondition(Condition& cond, ExecutionPlan const* plan,
                              Variable const* variable, AstNode const* other,
                              Index const* index) {
  TRI_ASSERT(index != nullptr);
  AstNode* root = cond.root();
  if (root == nullptr || other == nullptr) {
    return root;
  }
  AstNode const* andNode = nullptr;
  AstNode const* conditionAndNode = nullptr;
  if (!extractSingleAndNodes(root, other, andNode, conditionAndNode)) {
    return root;
  }
  auto toRemove = collectOverlappingMembersForIndex(plan, variable, andNode,
                                                    conditionAndNode, index);
  if (toRemove.empty()) {
    return root;
  }
  return rebuildConditionWithoutMembers(plan->getAst(), andNode, toRemove);
}

void removeFiltersCoveredByIndexRule(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
  bool modified = false;
  bool rerun = false;

  for (auto const& node : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    auto setter = plan->getVarSetBy(fn->inVariable()->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = ExecutionNode::castTo<IndexNode*>(current);

        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && !indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            AstNode* newNode{nullptr};
            if (!indexNode->isAllCoveredByOneIndex()) {
              if (n != 1) {
                break;
              }
              newNode = removeIndexCondition(
                  condition, plan.get(), indexNode->outVariable(),
                  indexCondition->root(), indexesUsed[0].get());
            }
            if (newNode == nullptr) {
              toUnlink.emplace(node);
              modified = true;
              handled = true;
            } else if (newNode != condition.root()) {
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              CalculationNode* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
              rerun = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  if (rerun) {
    TRI_ASSERT(modified);
    opt->addPlanAndRerun(std::move(plan), rule, modified);
  } else {
    opt->addPlan(std::move(plan), rule, modified);
  }
}
}  // namespace arangodb::aql
