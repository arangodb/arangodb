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
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/WalkerWorker.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <utils/misc.hpp>

using namespace arangodb::iresearch;
using namespace arangodb::aql;

namespace {

inline IResearchViewSort const& primarySort(arangodb::LogicalView const& view) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto& viewImpl = arangodb::LogicalView::cast<IResearchViewCoordinator>(view);
    return viewImpl.primarySort();
  }

  auto& viewImpl = arangodb::LogicalView::cast<IResearchView>(view);
  return viewImpl.primarySort();
}

bool addView(arangodb::LogicalView const& view, Query& query) {
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

  // check filter condition if present
  if (searchCondition.root()) {
    auto filterCreated = FilterFactory::filter(
      nullptr,
      { query.trx(), nullptr, nullptr, nullptr, &viewNode.outVariable() },
      *searchCondition.root());

    if (filterCreated.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(filterCreated.errorNumber(),
                                     "unsupported SEARCH condition: " + filterCreated.errorMessage());
    }
  }

  if (!searchCondition.isEmpty()) {
    viewNode.filterCondition(searchCondition.root());
  }

  return true;
}

bool optimizeSort(IResearchViewNode& viewNode, ExecutionPlan* plan) {
  TRI_ASSERT(viewNode.view());
  auto& primarySort = ::primarySort(*viewNode.view());

  if (primarySort.empty()) {
    // use system sort
    return false;
  }

  std::unordered_map<VariableId, AstNode const*> variableDefinitions;

  ExecutionNode* current = static_cast<ExecutionNode*>(&viewNode);

  while (true) {
    current = current->getFirstParent();

    if (current == nullptr) {
      // we are at the bottom end of the plan
      return false;
    }

    if (current->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW ||
        current->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
        current->getType() == ExecutionNode::TRAVERSAL ||
        current->getType() == ExecutionNode::SHORTEST_PATH ||
        current->getType() == ExecutionNode::K_SHORTEST_PATHS ||
        current->getType() == ExecutionNode::INDEX ||
        current->getType() == ExecutionNode::COLLECT) {
      // any of these node types will lead to more/less results in the output,
      // and may as well change the sort order, so let's better abort here
      return false;
    }

    if (current->getType() == ExecutionNode::CALCULATION) {
      // pick up the meanings of variables as we walk the plan
      variableDefinitions.try_emplace(
          ExecutionNode::castTo<CalculationNode const*>(current)->outVariable()->id,
          ExecutionNode::castTo<CalculationNode const*>(current)->expression()->node());
    }

    if (current->getType() != ExecutionNode::SORT) {
      // from here on, we are only interested in sorts
      continue;
    }

    std::vector<std::pair<Variable const*, bool>> sorts;

    auto* sortNode = ExecutionNode::castTo<SortNode*>(current);
    auto const& sortElements = sortNode->elements();

    sorts.reserve(sortElements.size());
    for (auto& it : sortElements) {
      // note: in contrast to regular indexes, views support sorting in different
      // directions for multiple fields (e.g. SORT doc.a ASC, doc.b DESC).
      // this is not supported by indexes
      sorts.emplace_back(it.var, it.ascending);
    }

    SortCondition sortCondition(
        plan, sorts, std::vector<std::vector<arangodb::basics::AttributeName>>(),
        ::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>>(),
        variableDefinitions);

    if (sortCondition.isEmpty() || !sortCondition.isOnlyAttributeAccess()) {
      // unusable sort condition
      return false;
    }

    // sort condition found, and sorting only by attributes!

    if (sortCondition.numAttributes() > primarySort.size()) {
      // the SORT condition in the query has more attributes than the view
      // is sorted by. we cannot optimize in this case
      return false;
    }

    // check if all sort conditions match
    for (size_t i = 0; i < sortElements.size(); ++i) {
      if (sortElements[i].ascending != primarySort.direction(i)) {
        // view is sorted in different order than requested in SORT condition
        return false;
      }
    }

    // all sort orders equal!
    // now finally check how many of the SORT conditions attributes we cover
    size_t numCovered = sortCondition.coveredAttributes(&viewNode.outVariable(), primarySort.fields());

    if (numCovered < sortNode->elements().size()) {
      // the sort is not covered by the view
      return false;
    }

    // we are almost done... but we need to do a final check and verify that our
    // sort node itself is not followed by another node that injects more data into
    // the result or that re-sorts it
    while (current->hasParent()) {
      current = current->getFirstParent();
      if (current->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW ||
          current->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
          current->getType() == ExecutionNode::TRAVERSAL ||
          current->getType() == ExecutionNode::SHORTEST_PATH ||
          current->getType() == ExecutionNode::K_SHORTEST_PATHS ||
          current->getType() == ExecutionNode::INDEX ||
          current->getType() == ExecutionNode::COLLECT ||
          current->getType() == ExecutionNode::SORT) {
        // any of these node types will lead to more/less results in the output,
        // and may as well change the sort order, so let's better abort here
        return false;
      }
    }

    assert(!primarySort.empty());
    viewNode.sort(&primarySort, sortElements.size());

    sortNode->_reinsertInCluster = false;
    if (!arangodb::ServerState::instance()->isCoordinator()) {
      // in cluster node will be unlinked later by 'distributeSortToClusterRule'
      plan->unlinkNode(sortNode);
    }

    return true;
  }
}

}  // namespace

