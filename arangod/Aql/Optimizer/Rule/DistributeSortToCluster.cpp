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

#include "DistributeSortToCluster.h"

#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/types.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

/// @brief move sorts up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// sorts are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
///
/// filters are not pushed beyond limits
void distributeSortToClusterRule(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  VarSet usedBySort;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  bool modified = false;

  for (auto& n : nodes) {
    auto const remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    auto gatherNode = ExecutionNode::castTo<GatherNode*>(n);

    auto parents = n->getParents();

    while (true) {
      TRI_ASSERT(!parents.empty());
      bool stopSearching = false;
      auto inspectNode = parents[0];
      TRI_ASSERT(inspectNode != nullptr);

      switch (inspectNode->getType()) {
        case EN::SINGLETON:
        case EN::ENUMERATE_COLLECTION:
        case EN::ENUMERATE_LIST:
        case EN::ENUMERATE_NEAR_VECTORS:
        case EN::COLLECT:
        case EN::INDEX_COLLECT:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
        case EN::CALCULATION:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX:
        case EN::JOIN:
        case EN::TRAVERSAL:
        case EN::ENUMERATE_PATHS:
        case EN::SHORTEST_PATH:
        case EN::REMOTE_SINGLE:
        case EN::REMOTE_MULTIPLE:
        case EN::ENUMERATE_IRESEARCH_VIEW:
        case EN::WINDOW:
        case EN::MATERIALIZE:
        case EN::OFFSET_INFO_MATERIALIZE:
          stopSearching = true;
          break;

        case EN::SORT: {
          auto thisSortNode = ExecutionNode::castTo<SortNode*>(inspectNode);
          usedBySort.clear();
          thisSortNode->getVariablesUsedHere(usedBySort);
          parents = inspectNode->getParents();
          plan->unlinkNode(inspectNode);
          if (thisSortNode->reinsertInCluster()) {
            ExecutionNode* insertPoint = rn;
            auto current = insertPoint->getFirstDependency();
            while (current != nullptr &&
                   current->getType() == EN::CALCULATION) {
              auto nn = ExecutionNode::castTo<CalculationNode*>(current);
              if (!nn->expression()->isDeterministic()) {
                break;
              }
              auto variable = nn->outVariable();
              if (usedBySort.find(variable) == usedBySort.end()) {
                insertPoint = current;
              } else {
                break;
              }
              current = current->getFirstDependency();
            }
            plan->insertDependency(insertPoint, inspectNode);
          }

          gatherNode->elements(thisSortNode->elements());
          modified = true;
          break;
        }
        case EN::SUBQUERY_START:
        case EN::SUBQUERY_END:
        case EN::DISTRIBUTE_CONSUMER:
        case EN::ASYNC:
        case EN::MUTEX:
        case EN::MAX_NODE_TYPE_VALUE: {
          stopSearching = true;
          TRI_ASSERT(false);
          break;
        }
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
