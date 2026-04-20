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

#include "RemoveRedundantSorts.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void removeRedundantSortsRule(Optimizer* opt,
                              std::unique_ptr<ExecutionPlan> plan,
                              OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SORT, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      continue;
    }

    auto const sortNode = ExecutionNode::castTo<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation();

    if (sortInfo.isValid && !sortInfo.criteria.empty()) {
      std::vector<ExecutionNode*> stack;

      sortNode->dependencies(stack);

      int nodesRelyingOnSort = 0;

      while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == EN::SORT) {
          auto other =
              ExecutionNode::castTo<SortNode*>(current)->getSortInformation();

          bool canContinueSearch = true;
          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              if (nodesRelyingOnSort == 0) {
                if (!other.isDeterministic) {
                  canContinueSearch = false;
                  break;
                }

                if (sortNode->isStable()) {
                  canContinueSearch = false;
                  break;
                }

                toUnlink.emplace(current);
              } else {
                canContinueSearch = false;
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.emplace(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              toUnlink.emplace(n);
              canContinueSearch = false;
              break;
            }

            case SortInformation::allEqual: {
              toUnlink.emplace(current);
              break;
            }
          }
          if (!canContinueSearch) {
            break;
          }
        } else if (current->getType() == EN::FILTER) {
          // a filter does not depend on sort order
        } else if (current->getType() == EN::CALCULATION) {
          if (!current->isDeterministic()) {
            ++nodesRelyingOnSort;
          }
        } else if (current->getType() == EN::ENUMERATE_LIST ||
                   current->getType() == EN::ENUMERATE_COLLECTION ||
                   current->getType() == EN::TRAVERSAL ||
                   current->getType() == EN::ENUMERATE_PATHS ||
                   current->getType() == EN::SHORTEST_PATH) {
          ++nodesRelyingOnSort;
        } else {
          break;
        }

        if (!current->hasDependency()) {
          break;
        }

        current->dependencies(stack);
      }

      if (toUnlink.find(n) == toUnlink.end() &&
          sortNode->simplify(plan.get())) {
        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}
}  // namespace arangodb::aql
