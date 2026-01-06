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
/// @author Julia Puget
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRuleScatterInCluster.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    scatterInClusterNodeTypes{
        arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
        arangodb::aql::ExecutionNode::ENUMERATE_NEAR_VECTORS,
        arangodb::aql::ExecutionNode::INDEX,
        arangodb::aql::ExecutionNode::MATERIALIZE,
        arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
        arangodb::aql::ExecutionNode::INSERT,
        arangodb::aql::ExecutionNode::UPDATE,
        arangodb::aql::ExecutionNode::REPLACE,
        arangodb::aql::ExecutionNode::REMOVE,
        arangodb::aql::ExecutionNode::UPSERT};

auto extractVocbaseFromNode(ExecutionNode* at) -> TRI_vocbase_t* {
  auto collectionAccessingNode =
      dynamic_cast<CollectionAccessingNode const*>(at);

  if (collectionAccessingNode != nullptr) {
    return collectionAccessingNode->vocbase();
  } else if (at->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
    // Really? Yes, the & below is correct.
    return &ExecutionNode::castTo<iresearch::IResearchViewNode const*>(at)
                ->vocbase();
  }

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "Cannot determine vocbase for execution node.");
}

// Sets up a Gather node for scatterInClusterRule.
//
// Each of EnumerateCollectionNode, IndexNode, IResearchViewNode, and
// ModificationNode needs slightly different treatment.
//
// In an ideal world the node itself would know how to compute these parameters
// for GatherNode (sortMode, parallelism, and elements), and we'd just ask it.
//
// firstDependency servers to indicate what was the first dependency to the
// execution node before it got unlinked and linked with remote nodes
auto insertGatherNode(
    ExecutionPlan& plan, ExecutionNode* node,
    containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> const&
        subqueries) -> GatherNode* {
  TRI_ASSERT(node);

  GatherNode* gatherNode{nullptr};

  auto nodeType = node->getType();
  switch (nodeType) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      auto collection =
          ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
              ->collection();
      auto numberOfShards = collection->numberOfShards();

      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);
    } break;
    case ExecutionNode::INDEX: {
      auto idxNode = ExecutionNode::castTo<IndexNode const*>(node);
      auto collection = idxNode->collection();
      TRI_ASSERT(collection != nullptr);
      auto numberOfShards = collection->numberOfShards();

      auto elements = [&] {
        auto allIndexes = idxNode->getIndexes();
        TRI_ASSERT(!allIndexes.empty());

        // Using Index for sort only works if all indexes are equal.
        auto const& first = allIndexes[0];

        // also check if we actually need to bother about the sortedness of the
        // result, or if we use the index for filtering only
        if (first->isSorted() && idxNode->needsGatherNodeSort()) {
          for (auto const& it : allIndexes) {
            if (first != it) {
              return SortElementVector{};
            }
          }
        }

        return idxNode->getSortElements();
      }();

      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);

      if (!elements.empty() && numberOfShards != 1) {
        gatherNode->elements(elements);
      }
      return gatherNode;
    }
    case ExecutionNode::MATERIALIZE: {
      auto const* materializeNode =
          ExecutionNode::castTo<materialize::MaterializeNode const*>(node);
      auto const* maybeEnumerateNearVectorNode =
          plan.getVarSetBy(materializeNode->outVariable().id);

      if (maybeEnumerateNearVectorNode != nullptr &&
          maybeEnumerateNearVectorNode->getType() ==
              ExecutionNode::ENUMERATE_NEAR_VECTORS) {
        auto const* enumerateNearVectorNode =
            ExecutionNode::castTo<EnumerateNearVectorNode const*>(
                maybeEnumerateNearVectorNode);
        auto elements = SortElementVector{};
        auto const* collection = enumerateNearVectorNode->collection();
        TRI_ASSERT(collection != nullptr);
        auto numberOfShards = collection->numberOfShards();

        Variable const* sortVariable =
            enumerateNearVectorNode->distanceOutVariable();
        elements.push_back(SortElement::createWithPath(
            sortVariable, enumerateNearVectorNode->isAscending(), {}));

        auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
        auto parallelism = GatherNode::evaluateParallelism(*collection);

        gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                                 parallelism);

        if (!elements.empty() && numberOfShards != 1) {
          gatherNode->elements(elements);
        }
        return gatherNode;
      }
      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(),
                                               GatherNode::SortMode::Default);
      break;
    }
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPSERT: {
      auto collection =
          ExecutionNode::castTo<ModificationNode*>(node)->collection();

      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        // Note that in the REPLACE or UPSERT case we are not getting here,
        // since the distributeInClusterRule fires and a DistributionNode is
        // used.
        auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
        modNode->getOptions().ignoreDocumentNotFound = true;
      }

      auto numberOfShards = collection->numberOfShards();
      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);
      break;
    }
    default: {
      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(),
                                               GatherNode::SortMode::Default);

      break;
    }
  }

  auto it = subqueries.find(node);
  if (it != subqueries.end()) {
    ExecutionNode::castTo<SubqueryNode*>((*it).second)
        ->setSubquery(gatherNode, true);
  }

  return gatherNode;
}

// replace
//
// A -> at -> B
//
// by
//
// A -> SCATTER -> REMOTE -> at -> REMOTE -> GATHER -> B
//
// in plan
//
// gatherNode is a parameter because it needs to be configured depending
// on they type of `at`, in particular at the moment this configuration
// uses a list of subqueries which are precomputed at the beginning
// of the optimizer rule; once that list is gone the configuration of the
// gather node can be moved into this function.

}  // namespace

