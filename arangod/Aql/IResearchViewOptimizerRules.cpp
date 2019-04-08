////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "IResearchViewOptimizerRules.h"

#include "Aql/ClusterNodes.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include "utils/misc.hpp"

using namespace arangodb::iresearch;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

bool addView(arangodb::LogicalView const& view, arangodb::aql::Query& query) {
  auto* collections = query.collections();

  if (!collections) {
    return false;
  }

  // linked collections
  auto visitor = [&query](TRI_voc_cid_t cid) {
    query.addCollection(arangodb::basics::StringUtils::itoa(cid),
                        arangodb::AccessMode::Type::READ);
    return true;
  };

  return view.visitCollections(visitor);
}

bool optimizeSearchCondition(IResearchViewNode& viewNode, Query& query, ExecutionPlan& plan) {
  auto view = viewNode.view();

  // add view and linked collections to the query
  if (!addView(*view, query)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_PARSE,
        "failed to process all collections linked with the view '" +
            view->name() + "'");
  }

  // build search condition
  Condition searchCondition(plan.getAst());

  if (!viewNode.filterConditionIsEmpty()) {
    searchCondition.andCombine(&viewNode.filterCondition());
    searchCondition.normalize(&plan, true);  // normalize the condition

    if (searchCondition.isEmpty()) {
      // condition is always false
      for (auto const& x : viewNode.getParents()) {
        plan.insertDependency(x, plan.registerNode(
                                     std::make_unique<NoResultsNode>(&plan, plan.nextId())));
      }
      return false;
    }

    auto const& varsValid = viewNode.getVarsValid();

    // remove all invalid variables from the condition
    if (searchCondition.removeInvalidVariables(varsValid)) {
      // removing left a previously non-empty OR block empty...
      // this means we can't use the index to restrict the results
      return false;
    }
  }

  // check filter condition
  auto const conditionValid = !searchCondition.root() || FilterFactory::filter(
    nullptr,
    { query.trx(), nullptr, nullptr, nullptr, &viewNode.outVariable() },
    *searchCondition.root()
  );

  if (!conditionValid) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "unsupported SEARCH condition");
  }

  if (!searchCondition.isEmpty()) {
    viewNode.filterCondition(searchCondition.root());
  }

  return true;
}
    
bool optimizeSort(IResearchViewNode& viewNode, ExecutionPlan* plan) {
  std::unordered_map<VariableId, AstNode const*> variableDefinitions;

  ExecutionNode* current = static_cast<ExecutionNode*>(&viewNode);

  while (true) {
    current = current->getFirstParent();

    if (current == nullptr) {
      // we are at the bottom end of the plan
      return false;
    }
      
    if (current->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
        current->getType() == EN::ENUMERATE_COLLECTION ||
        current->getType() == EN::TRAVERSAL ||
        current->getType() == EN::SHORTEST_PATH ||
        current->getType() == EN::INDEX ||
        current->getType() == EN::COLLECT) {
      // any of these node types will lead to more/less results in the output,
      // and may as well change the sort order, so let's better abort here
      return false;
    }
        
    if (current->getType() == EN::CALCULATION) {
      // pick up the meanings of variables as we walk the plan
      variableDefinitions.emplace(
          ExecutionNode::castTo<CalculationNode const*>(current)->outVariable()->id,
          ExecutionNode::castTo<CalculationNode const*>(current)->expression()->node());
    }

    if (current->getType() != EN::SORT) {
      // from here on, we are only interested in sorts
      continue;
    }
    
    std::vector<std::pair<Variable const*, bool>> sorts;

    auto* sortNode = ExecutionNode::castTo<SortNode*>(current);
    auto const& sortElements = sortNode->elements();

    for (auto& it : sortElements) {
      // note: in contrast to regular indexes, views support sorting in different 
      // directions for multiple fields (e.g. SORT doc.a ASC, doc.b DESC). 
      // this is not supported by indexes
      sorts.emplace_back(it.var, it.ascending);
    }

    SortCondition sortCondition(plan, 
                                sorts,
                                std::vector<std::vector<arangodb::basics::AttributeName>>(),
                                variableDefinitions);

    if (sortCondition.isEmpty() || !sortCondition.isOnlyAttributeAccess()) {
      // unusable sort condition
      return false;
    }
    
    // sort condition found, and sorting only by attributes!

    // TODO: get the view's sort fields and orders from the actual view here.
    // for now, there are hard-coded to value1 ASC, value2 DESC
    std::vector<std::vector<arangodb::basics::AttributeName>> viewSortFields;
      
    // vector<bool> is not nice, but will do the job here
    std::vector<bool> viewSortOrders;

    {
      // auto const& view = viewNode.view();
      viewSortFields.emplace_back();
      viewSortFields.back().emplace_back(arangodb::basics::AttributeName("value1", false));
      viewSortFields.emplace_back();
      viewSortFields.back().emplace_back(arangodb::basics::AttributeName("value2", false));
  
      viewSortOrders.push_back(true); // ascending
      viewSortOrders.push_back(false); // descending
    }

    TRI_ASSERT(viewSortFields.size() == viewSortOrders.size());

    if (sortCondition.numAttributes() > viewSortFields.size()) {
      // the SORT condition in the query has more attributes than the view
      // is sorted by. we cannot optimize in this case
      return false;
    }
    
    // check if all sort conditions match
    for (size_t i = 0; i < sortElements.size(); ++i) {
      if (sortElements[i].ascending != viewSortOrders[i]) {
        // view is sorted in different order than requested in SORT condition
        return false;
      }
    }

    // all sort orders equal!
    // now finally check how many of the SORT conditions attributes we cover
    size_t numCovered = sortCondition.coveredAttributes(&viewNode.outVariable(), viewSortFields);

    if (numCovered < sortNode->elements().size()) {
      // the sort is not covered by the view
      return false;
    }
    
    // we are almost done... but we need to do a final check and verify that our
    // sort node itself is not followed by another node that injects more data into
    // the result or that re-sorts it
    while (current->hasParent()) {
      current = current->getFirstParent();
      if (current->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
          current->getType() == EN::ENUMERATE_COLLECTION ||
          current->getType() == EN::TRAVERSAL ||
          current->getType() == EN::SHORTEST_PATH ||
          current->getType() == EN::INDEX ||
          current->getType() == EN::COLLECT ||
          current->getType() == EN::SORT) {
        // any of these node types will lead to more/less results in the output,
        // and may as well change the sort order, so let's better abort here
        return false;
      }
    }
  
    LOG_TOPIC("affe4", WARN, arangodb::Logger::FIXME) << "found a SORT statement (#" << sortNode->id() << ") to optimize away";
    plan->unlinkNode(sortNode); 
    return true;
  }
}

}  // namespace

