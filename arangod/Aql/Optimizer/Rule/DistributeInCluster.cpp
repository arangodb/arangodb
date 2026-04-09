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

/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only sent to one shard and not all as in scatterInCluster
///
/// it will change plans in place
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

    // Note that here we are in the Disjoint SmartGraph case and "collection()"
    // will give us any collection in the graph, but they're all sharded the
    // same way.
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

void distributeInClusterRule(Optimizer* opt,
                             std::unique_ptr<ExecutionPlan> plan,
                             OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  // we are a coordinator, we replace the root if it is a modification node

  // only replace if it is the last node in the plan
  containers::SmallVector<ExecutionNode*, 8> subqueryNodes;
  // inspect each return node and work upwards to SingletonNode
  subqueryNodes.push_back(plan->root());
  plan->findNodesOfType(subqueryNodes, ExecutionNode::SUBQUERY, true);

  for (ExecutionNode* subqueryNode : subqueryNodes) {
    SubqueryNode* snode = nullptr;
    ExecutionNode* root = nullptr;  // only used for asserts
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

    // TODO: we might be able to use a walker here?
    while (node != nullptr) {
      auto nodeType = node->getType();

      // loop until we find a modification node or the end of the plan
      while (node != nullptr) {
        // update type
        nodeType = node->getType();

        // check if there is a node type that needs distribution
        if (nodeEligibleForDistribute(nodeType)) {
          // found a node!
          break;
        }

        // there is nothing above us
        if (!node->hasDependency()) {
          // reached the end
          reachedEnd = true;
          break;
        }

        // go further up the tree
        node = node->getFirstDependency();
      }

      if (reachedEnd) {
        // break loop for subquery
        break;
      }

      TRI_ASSERT(node != nullptr);
      if (node == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
      }

      // when we get here, we have found a matching data-modification or
      // traversal/shortest_path/k_shortest_paths node!
      TRI_ASSERT(nodeEligibleForDistribute(nodeType));

      auto const [isSmart, isDisjoint, collection] =
          extractSmartnessAndCollection(node);

#ifdef USE_ENTERPRISE
      if (isSmart) {
        node =
            distributeInClusterRuleSmart(plan.get(), snode, node, wasModified);
        // TODO: MARKUS CHECK WHEN YOU NEED TO CONTINUE HERE!
        //       We want to just handle all smart collections here, so we
        //       probably just want to always continue
        // continue;
      }
#endif

      TRI_ASSERT(collection != nullptr);
      bool const defaultSharding = collection->usesDefaultSharding();

      // If the collection does not use default sharding, we have to use a
      // scatter node this is because we might only have a _key for REMOVE or
      // UPDATE
      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        if (!defaultSharding) {
          // We have to use a ScatterNode.
          node = node->getFirstDependency();
          continue;
        }
      }

      // For INSERT, REPLACE,
      if (isModificationNode(nodeType) ||
          (isGraphNode(nodeType) && isSmart && isDisjoint)) {
        node = insertDistributeGatherSnippet(*plan, node, snode);
        wasModified = true;
      } else {
        node = node->getFirstDependency();
      }
    }  // for node in subquery
  }  // for end subquery in plan
  opt->addPlan(std::move(plan), rule, wasModified);
}

}  // namespace arangodb::aql
