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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "ScatterViewInCluster.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Cluster/ServerState.h"

namespace arangodb::iresearch {

void scatterViewInClusterRule(aql::Optimizer* opt,
                              std::unique_ptr<aql::ExecutionPlan> plan,
                              aql::OptimizerRule const& rule) {
  using EN = aql::ExecutionNode;

  TRI_ASSERT(ServerState::instance()->isCoordinator());
  bool wasModified = false;
  containers::SmallVector<EN*, 8> nodes;

  // find subqueries
  std::unordered_map<EN*, EN*> subqueries;
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  for (auto& it : nodes) {
    subqueries.try_emplace(
        EN::castTo<aql::SubqueryNode const*>(it)->getSubquery(), it);
  }

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateIResearchViewNode
  nodes.clear();
  plan->findNodesOfType(nodes, EN::ENUMERATE_IRESEARCH_VIEW, true);

  for (auto* node : nodes) {
    TRI_ASSERT(node);
    auto& viewNode = *EN::castTo<IResearchViewNode*>(node);
    auto& options = viewNode.options();

    if (viewNode.empty() ||
        (options.restrictSources && options.sources.empty())) {
      // FIXME we have to invalidate plan cache (if exists)
      // in case if corresponding view has been modified

      // nothing to scatter, view has no associated collections
      // or node is restricted to empty collection list
      continue;
    }

    auto const& parents = node->getParents();
    // intentional copy of the dependencies, as we will be modifying
    // dependencies later on
    auto const deps = node->getDependencies();
    TRI_ASSERT(deps.size() == 1);

    // don't do this if we are already distributing!
    if (deps[0]->getType() == EN::REMOTE) {
      auto const* firstDep = deps[0]->getFirstDependency();
      if (!firstDep || firstDep->getType() == EN::DISTRIBUTE) {
        continue;
      }
    }

    if (plan->shouldExcludeFromScatterGather(node)) {
      continue;
    }

    auto& vocbase = viewNode.vocbase();

    bool const isRootNode = plan->isRoot(node);
    plan->unlinkNode(node, true);

    // insert a scatter node
    auto scatterNode = plan->registerNode(std::make_unique<aql::ScatterNode>(
        plan.get(), plan->nextId(), aql::ScatterNode::ScatterType::SHARD));
    TRI_ASSERT(!deps.empty());
    scatterNode->addDependency(deps[0]);

    // insert a remote node
    auto* remoteNode = plan->registerNode(std::make_unique<aql::RemoteNode>(
        plan.get(), plan->nextId(), &vocbase, "", "", ""));
    TRI_ASSERT(scatterNode);
    remoteNode->addDependency(scatterNode);
    node->addDependency(remoteNode);  // re-link with the remote node

    // insert another remote node
    remoteNode = plan->registerNode(std::make_unique<aql::RemoteNode>(
        plan.get(), plan->nextId(), &vocbase, "", "", ""));
    TRI_ASSERT(node);
    remoteNode->addDependency(node);

    // so far we don't know the exact number of db servers where
    // this query will be distributed, mode will be adjusted
    // during query distribution phase by EngineInfoContainerDBServer
    auto const sortMode = aql::GatherNode::SortMode::Default;

    // insert gather node
    auto* gatherNode = plan->registerNode(std::make_unique<aql::GatherNode>(
        plan.get(), plan->nextId(), sortMode));
    TRI_ASSERT(remoteNode);
    gatherNode->addDependency(remoteNode);

    // and now link the gather node with the rest of the plan
    if (parents.size() == 1) {
      parents[0]->replaceDependency(deps[0], gatherNode);
    }

    // check if the node that we modified was at the end of a subquery
    auto it = subqueries.find(node);

    if (it != subqueries.end()) {
      auto* subQueryNode = EN::castTo<aql::SubqueryNode*>((*it).second);
      subQueryNode->setSubquery(gatherNode, true);
    }

    if (isRootNode) {
      // if we replaced the root node, set a new root node
      plan->root(gatherNode);
    }

    wasModified = true;
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

}  // namespace arangodb::iresearch