namespace arangodb {
namespace iresearch {

/// @brief move filters and sort conditions into views
void handleViewsRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const* rule) {
  TRI_ASSERT(plan && plan->getAst() && plan->getAst()->query());
  auto& query = *plan->getAst()->query();

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  auto addPlan = irs::make_finally([opt, &plan, rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  // replace scorers in all calculation nodes with references
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  ScorerReplacer scorerReplacer;

  for (auto* node : nodes) {
    TRI_ASSERT(node && EN::CALCULATION == node->getType());

    scorerReplacer.replace(*EN::castTo<CalculationNode*>(node));
  }

  // register replaced scorers to be evaluated by corresponding view nodes
  nodes.clear();
  plan->findNodesOfType(nodes, EN::ENUMERATE_IRESEARCH_VIEW, true);

  std::vector<Scorer> scorers;

  for (auto* node : nodes) {
    TRI_ASSERT(node && EN::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *EN::castTo<IResearchViewNode*>(node);
    
    if (!viewNode.isInInnerLoop()) {
      // check if we can optimize away a sort that follows the EnumerateView node
      // this is only possible if the view node itself is not contained in another loop
      if (optimizeSort(viewNode, plan.get())) {
        modified = true;
      }
    }

    if (!optimizeSearchCondition(viewNode, query, *plan)) {
      continue;
    }

    // find scorers that have to be evaluated by a view
    scorerReplacer.extract(viewNode.outVariable(), scorers);
    viewNode.scorers(std::move(scorers));

    modified = true;
  }

  // ensure all replaced scorers are covered by corresponding view nodes
  scorerReplacer.visit([](Scorer const& scorer) -> bool {
    TRI_ASSERT(scorer.node);
    auto const funcName = iresearch::getFuncName(*scorer.node);

    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "Non ArangoSearch view variable '%s' is used in scorer function '%s'",
        scorer.var->name.c_str(), funcName.c_str());
  });
}

void scatterViewInClusterRule(arangodb::aql::Optimizer* opt,
                              std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                              arangodb::aql::OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  // find subqueries
  std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;
  plan->findNodesOfType(nodes, ExecutionNode::SUBQUERY, true);

  for (auto& it : nodes) {
    subqueries.emplace(EN::castTo<SubqueryNode const*>(it)->getSubquery(), it);
  }

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateIResearchViewNode
  nodes.clear();
  plan->findNodesOfType(nodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  TRI_ASSERT(plan->getAst() && plan->getAst()->query() &&
             plan->getAst()->query()->trx());
  auto* resolver = plan->getAst()->query()->trx()->resolver();
  TRI_ASSERT(resolver);

  for (auto* node : nodes) {
    TRI_ASSERT(node);
    auto& viewNode = *EN::castTo<IResearchViewNode*>(node);
    auto& options = viewNode.options();

    if (viewNode.empty() || (options.restrictSources && options.sources.empty())) {
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
    if (deps[0]->getType() == ExecutionNode::REMOTE) {
      auto const* firstDep = deps[0]->getFirstDependency();
      if (!firstDep || firstDep->getType() == ExecutionNode::DISTRIBUTE) {
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
    auto scatterNode =
        plan->registerNode(std::make_unique<ScatterNode>(plan.get(), plan->nextId()));
    TRI_ASSERT(!deps.empty());
    scatterNode->addDependency(deps[0]);

    // insert a remote node
    auto* remoteNode =
        plan->registerNode(std::make_unique<RemoteNode>(plan.get(), plan->nextId(),
                                                        &vocbase, "", "", ""));
    TRI_ASSERT(scatterNode);
    remoteNode->addDependency(scatterNode);
    node->addDependency(remoteNode);  // re-link with the remote node

    // insert another remote node
    remoteNode =
        plan->registerNode(std::make_unique<RemoteNode>(plan.get(), plan->nextId(),
                                                        &vocbase, "", "", ""));
    TRI_ASSERT(node);
    remoteNode->addDependency(node);

    // so far we don't know the exact number of db servers where
    // this query will be distributed, mode will be adjusted
    // during query distribution phase by EngineInfoContainerDBServer
    auto const sortMode = GatherNode::SortMode::Default;

    // insert gather node
    auto* gatherNode = plan->registerNode(
        std::make_unique<GatherNode>(plan.get(), plan->nextId(), sortMode));
    TRI_ASSERT(remoteNode);
    gatherNode->addDependency(remoteNode);

    // and now link the gather node with the rest of the plan
    if (parents.size() == 1) {
      parents[0]->replaceDependency(deps[0], gatherNode);
    }

    // check if the node that we modified was at the end of a subquery
    auto it = subqueries.find(node);

    if (it != subqueries.end()) {
      auto* subQueryNode = EN::castTo<SubqueryNode*>((*it).second);
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

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
