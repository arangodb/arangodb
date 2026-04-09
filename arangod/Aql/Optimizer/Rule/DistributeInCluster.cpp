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

#include "DistributeInCluster.h"

#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

#include <tuple>

namespace {

using EN = arangodb::aql::ExecutionNode;

auto isGraphNode(EN::NodeType nodeType) noexcept -> bool {
  return nodeType == EN::TRAVERSAL || nodeType == EN::SHORTEST_PATH ||
         nodeType == EN::ENUMERATE_PATHS;
}

auto isModificationNode(EN::NodeType nodeType) noexcept -> bool {
  return nodeType == EN::INSERT || nodeType == EN::REMOVE ||
         nodeType == EN::UPDATE || nodeType == EN::REPLACE ||
         nodeType == EN::UPSERT;
}

auto nodeEligibleForDistribute(EN::NodeType nodeType) noexcept -> bool {
  return isModificationNode(nodeType) || isGraphNode(nodeType);
}

auto extractSmartnessAndCollection(arangodb::aql::ExecutionNode* node)
    -> std::tuple<bool, bool, arangodb::aql::Collection const*> {
  auto nodeType = node->getType();
  auto collection = static_cast<arangodb::aql::Collection const*>(nullptr);
  auto isSmart = bool{false};
  auto isDisjoint = bool{false};

  if (nodeType == EN::TRAVERSAL || nodeType == EN::SHORTEST_PATH ||
      nodeType == EN::ENUMERATE_PATHS) {
    auto const* graphNode = EN::castTo<arangodb::aql::GraphNode*>(node);

    isSmart = graphNode->isSmart();
    isDisjoint = graphNode->isDisjoint();

    collection = graphNode->collection();
  } else {
    auto const* collectionAccessingNode =
        dynamic_cast<arangodb::aql::CollectionAccessingNode*>(node);
    TRI_ASSERT(collectionAccessingNode != nullptr);

    collection = collectionAccessingNode->collection();
    isSmart = collection->isSmart();
  }

  return std::tuple<bool, bool, arangodb::aql::Collection const*>{
      isSmart, isDisjoint, collection};
}

}  // namespace

namespace arangodb::aql {

void distributeInClusterRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                             OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  containers::SmallVector<ExecutionNode*, 8> subqueryNodes;
  subqueryNodes.push_back(plan->root());
  plan->findNodesOfType(subqueryNodes, ExecutionNode::SUBQUERY, true);

  for (ExecutionNode* subqueryNode : subqueryNodes) {
    SubqueryNode* snode = nullptr;
    ExecutionNode* root = nullptr;
    bool reachedEnd = false;
    if (subqueryNode == plan->root()) {
      snode = nullptr;
      root = plan->root();
    } else {
      snode = ExecutionNode::castTo<SubqueryNode*>(subqueryNode);
      root = snode->getSubquery();
    }
    ExecutionNode* node = root;
    TRI_ASSERT(node != nullptr);

    while (node != nullptr) {
      auto nodeType = node->getType();

      while (node != nullptr) {
        nodeType = node->getType();

        if (nodeEligibleForDistribute(nodeType)) {
          break;
        }

        if (!node->hasDependency()) {
          reachedEnd = true;
          break;
        }

        node = node->getFirstDependency();
      }

      if (reachedEnd) {
        break;
      }

      TRI_ASSERT(node != nullptr);
      if (node == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
      }

      TRI_ASSERT(nodeEligibleForDistribute(nodeType));

      auto const [isSmart, isDisjoint, collection] =
          extractSmartnessAndCollection(node);

#ifdef USE_ENTERPRISE
      if (isSmart) {
        node =
            distributeInClusterRuleSmart(plan.get(), snode, node, wasModified);
      }
#endif

      TRI_ASSERT(collection != nullptr);
      bool const defaultSharding = collection->usesDefaultSharding();

      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        if (!defaultSharding) {
          node = node->getFirstDependency();
          continue;
        }
      }

      if (isModificationNode(nodeType) ||
          (isGraphNode(nodeType) && isSmart && isDisjoint)) {
        node = insertDistributeGatherSnippet(*plan, node, snode);
        wasModified = true;
      } else {
        node = node->getFirstDependency();
      }
    }
  }
  opt->addPlan(std::move(plan), rule, wasModified);
}

}  // namespace arangodb::aql
