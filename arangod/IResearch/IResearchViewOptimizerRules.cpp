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
#include "IResearchViewNode.h"
#include "IResearchFilterFactory.h"
#include "IResearchOrderFactory.h"
#include "AqlHelper.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Condition.h"
#include "Aql/Query.h"
#include "Aql/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ServerState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb::iresearch;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

size_t numberOfShards(
    arangodb::CollectionNameResolver const& resolver,
    arangodb::LogicalView const& view
) {
  size_t numberOfShards = 0;

  auto visitor = [&numberOfShards](
      arangodb::LogicalCollection const& collection
  ) noexcept {
    numberOfShards += collection.numberOfShards();
    return true;
  };

  resolver.visitCollections(visitor, view.id());

  return numberOfShards;
}

std::vector<arangodb::iresearch::IResearchSort> buildSort(
    ExecutionPlan const& plan,
    arangodb::aql::Variable const& ref,
    std::vector<std::pair<Variable const*, bool>> const& sorts,
    std::map<VariableId, AstNode const*> const& vars,
    bool scorersOnly
) {
  std::vector<IResearchSort> entries;

  QueryContext const ctx { nullptr, nullptr, nullptr, nullptr, &ref };

  for (auto& sort : sorts) {
    auto const* var = sort.first;
    auto varId = var->id;

    AstNode const* rootNode = nullptr;
    auto it = vars.find(varId);

    if (it != vars.end()) {
      auto const* node = rootNode = it->second;

      while (node && NODE_TYPE_ATTRIBUTE_ACCESS == node->type) {
        node = node->getMember(0);
      }

      if (node && NODE_TYPE_REFERENCE == node->type) {
        var = reinterpret_cast<Variable const*>(node->getData());
      }
    } else {
      auto const* setter = plan.getVarSetBy(varId);
      if (setter && EN::CALCULATION == setter->getType()) {
        auto const* expr = ExecutionNode::castTo<CalculationNode const*>(setter)->expression();

        if (expr) {
          rootNode = expr->node();
        }
      }
    }

    if (var && rootNode && (!scorersOnly || OrderFactory::scorer(nullptr, *rootNode, ctx))) {
      entries.emplace_back(var, rootNode, sort.second);
    }
  }

  return entries;
}

bool addView(
    arangodb::LogicalView const& view,
    arangodb::aql::Query& query
) {
  auto* collections = query.collections();

  if (!collections) {
    return false;
  }

  // linked collections
  auto visitor = [&query](TRI_voc_cid_t cid) {
    query.addCollection(
      arangodb::basics::StringUtils::itoa(cid),
      arangodb::AccessMode::Type::READ
    );
    return true;
  };

  return view.visitCollections(visitor);
}

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewConditionFinder
///////////////////////////////////////////////////////////////////////////////
class IResearchViewConditionHandler final
    : public arangodb::aql::WalkerWorker<ExecutionNode> {
 public:
  IResearchViewConditionHandler(
      ExecutionPlan& plan,
      std::set<arangodb::iresearch::IResearchViewNode const *>& processedViewNodes) noexcept
   : _plan(&plan),
     _processedViewNodes(&processedViewNodes) {
  }

  virtual bool before(ExecutionNode*) override;

  virtual bool enterSubquery(ExecutionNode*, ExecutionNode*) override {
    return false;
  }

 private:
  bool handleFilterCondition(
    ExecutionNode* en,
    Condition& condition
  );

  ExecutionPlan* _plan;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  // map and set are 25-30% faster than corresponding
  // unordered_set for small number of elements
  std::map<VariableId, AstNode const*> _variableDefinitions;
  std::set<arangodb::iresearch::IResearchViewNode const*>* _processedViewNodes;
}; // IResearchViewConditionFinder

bool IResearchViewConditionHandler::before(ExecutionNode* en) {
  switch (en->getType()) {
    case EN::LIMIT:
      // LIMIT invalidates the sort expression we already found
      _sorts.clear();
      break;

    case EN::SINGLETON:
    case EN::NORESULTS:
      // in all these cases we better abort
      return true;

    case EN::SORT: {
      // register which variables are used in a SORT
      if (_sorts.empty()) {
        for (auto& it : EN::castTo<SortNode const*>(en)->elements()) {
          _sorts.emplace_back(it.var, it.ascending);
          TRI_IF_FAILURE("IResearchViewConditionFinder::sortNode") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
      break;
    }

    case EN::CALCULATION: {
      auto outvars = en->getVariablesSetHere();
      TRI_ASSERT(outvars.size() == 1);

      _variableDefinitions.emplace(
          outvars[0]->id,
          EN::castTo<CalculationNode const*>(en)->expression()->node());
      TRI_IF_FAILURE("IResearchViewConditionFinder::variableDefinition") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      break;
    }

    case EN::ENUMERATE_IRESEARCH_VIEW: {
      auto node = EN::castTo<IResearchViewNode*>(en);
      auto& view = *node->view();

      // add view and linked collections to the query
      TRI_ASSERT(_plan && _plan->getAst() && _plan->getAst()->query());
      if (!addView(view, *_plan->getAst()->query())) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          "failed to process all collections linked with the view '" + view.name() + "'"
        );
      }

      if (_processedViewNodes->find(node) != _processedViewNodes->end()) {
        // already optimized this node
        break;
      }

      Condition filterCondition(_plan->getAst());

      if (!node->filterConditionIsEmpty()) {
        filterCondition.andCombine(&node->filterCondition());

        if (!handleFilterCondition(en, filterCondition)) {
          break;
        }
      }

      auto sortCondition = buildSort(
        *_plan,
        node->outVariable(),
        _sorts,
        _variableDefinitions,
        true  // node->isInInnerLoop() // build scorers only in case if we're inside a loop
      );

      if (filterCondition.isEmpty() && sortCondition.empty()) {
        // no conditions left
        break;
      }

      auto const canUseView = !filterCondition.root() || FilterFactory::filter(
        nullptr,
        { nullptr, nullptr, nullptr, nullptr, &node->outVariable() },
        *filterCondition.root()
      );

      if (!canUseView) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          "unsupported SEARCH condition"
        );
      }

      node->filterCondition(filterCondition.root());
      node->sortCondition(std::move(sortCondition));

      TRI_IF_FAILURE("IResearchViewConditionFinder::insertViewNode") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      _processedViewNodes->insert(node);

      break;
    }

    default:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;
  }

  return false;
}