namespace arangodb::aql {

void insertScatterGatherSnippet(
    ExecutionPlan& plan, ExecutionNode* at,
    containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> const&
        subqueries) {
  // TODO: necessary?
  TRI_vocbase_t* vocbase = extractVocbaseFromNode(at);
  auto const isRootNode = plan.isRoot(at);
  auto const nodeDependencies = at->getDependencies();
  auto const nodeParents = at->getParents();

  // Unlink node from plan, note that we allow removing the root node
  plan.unlinkNode(at, true);

  auto* scatterNode = plan.createNode<ScatterNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD);

  TRI_ASSERT(at->getDependencies().empty());
  TRI_ASSERT(!nodeDependencies.empty());
  scatterNode->addDependency(nodeDependencies[0]);

  // insert REMOTE
  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(scatterNode);

  // Wire in `at`
  at->addDependency(remoteNode);

  // insert (another) REMOTE
  remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  TRI_ASSERT(at);
  remoteNode->addDependency(at);

  // GATHER needs some setup, so this happens in a separate function
  auto gatherNode = insertGatherNode(plan, at, subqueries);
  TRI_ASSERT(gatherNode);
  TRI_ASSERT(remoteNode);
  gatherNode->addDependency(remoteNode);

  // Link the gather node with the rest of the plan (if we have any)
  // TODO: what other cases can occur here?
  if (nodeParents.size() == 1) {
    nodeParents[0]->replaceDependency(nodeDependencies[0], gatherNode);
  }

  if (isRootNode) {
    // if we replaced the root node, set a new root node
    plan.root(gatherNode);
  }
}

// Moves a SCATTER/REMOTE from below `at` (where it was previously inserted by
// scatterInClusterRule), to just above `at`, because `at` was marked as
// excludeFromScatter by the smartJoinRule.
void moveScatterAbove(ExecutionPlan& plan, ExecutionNode* at) {
  TRI_vocbase_t* vocbase = extractVocbaseFromNode(at);

  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  plan.insertBefore(at, remoteNode);

  ExecutionNode* scatterNode = plan.createNode<ScatterNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD);
  plan.insertBefore(remoteNode, scatterNode);

  // There must be a SCATTER/REMOTE block south of us, which was inserted by
  // an earlier iteration in scatterInClusterRule.
  // We remove that block, effectively moving the SCATTER/REMOTE past the
  // current node
  // The effect is that in a SmartJoin we get joined up nodes that are
  // all executed on the DB-Server
  auto found = false;
  auto* current = at->getFirstParent();
  while (current != nullptr) {
    if (current->getType() == ExecutionNode::SCATTER) {
      auto* next = current->getFirstParent();
      if (next != nullptr && next->getType() == ExecutionNode::REMOTE) {
        plan.unlinkNode(current, true);
        plan.unlinkNode(next, true);
        found = true;
        break;
      } else {
        // If we have a SCATTER node, we also have to have a REMOTE node
        // otherwise the plan is inconsistent.
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Inconsistent plan.");
      }
    }
    current = current->getFirstParent();
  }
  if (!found) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    plan.show();
#endif
    // TODO: maybe we should *not* throw in maintainer mode, as the optimizer
    //       gives more useful error messages?
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Inconsistent plan.");
  }
}

// TODO: move into ExecutionPlan?
// TODO: Is this still needed after register planning is refactored?
// Find all Subquery Nodes
void findSubqueriesInPlan(
    ExecutionPlan& plan,
    containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*>& subqueries) {
  containers::SmallVector<ExecutionNode*, 8> subs;
  plan.findNodesOfType(subs, ExecutionNode::SUBQUERY, true);

  for (auto& it : subs) {
    subqueries.emplace(
        ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery(), it);
  }
}

}  // namespace

/// @brief scatter operations in cluster
/// this rule inserts scatter, gather and remote nodes so operations on
/// sharded collections actually work
/// it will change plans in place
void arangodb::aql::scatterInClusterRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  // We cache the subquery map to not compute it over and over again
  // It is needed to setup the gather node later on
  containers::SmallUnorderedMap<ExecutionNode*,
                                ExecutionNode*>::allocator_type::arena_type
      subqueriesArena;
  containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> subqueries{
      subqueriesArena};
  findSubqueriesInPlan(*plan, subqueries);

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateCollectionNode, IndexNode, IResearchViewNode, and modification
  // nodes
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::scatterInClusterNodeTypes, true);

  TRI_ASSERT(plan->getAst());

  for (auto& node : nodes) {
    // found a node we need to replace in the plan

    auto const deps = node->getDependencies();
    TRI_ASSERT(deps.size() == 1);

    // don't do this if we are already distributing!
    if (deps[0]->getType() == ExecutionNode::REMOTE &&
        deps[0]->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE) {
      continue;
    }

    // TODO: sonderlocke for ENUMERATE_IRESEARCH_VIEW to skip views that are
    // empty Can this be done better?
    if (node->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
      auto& viewNode =
          *ExecutionNode::castTo<iresearch::IResearchViewNode*>(node);
      auto& options = viewNode.options();

      if (viewNode.empty() ||
          (options.restrictSources && options.sources.empty())) {
        // nothing to scatter, view has no associated collections
        // or node is restricted to empty collection list
        continue;
      }
    }

    if (plan->shouldExcludeFromScatterGather(node)) {
      // If the smart-joins rule marked this node as not requiring a full
      // scatter..gather setup, we move the scatter/remote from below above
      moveScatterAbove(*plan, node);
    } else {
      // insert a full SCATTER/GATHER
      insertScatterGatherSnippet(*plan, node, subqueries);
    }
    wasModified = true;
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}