namespace arangodb {

namespace iresearch {

void lateDocumentMaterializationArangoSearchRule(Optimizer* opt,
                     std::unique_ptr<ExecutionPlan> plan,
                     OptimizerRule const& rule) {
  bool modified = false;
  auto addPlan = arangodb::scopeGuard([opt, &plan, &rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });
      // arangosearch view node supports late materialization
  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW) ||
      // we need sort node  to be present  (without sort it will be just skip, nothing to optimize)
      !plan->contains(ExecutionNode::SORT) ||
      // limit node is needed as without limit all documents will be returned anyway, nothing to optimize
      !plan->contains(ExecutionNode::LIMIT)) {
    return;
  }

  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ExecutionNode::LIMIT, true);
  for (auto limitNode : nodes) {
    auto loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (ExecutionNode::ENUMERATE_IRESEARCH_VIEW == loop->getType()) {
      auto & viewNode = *ExecutionNode::castTo<IResearchViewNode*>(loop);
      if (viewNode.isLateMaterialized()) {
        continue; //loop is already optimized
      }
      ExecutionNode* current = limitNode->getFirstDependency();
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      bool stopSearch = false;
      while (current != loop) {
        switch (current->getType()) {
          case ExecutionNode::SORT:
            if (sortNode == nullptr) { // we need nearest to limit sort node, so keep selected if any
              sortNode = current;
            }
            break;
          case ExecutionNode::REMOTE:
            // REMOTE node is a blocker  - we do not want to make materialization calls across cluster!
            // Moreover we pass raw collection pointer - this must not cross process border!
            if (sortNode != nullptr) {
              // this limit node affects only closest sort, if this sort is invalid
              // we need to check other limit node
              stopSearch = true;
              sortNode = nullptr;
            }
            break;
          default: // make clang happy
            break;
        }
        if (sortNode != nullptr) {
          ::arangodb::containers::HashSet<Variable const*> currentUsedVars;
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(&viewNode.outVariable()) != currentUsedVars.end()) {
            // we have a doc body used before selected SortNode. Forget it, let`s look for better sort to use
            sortNode = nullptr;
            // this limit node affects only closest sort, if this sort is invalid
            // we need to check other limit node
            stopSearch = true;
          }
        }
        if (stopSearch) {
          break;
        }
        current = current->getFirstDependency();  // inspect next node
      }
      if (sortNode) {
        // we could apply late materialization
        // 1. We need to notify view - it should not materialize documents, but produce only localDocIds
        // 2. We need to add materializer after limit node to do materialization
        Ast* ast = plan->getAst();
        auto* localDocIdTmp = ast->variables()->createTemporaryVariable();
        auto* localColPtrTmp = ast->variables()->createTemporaryVariable();
        viewNode.setLateMaterialized(localColPtrTmp, localDocIdTmp);
        // insert a materialize node
        auto materializeNode =
            plan->registerNode(std::make_unique<materialize::MaterializeMultiNode>(
              plan.get(), plan->nextId(), *localColPtrTmp, *localDocIdTmp, viewNode.outVariable()));

        // on cluster we need to materialize node stay close to sort node on db server (to avoid network hop for materialization calls)
        // however on single server we move it to limit node to make materialization as lazy as possible
        auto materializeDependency = ServerState::instance()->isCoordinator() ? sortNode : limitNode;
        auto* dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent);
        dependencyParent->replaceDependency(materializeDependency, materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}

// ===============================================================================================
namespace {
  struct NodeWithAttrs {
    struct AttributeAndField {
      std::vector<arangodb::basics::AttributeName> attr;
      AstNode* astNode;
      size_t astNodeChildNum;
      size_t sortFieldNum;
      std::vector<arangodb::basics::AttributeName> const* sortField;
    };

    std::vector<AttributeAndField> attrs;
    CalculationNode* node;
  };

  bool attributesMatch(IResearchViewSort const& primarySort, NodeWithAttrs& node) {
    // check all node attributes to be in sort
    for (auto& nodeAttr : node.attrs) {
      size_t sortFieldNum = 0;
      for (auto const& field : primarySort.fields()) {
        if (arangodb::basics::AttributeName::isIdentical(nodeAttr.attr, field, false)) {
          nodeAttr.sortFieldNum = sortFieldNum;
          nodeAttr.sortField = &field;
          break;
        }
        ++sortFieldNum;
      }
      // not found
      if (nodeAttr.sortField == nullptr) {
        return false;
      }
    }
    return true;
  }

  // traverse the AST, using previsitor
  void traverseReadOnly(AstNode* node, AstNode* parentNode, size_t childNumber,
                        std::function<bool(AstNode const*, AstNode*, size_t)> const& preVisitor) {
    if (node == nullptr) {
      return;
    }

    if (!preVisitor(node, parentNode, childNumber)) {
      return;
    }

    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMemberUnchecked(i);

      if (member != nullptr) {
        traverseReadOnly(member, node, i, preVisitor);
      }
    }
  }

  // traversal state
  struct TraversalState {
    Variable const* variable;
    NodeWithAttrs& nodeAttrs;
    bool optimize;
    bool wasAccess;
  };

  // determines attributes referenced in an expression for the specified out variable
  bool getReferencedAttributes(AstNode* node,
                               Variable const* variable,
                               NodeWithAttrs& nodeAttrs) {
    TraversalState state{variable, nodeAttrs, true, false};

    auto preVisitor = [&state](AstNode const* node,
        AstNode* parentNode, size_t childNumber) {
      if (node == nullptr) {
        return false;
      }

      switch (node->type) {
        case NODE_TYPE_ATTRIBUTE_ACCESS:
          if (!state.wasAccess) {
            state.nodeAttrs.attrs.emplace_back(
              NodeWithAttrs::AttributeAndField{std::vector<arangodb::basics::AttributeName>{
                {std::string(node->getStringValue(), node->getStringLength()), false}}, parentNode, childNumber, 0, nullptr});
            state.wasAccess = true;
          } else {
            state.nodeAttrs.attrs.back().attr.emplace_back(std::string(node->getStringValue(), node->getStringLength()), false);
          }
          return true;
        case NODE_TYPE_REFERENCE: {
          // reference to a variable
          auto v = static_cast<Variable const*>(node->getData());
          if (v == state.variable) {
            if (!state.wasAccess) {
              // we haven't seen an attribute access directly before
              state.optimize = false;

              return false;
            }
            std::reverse(state.nodeAttrs.attrs.back().attr.begin(), state.nodeAttrs.attrs.back().attr.end());
          } else {
            if (state.wasAccess) {
              state.nodeAttrs.attrs.pop_back();
            }
          }
          // finish an attribute path
          state.wasAccess = false;
          return true;
        }
        default:
          break;
      }

      if (state.wasAccess) {
        // not appropriate node type
        state.wasAccess = false;
        state.optimize = false;

        return false;
      }

      return true;
    };

    traverseReadOnly(node, nullptr, 0, preVisitor);

    return state.optimize;
  }
}

