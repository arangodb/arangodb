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
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"
#include "Graph/ShortestPathOptions.h"

#include <absl/strings/str_cat.h>
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
  }    // for end subquery in plan
  opt->addPlan(std::move(plan), rule, wasModified);
}

// Create a new DistributeNode for the ExecutionNode passed in node, and
// register it with the plan
DistributeNode* createDistributeNodeFor(ExecutionPlan& plan,
                                        ExecutionNode* node) {
  auto collection = static_cast<Collection const*>(nullptr);
  auto inputVariable = static_cast<Variable const*>(nullptr);

  bool isTraversalNode = false;
  // TODO: this seems a bit verbose, but is at least local & simple
  //       the modification nodes are all collectionaccessing, the graph nodes
  //       are currently assumed to be disjoint, and hence smart, so all
  //       collections are sharded the same way!
  switch (node->getType()) {
    case ExecutionNode::INSERT: {
      auto const* insertNode = ExecutionNode::castTo<InsertNode const*>(node);
      collection = insertNode->collection();
      inputVariable = insertNode->inVariable();
    } break;
    case ExecutionNode::REMOVE: {
      auto const* removeNode = ExecutionNode::castTo<RemoveNode const*>(node);
      collection = removeNode->collection();
      inputVariable = removeNode->inVariable();
    } break;
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      auto const* updateReplaceNode =
          ExecutionNode::castTo<UpdateReplaceNode const*>(node);
      collection = updateReplaceNode->collection();
      if (updateReplaceNode->inKeyVariable() != nullptr) {
        inputVariable = updateReplaceNode->inKeyVariable();
      } else {
        inputVariable = updateReplaceNode->inDocVariable();
      }
    } break;
    case ExecutionNode::UPSERT: {
      auto upsertNode = ExecutionNode::castTo<UpsertNode const*>(node);
      collection = upsertNode->collection();
      inputVariable = upsertNode->inDocVariable();
    } break;
    case ExecutionNode::TRAVERSAL: {
      auto traversalNode = ExecutionNode::castTo<TraversalNode const*>(node);
      TRI_ASSERT(traversalNode->isDisjoint());
      collection = traversalNode->collection();
      inputVariable = traversalNode->inVariable();
      isTraversalNode = true;
    } break;
    case ExecutionNode::ENUMERATE_PATHS: {
      auto pathsNode = ExecutionNode::castTo<EnumeratePathsNode const*>(node);
      TRI_ASSERT(pathsNode->isDisjoint());
      collection = pathsNode->collection();
      // Subtle: EnumeratePathsNode uses a reference when returning
      // startInVariable
      inputVariable = &pathsNode->startInVariable();
    } break;
    case ExecutionNode::SHORTEST_PATH: {
      auto shortestPathNode =
          ExecutionNode::castTo<ShortestPathNode const*>(node);
      TRI_ASSERT(shortestPathNode->isDisjoint());
      collection = shortestPathNode->collection();
      inputVariable = shortestPathNode->startInVariable();
    } break;
    default: {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat("Cannot distribute ", node->getTypeString(), "."));
    }
  }

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(inputVariable != nullptr);

  // The DistributeNode needs specially prepared input, but we do not want to
  // insert the calculation for that just yet, because it would interfere with
  // some optimizations, in particular those that might completely remove the
  // DistributeNode (which would) also render the calculation pointless. So
  // instead we insert this calculation in a post-processing step when
  // finalizing the plan in the Optimizer.
  auto distNode = plan.createNode<DistributeNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD, collection,
      inputVariable, node->id());

  if (isTraversalNode) {
#ifdef USE_ENTERPRISE
    // Only relevant for Disjoint Smart Graphs that can only be part of the
    // Enterprise version
    // ShortestPath, and K_SHORTEST_PATH will handle satellites differently.
    auto graphNode = ExecutionNode::castTo<GraphNode const*>(node);
    auto vertices = graphNode->vertexColls();
    for (auto const& it : vertices) {
      if (it->isSatellite()) {
        distNode->addSatellite(it);
      }
    }
#endif
  }
  TRI_ASSERT(distNode != nullptr);
  return distNode;
}

// Create a new GatherNode for the DistributeNode passed in node, and
// register it with the plan
//
// TODO: Really Scatter/Gather and Distribute/Gather should be created in pairs.
GatherNode* createGatherNodeFor(ExecutionPlan& plan, DistributeNode* node) {
  auto const collection = node->collection();

  auto const sortMode =
      GatherNode::evaluateSortMode(collection->numberOfShards());
  auto const parallelism = GatherNode::Parallelism::Undefined;
  return plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                     parallelism);
}

//
// for a node `at` of type
//  - INSERT, REMOVE, UPDATE, REPLACE, UPSERT
//  - TRAVERSAL, SHORTEST_PATH, K_SHORTEST_PATHS,
// we transform
//
// parents[0] -> `node` -> deps[0]
//
// into
//
// parents[0] -> GATHER -> REMOTE -> `node` -> REMOTE -> DISTRIBUTE -> deps[0]
//
// We can only handle the above mentioned node types, because the setup of
// distribute and gather requires knowledge from these nodes.
//
// Note that parents[0] might be `nullptr` if `node` is the root of the plan,
// and we handle this case in here as well by resetting the root to the
// inserted GATHER node.
//
DistributeNode* insertDistributeGatherSnippet(ExecutionPlan& plan,
                                              ExecutionNode* at,
                                              SubqueryNode* snode) {
  auto const parents = at->getParents();
  auto const deps = at->getDependencies();

  // This transforms `parents[0] -> node -> deps[0]` into `parents[0] ->
  // deps[0]`
  plan.unlinkNode(at, true);

  // create, and register a distribute node
  DistributeNode* distNode = createDistributeNodeFor(plan, at);
  TRI_ASSERT(distNode != nullptr);
  TRI_ASSERT(deps.size() == 1);
  distNode->addDependency(deps[0]);

  // TODO: This dance is only needed to extract vocbase for
  //       creating the remote node. The vocbase parameter for
  //       the remote node does not seem to be really needed, since
  //       the vocbase is stored in plan (and this variable is actually used in)
  //       some code, so maybe this parameter could be removed?
  auto const* collection = distNode->collection();
  TRI_vocbase_t* vocbase = collection->vocbase();

  // insert a remote node
  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(distNode);

  // re-link with the remote node
  at->addDependency(remoteNode);

  // insert another remote node
  remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(at);

  // insert a gather node matching the distribute node
  auto* gatherNode = createGatherNodeFor(plan, distNode);
  gatherNode->addDependency(remoteNode);

  TRI_ASSERT(parents.size() < 2);
  // Song and dance to deal with at being the root of a plan or a subquery
  if (parents.empty()) {
    if (snode) {
      if (snode->getSubquery() == at) {
        snode->setSubquery(gatherNode, true);
      }
    } else {
      plan.root(gatherNode, true);
    }
  } else {
    // This is correct: Since we transformed `parents[0] -> node -> deps[0]`
    // into `parents[0] -> deps[0]` above, created
    //
    // gather -> remote -> node -> remote -> distribute -> deps[0]
    // and now make the plan consistent again by splicing in our snippet.
    parents[0]->replaceDependency(deps[0], gatherNode);
  }
  return ExecutionNode::castTo<DistributeNode*>(distNode);
}

}  // namespace arangodb::aql
