////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRuleSortLimit.h"

#include "Aql/ExecutionNode/AsyncNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Basics/Exceptions.h"
#include "Containers/SmallVector.h"

#include <absl/strings/str_cat.h>
#include <cmath>
#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

bool shouldApplyHeapOptimization(arangodb::aql::SortNode& sortNode,
                                 arangodb::aql::LimitNode& limitNode) {
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
    case ExecutionNode::CALCULATION:
    case ExecutionNode::OFFSET_INFO_MATERIALIZE:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::REMOTE:
    case ExecutionNode::ASYNC:
      return true;
    case ExecutionNode::GATHER:
      return ExecutionNode::castTo<GatherNode*>(node)->isSortingGather();
    case ExecutionNode::WINDOW:
      return !ExecutionNode::castTo<WindowNode*>(node)->needsFollowingRows();
    case ExecutionNode::SINGLETON:
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::ENUMERATE_NEAR_VECTORS:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::SORT:
    case ExecutionNode::COLLECT:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::UPSERT:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::INDEX:
    case ExecutionNode::INDEX_COLLECT:
    case ExecutionNode::JOIN:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::ENUMERATE_PATHS:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::RETURN:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::SCATTER:
    case ExecutionNode::REMOTE_SINGLE:
    case ExecutionNode::REMOTE_MULTIPLE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::MATERIALIZE:
    case ExecutionNode::MUTEX:
      return false;
    case ExecutionNode::MAX_NODE_TYPE_VALUE:
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

void arangodb::aql::sortLimitRule(Optimizer* opt,
                                  std::unique_ptr<ExecutionPlan> plan,
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