void lateDocumentMaterializationArangoSearchRule2(Optimizer* opt,
                                                  std::unique_ptr<ExecutionPlan> plan,
                                                  OptimizerRule const& rule) {
  auto modified = false;
  auto addPlan = arangodb::scopeGuard([opt, &plan, &rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });
  // arangosearch view node supports late materialization
  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW) ||
      // we need sort node to be present (without sort it will be just skip, nothing to optimize)
      !plan->contains(ExecutionNode::SORT) ||
      // limit node is needed as without limit all documents will be returned anyway, nothing to optimize
      !plan->contains(ExecutionNode::LIMIT)) {
    return;
  }

  arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  arangodb::containers::SmallVector<ExecutionNode*> nodes{a};

  plan->findNodesOfType(nodes, ExecutionNode::LIMIT, true);
  for (auto limitNode : nodes) {
    auto loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (ExecutionNode::ENUMERATE_IRESEARCH_VIEW == loop->getType()) {
      auto viewNode = ExecutionNode::castTo<IResearchViewNode*>(loop);
      if (viewNode->isLateMaterialized()) {
        continue; // loop is already optimized
      }

      auto const& primarySort = ::primarySort(*viewNode->view());
      if (primarySort.empty()) {
        // no primary sort
        continue;
      }

      auto current = limitNode->getFirstDependency();
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      bool stopSearch = false;
      std::vector<NodeWithAttrs> nodesToChange;
      while (current != loop) {
        switch (current->getType()) {
          case ExecutionNode::SORT:
            if (sortNode == nullptr) { // we need nearest to limit sort node, so keep selected if any
              sortNode = current;
            }
            break;
          case ExecutionNode::CALCULATION: {
            auto calculationNode = ExecutionNode::castTo<CalculationNode*>(current);
            auto astNode = calculationNode->expression()->nodeForModification();
            NodeWithAttrs node;
            node.node = calculationNode;
            // find attributes referenced to view node out variable
            if (!getReferencedAttributes(astNode, &viewNode->outVariable(), node)) {
              // is not safe for optimization
              stopSearch = true;
            } else if (!node.attrs.empty()) {
              if (!attributesMatch(primarySort, node)) {
                // the node uses attributes which is not in view sort
                stopSearch = true;
              } else {
                nodesToChange.emplace_back(node);
              }
            }
            break;
          }
          case ExecutionNode::REMOTE:
            // REMOTE node is a blocker - we do not want to make materialization calls across cluster!
            if (sortNode != nullptr) {
              stopSearch = true;
            }
            break;
          default: // make clang happy
            break;
        }
        if (sortNode != nullptr && current->getType() != ExecutionNode::CALCULATION) {
          arangodb::containers::HashSet<Variable const*> currentUsedVars;
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(&viewNode->outVariable()) != currentUsedVars.end()) {
            // this limit node affects only closest sort, if this sort is invalid
            // we need to check other limit node
            stopSearch = true;
          }
        }
        if (stopSearch) {
          // we have a doc body used before selected SortNode. Forget it, let`s look for better sort to use
          sortNode = nullptr;
          nodesToChange.clear();
          break;
        }
        current = current->getFirstDependency();  // inspect next node
      }
      if (sortNode && !nodesToChange.empty()) {
        auto ast = plan->getAst();
        IResearchViewNode::ViewVarsInfo uniqueVariables;
        for (auto& node : nodesToChange) {
          std::transform(node.attrs.cbegin(), node.attrs.cend(), std::inserter(uniqueVariables, uniqueVariables.end()),
                         [ast](auto const& attrAndField) {
                           return std::make_pair(attrAndField.sortField, IResearchViewNode::ViewVariable{attrAndField.sortFieldNum,
                             ast->variables()->createTemporaryVariable()});
                         });
        }
        auto localColPtrTmp = ast->variables()->createTemporaryVariable();
        auto localDocIdTmp = ast->variables()->createTemporaryVariable();
        for (auto& node : nodesToChange) {
          for (auto& attr : node.attrs) {
            auto it = uniqueVariables.find(attr.sortField);
            TRI_ASSERT(it != uniqueVariables.cend());
            auto newNode = ast->createNodeReference(it->second.var);
            if (attr.astNode != nullptr) {
              TEMPORARILY_UNLOCK_NODE(attr.astNode);
              attr.astNode->changeMember(attr.astNodeChildNum, newNode);
            } else {
              TRI_ASSERT(node.attrs.size() == 1);
              node.node->expression()->replaceNode(newNode);
            }
          }
        }

        // we could apply late materialization
        // 1. We need to notify index node - it should not materialize documents, but produce only localDocIds
        viewNode->setLateMaterialized(localColPtrTmp, localDocIdTmp, uniqueVariables);
        // 2. We need to add materializer after limit node to do materialization
        // insert a materialize node
        auto materializeNode =
          plan->registerNode(std::make_unique<materialize::MaterializeMultiNode>(
            plan.get(), plan->nextId(), *localColPtrTmp, *localDocIdTmp, viewNode->outVariable()));

        // on cluster we need to materialize node stay close to sort node on db server (to avoid network hop for materialization calls)
        // however on single server we move it to limit node to make materialization as lazy as possible
        auto materializeDependency = ServerState::instance()->isCoordinator() ? sortNode : limitNode;
        auto dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent != nullptr);
        dependencyParent->replaceDependency(materializeDependency, materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}

/// @brief move filters and sort conditions into views
void handleViewsRule(Optimizer* opt,
                     std::unique_ptr<ExecutionPlan> plan,
                     OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst() && plan->getAst()->query());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  auto addPlan = irs::make_finally([opt, &plan, &rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });

  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW)) {
    // no view present in the query, so no need to do any expensive
    // transformations
    return;
  }

  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};

  // replace scorers in all calculation nodes with references
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  ScorerReplacer scorerReplacer;

  for (auto* node : nodes) {
    TRI_ASSERT(node && ExecutionNode::CALCULATION == node->getType());

    scorerReplacer.replace(*ExecutionNode::castTo<CalculationNode*>(node));
  }

  // register replaced scorers to be evaluated by corresponding view nodes
  nodes.clear();
  plan->findNodesOfType(nodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  auto& query = *plan->getAst()->query();

  std::vector<Scorer> scorers;

  for (auto* node : nodes) {
    TRI_ASSERT(node && ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);

    if (!viewNode.isInInnerLoop()) {
      // check if we can optimize away a sort that follows the EnumerateView node
      // this is only possible if the view node itself is not contained in another loop
      modified = optimizeSort(viewNode, plan.get());
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

void scatterViewInClusterRule(Optimizer* opt,
                              std::unique_ptr<ExecutionPlan> plan,
                              OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};

  // find subqueries
  std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;
  plan->findNodesOfType(nodes, ExecutionNode::SUBQUERY, true);

  for (auto& it : nodes) {
    subqueries.try_emplace(ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery(), it);
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
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);
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
        plan->registerNode(std::make_unique<ScatterNode>(plan.get(), plan->nextId(), ScatterNode::ScatterType::SHARD));
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
      auto* subQueryNode = ExecutionNode::castTo<SubqueryNode*>((*it).second);
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
