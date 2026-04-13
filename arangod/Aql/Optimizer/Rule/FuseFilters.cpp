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

#include "FuseFilters.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void fuseFiltersRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                     OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  if (nodes.size() < 2) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  ::arangodb::containers::HashSet<ExecutionNode*> seen;
  std::vector<std::pair<ExecutionNode*, ExecutionNode*>> candidates;

  bool modified = false;

  for (auto const& n : nodes) {
    if (seen.find(n) != seen.end()) {
      continue;
    }

    Variable const* nextExpectedVariable = nullptr;
    ExecutionNode* lastFilter = nullptr;
    candidates.clear();

    ExecutionNode* current = n;
    while (current != nullptr) {
      if (current->getType() == EN::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode*>(current);
        if (!cn->isDeterministic() ||
            cn->outVariable() != nextExpectedVariable) {
          break;
        }
        TRI_ASSERT(lastFilter != nullptr);
        candidates.emplace_back(current, lastFilter);
        nextExpectedVariable = nullptr;
      } else if (current->getType() == EN::FILTER) {
        seen.emplace(current);

        if (nextExpectedVariable != nullptr) {
          break;
        }
        nextExpectedVariable =
            ExecutionNode::castTo<FilterNode const*>(current)->inVariable();
        TRI_ASSERT(nextExpectedVariable != nullptr);
        if (current->isVarUsedLater(nextExpectedVariable)) {
          break;
        }
        lastFilter = current;
      } else {
        break;
      }
      current = current->getFirstDependency();
    }

    if (candidates.size() >= 2) {
      modified = true;
      AstNode* root =
          ExecutionNode::castTo<CalculationNode*>(candidates[0].first)
              ->expression()
              ->nodeForModification();
      for (size_t i = 1; i < candidates.size(); ++i) {
        root = plan->getAst()->createNodeBinaryOperator(
            NODE_TYPE_OPERATOR_BINARY_AND,
            ExecutionNode::castTo<CalculationNode const*>(candidates[i].first)
                ->expression()
                ->node(),
            root);

        plan->unlinkNode(candidates[i - 1].second);
        plan->unlinkNode(candidates[i - 1].first);
      }

      ExecutionNode* en = candidates.back().first;
      TRI_ASSERT(en->getType() == EN::CALCULATION);
      ExecutionNode::castTo<CalculationNode*>(en)->expression()->replaceNode(
          root);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