bool IResearchViewConditionHandler::handleFilterCondition(
    ExecutionNode* en,
    Condition& condition) {
  // normalize the condition
  condition.normalize(_plan);
  TRI_IF_FAILURE("ConditionFinder::normalizePlan") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (condition.isEmpty()) {
    // condition is always false
    for (auto const& x : en->getParents()) {
      auto noRes = new NoResultsNode(_plan, _plan->nextId());
      _plan->registerNode(noRes);
      _plan->insertDependency(x, noRes);
    }
    return false;
  }

  auto const& varsValid = en->getVarsValid();

  // remove all invalid variables from the condition
  if (condition.removeInvalidVariables(varsValid)) {
    // removing left a previously non-empty OR block empty...
    // this means we can't use the index to restrict the results
    return false;
  }

  return true;
}

}

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/// @brief move filters and sort conditions into views
void handleViewsRule(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const* rule
) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  // try to find `EnumerateViewNode`s and process corresponding filters and sorts
  plan->findEndNodes(nodes, true);

  // set of processed view nodes
  std::set<IResearchViewNode const*> processedViewNodes;

  IResearchViewConditionHandler handler(*plan, processedViewNodes);
  for (auto const& n : nodes) {
    n->walk(handler);
  }

  if (!processedViewNodes.empty()) {
    std::unordered_set<ExecutionNode*> toUnlink;

    // remove sort setters covered by a view internally
    for (auto* viewNode : processedViewNodes) {
      TRI_ASSERT(viewNode);

      for (auto const& sort : viewNode->sortCondition()) {
        auto const* var = sort.var;

        if (!var) {
          continue;
        }

        auto* setter = plan->getVarSetBy(var->id);

        if (!setter || EN::CALCULATION != setter->getType()) {
          continue;
        }

        toUnlink.emplace(setter);
      }
    }

    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !processedViewNodes.empty());
}

void scatterViewInClusterRule(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const* rule
) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  // find subqueries
  std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;
  plan->findNodesOfType(nodes, ExecutionNode::SUBQUERY, true);

  for (auto& it : nodes) {
    subqueries.emplace(
      EN::castTo<SubqueryNode const*>(it)->getSubquery(), it
    );
  }

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateIResearchViewNode
  nodes.clear();
  plan->findNodesOfType(nodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  TRI_ASSERT(
    plan->getAst()
      && plan->getAst()->query()
      && plan->getAst()->query()->trx()
  );
  auto* resolver = plan->getAst()->query()->trx()->resolver();
  TRI_ASSERT(resolver);

  for (auto* node : nodes) {
    TRI_ASSERT(node);
    auto& viewNode = *EN::castTo<IResearchViewNode*>(node);

    if (viewNode.empty()) {
      // FIXME we have to invalidate plan cache (if exists)
      // in case if corresponding view has been modified

      // view has no associated collection, nothing to scatter
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
    auto& view = *viewNode.view();

    bool const isRootNode = plan->isRoot(node);
    plan->unlinkNode(node, true);

    // insert a scatter node
    auto scatterNode = plan->registerNode(
      std::make_unique<ScatterNode>(
        plan.get(), plan->nextId()
    ));
    TRI_ASSERT(!deps.empty());
    scatterNode->addDependency(deps[0]);

    // insert a remote node
    auto* remoteNode = plan->registerNode(
      std::make_unique<RemoteNode>(
        plan.get(),
        plan->nextId(),
        &vocbase,
        "", "", ""
    ));
    TRI_ASSERT(scatterNode);
    remoteNode->addDependency(scatterNode);
    node->addDependency(remoteNode); // re-link with the remote node

    // insert another remote node
    remoteNode = plan->registerNode(
      std::make_unique<RemoteNode>(
        plan.get(),
        plan->nextId(),
        &vocbase,
        "", "", ""
    ));
    TRI_ASSERT(node);
    remoteNode->addDependency(node);

    // insert gather node
    auto const sortMode = GatherNode::evaluateSortMode(
      numberOfShards(*resolver, view)
    );
    auto* gatherNode = plan->registerNode(
      std::make_unique<GatherNode>(
        plan.get(),
        plan->nextId(),
        sortMode
    ));
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

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
