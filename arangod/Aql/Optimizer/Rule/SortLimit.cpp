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

#include "SortLimit.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Containers/SmallVector.h"

#include <absl/strings/str_cat.h>

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

bool shouldApplyHeapOptimization(SortNode& sortNode, LimitNode& limitNode) {
  size_t input = sortNode.getCost().estimatedNrItems;
  size_t output = limitNode.limit() + limitNode.offset();

  if (input < 100) {
    return false;
  }

  double N = static_cast<double>(input);
  double M = static_cast<double>(output);
  double lgN = std::log2(N);
  double lgM = std::log2(M);

  return (0.25 * N * lgM + M * lgM) < (N * lgN);
}

bool isAllowedIntermediateSortLimitNode(ExecutionNode* node) {
  switch (node->getType()) {
    case EN::CALCULATION:
    case EN::OFFSET_INFO_MATERIALIZE:
    case EN::SUBQUERY:
    case EN::REMOTE:
    case EN::ASYNC:
      return true;
    case EN::GATHER:
      return ExecutionNode::castTo<GatherNode*>(node)->isSortingGather();
    case EN::WINDOW:
      return !ExecutionNode::castTo<WindowNode*>(node)->needsFollowingRows();
    case EN::SINGLETON:
    case EN::ENUMERATE_COLLECTION:
    case EN::ENUMERATE_LIST:
    case EN::ENUMERATE_NEAR_VECTORS:
    case EN::FILTER:
    case EN::LIMIT:
    case EN::SORT:
    case EN::COLLECT:
    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::NORESULTS:
    case EN::UPSERT:
    case EN::TRAVERSAL:
    case EN::INDEX:
    case EN::INDEX_COLLECT:
    case EN::JOIN:
    case EN::SHORTEST_PATH:
    case EN::ENUMERATE_PATHS:
    case EN::ENUMERATE_IRESEARCH_VIEW:
    case EN::RETURN:
    case EN::DISTRIBUTE:
    case EN::SCATTER:
    case EN::REMOTE_SINGLE:
    case EN::REMOTE_MULTIPLE:
    case EN::DISTRIBUTE_CONSUMER:
    case EN::SUBQUERY_START:
    case EN::SUBQUERY_END:
    case EN::MATERIALIZE:
    case EN::MUTEX:
      return false;
    case EN::MAX_NODE_TYPE_VALUE:
      break;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL_AQL,
      absl::StrCat(
          "Unhandled node type '", node->getTypeString(),
          "' in sort-limit optimizer rule. Please report "
          "this error. Try turning off the sort-limit rule to get your query "
          "working."));
}

}  // namespace

void sortLimitRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                   OptimizerRule const& rule) {
  bool mod = false;
  if (!plan->contains(EN::LIMIT) ||
      (!plan->contains(EN::SORT) && !plan->contains(EN::GATHER))) {
    opt->addPlan(std::move(plan), rule, mod);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> limitNodes;

  plan->findNodesOfType(limitNodes, EN::LIMIT, true);
  for (ExecutionNode* node : limitNodes) {
    bool hasRemoteBeforeSort{false};
    bool firstSortNode{true};
    auto limitNode = ExecutionNode::castTo<LimitNode*>(node);
    for (ExecutionNode* current = limitNode->getFirstDependency();
         current != nullptr; current = current->getFirstDependency()) {
      if (current->getType() == EN::SORT) {
        auto sortNode = ExecutionNode::castTo<SortNode*>(current);
        if (shouldApplyHeapOptimization(*sortNode, *limitNode)) {
          sortNode->setLimit(limitNode->offset() + limitNode->limit());
          if (firstSortNode) {
            auto& mainLimitNode = *ExecutionNode::castTo<LimitNode*>(limitNode);
            if (!hasRemoteBeforeSort) {
              plan->unlinkNode(limitNode);
            }
            auto* auxLimitNode =
                hasRemoteBeforeSort
                    ? plan->registerNode(std::make_unique<LimitNode>(
                          plan.get(), plan->nextId(), 0,
                          limitNode->offset() + limitNode->limit()))
                    : limitNode;
            TRI_ASSERT(auxLimitNode);
            if (hasRemoteBeforeSort && mainLimitNode.fullCount()) {
              TRI_ASSERT(limitNode != auxLimitNode);
              auto& tmp = *ExecutionNode::castTo<LimitNode*>(auxLimitNode);
              tmp.setFullCount();
              mainLimitNode.setFullCount(false);
            }
            auto* sortParent = sortNode->getFirstParent();
            TRI_ASSERT(sortParent);
            if (sortParent != auxLimitNode) {
              sortParent->replaceDependency(sortNode, auxLimitNode);
              sortNode->addParent(auxLimitNode);
            }
          }
          firstSortNode = false;
          mod = true;
        }
      } else if (current->getType() == EN::GATHER) {
        auto gatherNode = ExecutionNode::castTo<GatherNode*>(current);
        if (gatherNode->isSortingGather()) {
          gatherNode->setConstrainedSortLimit(limitNode->offset() +
                                              limitNode->limit());
          mod = true;
        }
      } else if (current->getType() == EN::REMOTE) {
        hasRemoteBeforeSort = true;
      }

      if (!isAllowedIntermediateSortLimitNode(current)) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, mod);
}
}  // namespace arangodb::aql
