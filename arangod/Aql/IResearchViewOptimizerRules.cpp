////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/CalculationNodeVarFinder.h"
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
#include "Basics/DownCast.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/Search.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <utils/misc.hpp>

using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb::iresearch {

aql::Variable const* replaceOffsetInfo(aql::CalculationNode& calcNode,
                                       aql::Variable const& ref,
                                       aql::AstNode const& node);

#ifndef USE_ENTERPRISE
aql::Variable const* replaceOffsetInfo(aql::CalculationNode& calcNode,
                                       aql::Variable const& ref,
                                       aql::AstNode const& node) {
  aql::functions::NotImplementedEE(nullptr, node, {});  // will throw
  return nullptr;
}
#endif

namespace {

// TODO deduplicate these functions with IResearchViewNode

IResearchSortBase const& getPrimarySort(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->primarySort;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.primarySort();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.primarySort();
}

IResearchViewStoredValues const& getStoredValues(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->storedValues;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.storedValues();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.storedValues();
}

/// @brief Moves all FCALLs in every AND node to the bottom of the AND node
/// @param condition SEARCH condition node
/// @param starts_with Function to push
void pushFuncToBack(arangodb::aql::AstNode& condition,
                    arangodb::aql::Function const* starts_with) {
  auto numMembers = condition.numMembers();
  for (size_t memberIdx = 0; memberIdx < numMembers; ++memberIdx) {
    auto current = condition.getMemberUnchecked(memberIdx);
    TRI_ASSERT(current);
    auto const numAndMembers = current->numMembers();
    if (current->type ==
            arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND &&
        numAndMembers > 1) {
      size_t movePoint = numAndMembers - 1;
      do {
        auto candidate = current->getMemberUnchecked(movePoint);
        if (candidate->type != arangodb::aql::AstNodeType::NODE_TYPE_FCALL ||
            static_cast<arangodb::aql::Function const*>(candidate->getData()) !=
                starts_with) {
          break;
        }
      } while ((--movePoint) != 0);
      for (size_t andMemberIdx = 0; andMemberIdx < movePoint; ++andMemberIdx) {
        auto andMember = current->getMemberUnchecked(andMemberIdx);
        if (andMember->type == arangodb::aql::AstNodeType::NODE_TYPE_FCALL &&
            static_cast<arangodb::aql::Function const*>(andMember->getData()) ==
                starts_with) {
          TEMPORARILY_UNLOCK_NODE(current);
          auto tmp = current->getMemberUnchecked(movePoint);
          current->changeMember(movePoint--, andMember);
          current->changeMember(andMemberIdx, tmp);
          while (movePoint > andMemberIdx &&
                 current->getMemberUnchecked(movePoint)->type ==
                     arangodb::aql::AstNodeType::NODE_TYPE_FCALL &&
                 static_cast<arangodb::aql::Function const*>(
                     current->getMemberUnchecked(movePoint)->getData()) ==
                     starts_with) {
            --movePoint;
          }
        } else {
          pushFuncToBack(*andMember, starts_with);
        }
      }
    } else {
      pushFuncToBack(*current, starts_with);
    }
  }
}

bool addView(arangodb::LogicalView const& view,
             arangodb::aql::QueryContext& query) {
  auto& collections = query.collections();

  // linked collections
  auto visitor = [&collections](arangodb::DataSourceId cid,
                                LogicalView::Indexes*) {
    collections.add(arangodb::basics::StringUtils::itoa(cid.id()),
                    arangodb::AccessMode::Type::READ,
                    arangodb::aql::Collection::Hint::Collection);
    return true;
  };

  return view.visitCollections(visitor);
}

bool optimizeSearchCondition(IResearchViewNode& viewNode,
                             arangodb::aql::QueryContext& query,
                             ExecutionPlan& plan) {
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

  auto nodeFilter = viewNode.filterCondition();
  if (!isFilterConditionEmpty(&nodeFilter)) {
    searchCondition.andCombine(&nodeFilter);
    searchCondition.normalize(&plan, true,
                              viewNode.options().conditionOptimization);

    if (searchCondition.isEmpty()) {
      // condition is always false
      for (auto const& x : viewNode.getParents()) {
        plan.insertDependency(
            x, plan.registerNode(
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
    if (viewNode.filterOptimization() !=
        arangodb::iresearch::FilterOptimization::NONE) {
      // we could benefit from merging STARTS_WITH and LEVENSHTEIN_MATCH
      auto& server = plan.getAst()->query().vocbase().server();
      auto starts_with =
          server.getFeature<AqlFunctionFeature>().byName("STARTS_WITH");
      TRI_ASSERT(starts_with);
      pushFuncToBack(*searchCondition.root(), starts_with);
    }

    arangodb::iresearch::QueryContext ctx{
        .trx = &query.trxForOptimization(),
        .ref = &viewNode.outVariable(),
        .isSearchQuery = true,
        .isOldMangling = (viewNode.meta() == nullptr),
        // we don't care here- we are checking condition in general
        .hasNestedFields = false};

    // The analyzer is referenced in the FilterContext and used during the
    // following ::makeFilter() call, so may not be a temporary.
    FieldMeta::Analyzer analyzer{IResearchAnalyzerFeature::identity()};
    FilterContext const filterCtx{.contextAnalyzer = analyzer};

    auto filterCreated =
        FilterFactory::filter(nullptr, ctx, filterCtx, *searchCondition.root());

    if (filterCreated.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          filterCreated.errorNumber(),
          StringUtils::concatT("unsupported SEARCH condition: ",
                               filterCreated.errorMessage()));
    }
  }

  if (!searchCondition.isEmpty()) {
    viewNode.setFilterCondition(searchCondition.root());
  }

  return true;
}

bool optimizeScoreSort(IResearchViewNode& viewNode, ExecutionPlan* plan) {
  if (!plan->contains(ExecutionNode::LIMIT) ||
      !plan->contains(ExecutionNode::SORT)) {
    return false;
  }

  auto current = static_cast<ExecutionNode*>(&viewNode);
  auto viewVariable = viewNode.outVariable();
  auto const& scorers = viewNode.scorers();
  SortNode* sortNode = nullptr;
  LimitNode const* limitNode = nullptr;
  while ((current = current->getFirstParent())) {
    switch (current->getType()) {
      case ExecutionNode::SORT:
        sortNode = ExecutionNode::castTo<SortNode*>(current);
        break;
      case ExecutionNode::LIMIT:
        if (sortNode == nullptr) {
          return false;
        }
        limitNode = ExecutionNode::castTo<LimitNode*>(current);
        break;
      case ExecutionNode::OFFSET_INFO_MATERIALIZE:
      case ExecutionNode::CALCULATION:
        // Only deterministic calcs allowed
        // Otherwise optimization should be forbidden
        // as number of calls will be changed!
        if (!current->isDeterministic()) {
          return false;
        }
        break;
      default:
        return false;
        break;
    }
    if (limitNode && sortNode) {
      // only first SORT + LIMIT makes sense
      break;
    }
  }
  if (!sortNode || !limitNode) {
    return false;
  }

  // we've found all we need
  auto const& sortElements = sortNode->elements();
  std::vector<std::pair<size_t, bool>> scoresSort;
  for (auto const& sort : sortElements) {
    TRI_ASSERT(sort.var);

    auto const* varSetBy = plan->getVarSetBy(sort.var->id);
    TRI_ASSERT(varSetBy);

    aql::Variable const* sortVariable{};
    switch (varSetBy->getType()) {
      case ExecutionNode::CALCULATION: {
        auto* calc = ExecutionNode::castTo<CalculationNode const*>(varSetBy);
        TRI_ASSERT(calc->expression());

        auto const* astCalcNode = calc->expression()->node();
        if (!astCalcNode ||
            astCalcNode->type != AstNodeType::NODE_TYPE_REFERENCE) {
          // Not a reference?  Seems that it is not
          // something produced by during search function replacement.
          // e.g. it is expected to be LET sortVar = scorerVar;
          // Definately not something we could handle.
          return false;
        }

        sortVariable =
            reinterpret_cast<aql::Variable const*>(astCalcNode->getData());
        TRI_ASSERT(sortVariable);
        break;
      }
      case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // FIXME (Dronplane): here we should deal with stored
        //                    values when we will support such optimization
        [[fallthrough]];
      default:
        TRI_ASSERT(false);
        return false;
    }

    TRI_ASSERT(sortVariable);
    auto const s = std::find_if(
        std::begin(scorers), std::end(scorers),
        [sortVariableId = sortVariable->id](auto const& t) noexcept {
          return t.var->id == sortVariableId;
        });
    if (s == std::end(scorers)) {
      return false;
    }
    scoresSort.emplace_back(std::distance(scorers.begin(), s), sort.ascending);
  }
  // all sort elements are covered by view's scorers
  viewNode.setScorersSort(std::move(scoresSort),
                          limitNode->offset() + limitNode->limit());
  sortNode->_reinsertInCluster = false;
  if (!arangodb::ServerState::instance()->isCoordinator()) {
    // in cluster node will be unlinked later by 'distributeSortToClusterRule'
    plan->unlinkNode(sortNode);
  }
  return true;
}

bool optimizeSort(IResearchViewNode& viewNode, ExecutionPlan* plan) {
  auto const& primarySort = getPrimarySort(viewNode.meta(), viewNode.view());

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
        current->getType() == ExecutionNode::ENUMERATE_PATHS ||
        current->getType() == ExecutionNode::INDEX ||
        current->getType() == ExecutionNode::COLLECT) {
      // any of these node types will lead to more/less results in the output,
      // and may as well change the sort order, so let's better abort here
      return false;
    }

    if (current->getType() == ExecutionNode::CALCULATION) {
      // pick up the meanings of variables as we walk the plan
      variableDefinitions.try_emplace(
          ExecutionNode::castTo<CalculationNode const*>(current)
              ->outVariable()
              ->id,
          ExecutionNode::castTo<CalculationNode const*>(current)
              ->expression()
              ->node());
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
      // note: in contrast to regular indexes, views support sorting in
      // different directions for multiple fields (e.g. SORT doc.a ASC, doc.b
      // DESC). this is not supported by indexes
      sorts.emplace_back(it.var, it.ascending);
    }

    SortCondition sortCondition(
        plan, sorts,
        std::vector<std::vector<arangodb::basics::AttributeName>>(),
        ::arangodb::containers::HashSet<
            std::vector<arangodb::basics::AttributeName>>(),
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
    size_t numCovered = sortCondition.coveredAttributes(&viewNode.outVariable(),
                                                        primarySort.fields());

    if (numCovered < sortNode->elements().size()) {
      // the sort is not covered by the view
      return false;
    }

    // we are almost done... but we need to do a final check and verify that
    // our sort node itself is not followed by another node that injects more
    // data into the result or that re-sorts it
    while (current->hasParent()) {
      current = current->getFirstParent();
      if (current->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW ||
          current->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
          current->getType() == ExecutionNode::TRAVERSAL ||
          current->getType() == ExecutionNode::SHORTEST_PATH ||
          current->getType() == ExecutionNode::ENUMERATE_PATHS ||
          current->getType() == ExecutionNode::INDEX ||
          current->getType() == ExecutionNode::COLLECT ||
          current->getType() == ExecutionNode::SORT) {
        // any of these node types will lead to more/less results in the
        // output, and may as well change the sort order, so let's better
        // abort here
        return false;
      }
    }

    assert(!primarySort.empty());
    viewNode.setSort(primarySort, sortElements.size());

    sortNode->_reinsertInCluster = false;
    if (!arangodb::ServerState::instance()->isCoordinator()) {
      // in cluster node will be unlinked later by
      // 'distributeSortToClusterRule'
      plan->unlinkNode(sortNode);
    }

    return true;
  }
}

void keepReplacementViewVariables(std::span<ExecutionNode* const> calcNodes,
                                  std::span<ExecutionNode* const> viewNodes) {
  std::vector<latematerialized::NodeWithAttrsColumn> nodesToChange;
  std::vector<std::vector<latematerialized::ColumnVariant<false>>>
      usedColumnsCounter;
  for (auto* vNode : viewNodes) {
    TRI_ASSERT(vNode &&
               ExecutionNode::ENUMERATE_IRESEARCH_VIEW == vNode->getType());
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(vNode);
    auto const& primarySort = getPrimarySort(viewNode.meta(), viewNode.view());
    auto const& storedValues =
        getStoredValues(viewNode.meta(), viewNode.view());
    if (primarySort.empty() && storedValues.empty()) {
      // neither primary sort nor stored values
      continue;
    }
    auto const& var = viewNode.outVariable();
    auto& viewNodeState = viewNode.state();
    auto const columnsCount = storedValues.columns().size() + 1;
    if (columnsCount > usedColumnsCounter.size()) {
      usedColumnsCounter.resize(columnsCount);
    }
    // restoring initial state for column accumulator (only potentially usable
    // part)
    auto const beginColumns = usedColumnsCounter.begin();
    auto const endColumns = usedColumnsCounter.begin() + columnsCount;
    for (auto it = beginColumns; it != endColumns; ++it) {
      it->clear();
    }
    for (auto* cNode : calcNodes) {
      TRI_ASSERT(cNode && ExecutionNode::CALCULATION == cNode->getType());
      auto& calcNode = *ExecutionNode::castTo<CalculationNode*>(cNode);
      auto* astNode = calcNode.expression()->nodeForModification();
      TRI_ASSERT(astNode);
      latematerialized::NodeWithAttrsColumn node;
      node.node = &calcNode;
      // find attributes referenced to view node out variable
      if (latematerialized::getReferencedAttributes(astNode, &var, node)) {
        if (!node.attrs.empty()) {
          if (attributesMatch(primarySort, storedValues, node.attrs,
                              usedColumnsCounter, columnsCount)) {
            nodesToChange.emplace_back(std::move(node));
          } else {
            viewNodeState.disableNoDocumentMaterialization();
          }
        }
      } else {
        viewNodeState.disableNoDocumentMaterialization();
      }
    }
    if (!nodesToChange.empty()) {
      setAttributesMaxMatchedColumns(usedColumnsCounter, columnsCount);
      viewNodeState.saveCalcNodesForViewVariables(nodesToChange);
      nodesToChange.clear();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE  // force nullptr`s to trigger assertion
                                        // on non-used-nodes access
      for (auto& a : usedColumnsCounter) {
        for (auto& b : a) {
          b.afData = nullptr;
        }
      }
#endif
    }
  }
}

bool noDocumentMaterialization(
    std::span<ExecutionNode* const> viewNodes,
    arangodb::containers::HashSet<ExecutionNode*>& toUnlink) {
  auto modified = false;
  VarSet currentUsedVars;
  for (auto* node : viewNodes) {
    TRI_ASSERT(node &&
               ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);
    auto& viewNodeState = viewNode.state();
    if (!(viewNode.options().noMaterialization &&
          viewNodeState.isNoDocumentMaterializationPossible())) {
      continue;  // can not optimize
    }
    auto* current = node;
    current = current->getFirstParent();
    TRI_ASSERT(current);
    auto const& var = viewNode.outVariable();
    auto isCalcNodesFound = false;
    auto valid = true;
    // check if there are any not calculation nodes in the plan referencing to
    // the view variable
    do {
      currentUsedVars.clear();
      current->getVariablesUsedHere(currentUsedVars);
      if (currentUsedVars.find(&var) != currentUsedVars.end()) {
        switch (current->getType()) {
          case ExecutionNode::CALCULATION:
            isCalcNodesFound = true;
            break;
          case ExecutionNode::SUBQUERY: {
            auto& subqueryNode = *ExecutionNode::castTo<SubqueryNode*>(current);
            auto* subquery = subqueryNode.getSubquery();
            TRI_ASSERT(subquery);
            // check calculation nodes in the plan of a subquery
            CalculationNodeVarExistenceFinder finder(&var);
            valid = !subquery->walk(finder);
            isCalcNodesFound |= finder.isCalculationNodesFound();
            break;
          }
          default:
            valid = false;
            break;
        }
        if (!valid) {
          break;
        }
      }
      current = current->getFirstParent();
    } while (current);
    if (!valid) {
      continue;  // can not optimize
    }
    // replace view variables in calculation nodes if need
    if (isCalcNodesFound) {
      auto viewVariables = viewNodeState.replaceAllViewVariables(toUnlink);
      // if no replacements were found
      if (viewVariables.empty()) {
        continue;  // can not optimize
      }
      viewNode.setViewVariables(viewVariables);
    }
    viewNode.setNoMaterialization();
    modified = true;
  }
  return modified;
}

enum class SearchFuncType { kInvalid, kScorer, kOffsetInfo };

std::pair<aql::Variable const*, SearchFuncType> resolveSearchFunc(
    aql::AstNode const& node) {
  if (aql::NODE_TYPE_FCALL == node.type ||
      aql::NODE_TYPE_FCALL_USER == node.type) {
    auto* impl = static_cast<aql::Function*>(node.getData());

    if (isScorer(*impl)) {
      return {getSearchFuncRef(node.getMember(0)), SearchFuncType::kScorer};
    } else if (isOffsetInfo(*impl)) {
      return {getSearchFuncRef(node.getMember(0)), SearchFuncType::kOffsetInfo};
    }
  }

  return {nullptr, SearchFuncType::kInvalid};
}

aql::Variable const* replaceScorer(aql::CalculationNode& calcNode,
                                   aql::Variable const& ref,
                                   aql::AstNode const& node) {
  arangodb::iresearch::QueryContext const ctx{.ref = &ref};

  if (!order_factory::scorer(nullptr, node, ctx)) {
    // invalid scorer function
    return nullptr;
  }

  auto* ast = calcNode.plan()->getAst();
  TRI_ASSERT(ast);
  auto* vars = ast->variables();
  TRI_ASSERT(vars);
  return vars->createTemporaryVariable();
}

aql::Variable const* replaceSearchFunc(aql::CalculationNode& calcNode,
                                       aql::Variable const& ref,
                                       aql::AstNode const& node,
                                       SearchFuncType type) {
  switch (type) {
    case SearchFuncType::kScorer:
      return replaceScorer(calcNode, ref, node);
    case SearchFuncType::kOffsetInfo:
      return replaceOffsetInfo(calcNode, ref, node);
    default:
      TRI_ASSERT(false);
      return nullptr;
  }
}

aql::AstNode* replaceSearchFunc(aql::CalculationNode& calcNode,
                                aql::AstNode& exprNode,
                                DedupSearchFuncs& dedup) {
  auto const [var, type] = resolveSearchFunc(exprNode);

  if (!var || type == SearchFuncType::kInvalid) {
    // not a valid search function
    return &exprNode;
  }

  HashedSearchFunc const key{var, &exprNode};

  auto const it =
      dedup.lazy_emplace(key, [&, var = var, type = type](auto const& ctor) {
        ctor(key, replaceSearchFunc(calcNode, *var, exprNode, type));
      });

  if (ADB_UNLIKELY(!it->second)) {
    dedup.erase(it);
    return &exprNode;
  }

  auto* ast = calcNode.plan()->getAst();
  TRI_ASSERT(ast);

  return ast->createNodeReference(it->second);
}

void replaceSearchFunc(aql::CalculationNode& calcNode,
                       DedupSearchFuncs& dedup) {
  auto* expr = calcNode.expression();

  if (ADB_UNLIKELY(!expr)) {
    return;
  }

  auto* ast = expr->ast();

  if (ADB_UNLIKELY(!ast)) {
    // ast is not set
    return;
  }

  auto* exprNode = expr->nodeForModification();

  if (ADB_UNLIKELY(!exprNode)) {
    // node is not set
    return;
  }

  if (auto const [var, type] = resolveSearchFunc(*exprNode); var) {
    auto* newNode = replaceSearchFunc(calcNode, *exprNode, dedup);
    TRI_ASSERT(newNode != exprNode);

    calcNode.expression()->replaceNode(newNode);
  } else if (bool const hasFunc = !arangodb::iresearch::visit<true>(
                 *exprNode,
                 [](aql::AstNode const& node) {
                   auto const [var, _] = resolveSearchFunc(node);
                   return !var;
                 });
             hasFunc) {
    auto replaceFunc = [&](aql::AstNode* node) -> aql::AstNode* {
      TRI_ASSERT(node);  // ensured by 'Ast::traverseAndModify(...)'
      return replaceSearchFunc(calcNode, *node, dedup);
    };

    auto* exprClone = exprNode->clone(ast);
    aql::Ast::traverseAndModify(exprClone, replaceFunc);
    expr->replaceNode(exprClone);
  }
}

void extractScorers(IResearchViewNode const& viewNode, DedupSearchFuncs& dedup,
                    std::vector<SearchFunc>& funcs) {
  auto* viewVar = &viewNode.outVariable();
  arangodb::aql::VarSet usedVars;

  for (auto it = std::begin(dedup); it != std::end(dedup);) {
    auto& func = it->first;
    if (isScorer(*func.node) && func.var == viewVar) {
      // extract all variables used in scorer
      usedVars.clear();
      aql::Ast::getReferencedVariables(func.node, usedVars);

      // get all variables valid in view node
      auto const& validVars = viewNode.getVarsValid();
      for (auto* v : usedVars) {
        if (!validVars.contains(v)) {
          TRI_ASSERT(func.node);
          auto const funcName = iresearch::getFuncName(*func.node);

          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_BAD_PARAMETER,
              "Inaccesible non-ArangoSearch view variable '%s' is used in "
              "search function '%s'",
              v->name.c_str(), funcName.c_str());
        }
      }

      funcs.emplace_back(it->second, func.node);
      const auto copy_it = it++;
      dedup.erase(copy_it);
    } else {
      ++it;
    }
  }
}

}  // namespace

void lateDocumentMaterializationArangoSearchRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  auto modified = false;
  auto const addPlan =
      arangodb::scopeGuard([opt, &plan, &rule, &modified]() noexcept {
        opt->addPlan(std::move(plan), rule, modified);
      });
  // arangosearch view node supports late materialization
  // cppcheck-suppress accessMoved
  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW) ||
      // we need sort node  to be present  (without sort it will be just skip,
      // nothing to optimize)
      !plan->contains(ExecutionNode::SORT) ||
      // limit node is needed as without limit all documents will be returned
      // anyway, nothing to optimize
      !plan->contains(ExecutionNode::LIMIT)) {
    return;
  }

  VarSet currentUsedVars;
  // nodes variables can be replaced
  containers::SmallVector<aql::CalculationNode*, 16> calcNodes;
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::LIMIT, true);
  for (auto* limitNode : nodes) {
    auto* loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (loop != nullptr &&
        ExecutionNode::ENUMERATE_IRESEARCH_VIEW == loop->getType()) {
      auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(loop);
      if (viewNode.isNoMaterialization() || viewNode.isLateMaterialized()) {
        continue;  // loop is already optimized
      }
      auto* current = limitNode->getFirstDependency();
      TRI_ASSERT(current);
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      auto stopSearch = false;
      auto stickToSortNode = false;
      auto const& var = viewNode.outVariable();
      calcNodes.clear();
      auto& viewNodeState = viewNode.state();
      while (current != loop) {
        auto const type = current->getType();
        switch (type) {
          case ExecutionNode::SORT:
            if (sortNode == nullptr) {  // we need nearest to limit sort node,
                                        // so keep selected if any
              sortNode = current;
            }
            break;
          case ExecutionNode::REMOTE:
            // REMOTE node is a blocker - we do not want to make materialization
            // calls across cluster! Moreover we pass raw collection pointer -
            // this must not cross process border!
            if (sortNode != nullptr) {
              stopSearch = true;
            } else {
              stickToSortNode = true;
            }
            break;
          case ExecutionNode::LIMIT:
            // After sort-limit rule was modified we could encounter additional
            // limit nodes before Sort. Break search on them if still no sort
            // found. As we need the closest LIMIT to the Sort. If we encounter
            // additional LIMITs after we found a Sort node that is ok as it
            // makes no harm for the late materialization.
            if (sortNode == nullptr) {
              stopSearch = true;
            }
            break;
          default:  // make clang happy
            break;
        }
        if (!stopSearch) {
          currentUsedVars.clear();
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(&var) != currentUsedVars.end()) {
            // currently only calculation nodes expected to use a loop variable
            // with attributes we successfully replace all references to the
            // loop variable
            auto valid = false;
            switch (type) {
              case ExecutionNode::CALCULATION: {
                auto* calcNode =
                    ExecutionNode::castTo<CalculationNode*>(current);
                TRI_ASSERT(calcNode);
                if (viewNodeState.canVariablesBeReplaced(calcNode)) {
                  calcNodes.emplace_back(calcNode);
                  valid = true;
                }
                break;
              }
              case ExecutionNode::SUBQUERY: {
                auto& subqueryNode =
                    *ExecutionNode::castTo<SubqueryNode*>(current);
                auto* subquery = subqueryNode.getSubquery();
                TRI_ASSERT(subquery);
                containers::SmallVector<ExecutionNode*, 8> subqueryCalcNodes;
                // find calculation nodes in the plan of a subquery
                CalculationNodeVarFinder finder(&var, subqueryCalcNodes);
                valid = !subquery->walk(finder);
                if (valid) {  // if the finder did not stop
                  for (auto* scn : subqueryCalcNodes) {
                    TRI_ASSERT(scn &&
                               scn->getType() == ExecutionNode::CALCULATION);
                    currentUsedVars.clear();
                    scn->getVariablesUsedHere(currentUsedVars);
                    if (currentUsedVars.find(&var) != currentUsedVars.end()) {
                      auto* calcNode =
                          ExecutionNode::castTo<CalculationNode*>(scn);
                      TRI_ASSERT(calcNode);
                      if (viewNodeState.canVariablesBeReplaced(calcNode)) {
                        calcNodes.emplace_back(calcNode);
                      } else {
                        valid = false;
                        break;
                      }
                    }
                  }
                }
                break;
              }
              default:
                break;
            }
            if (!valid) {
              if (sortNode != nullptr) {
                // we have a doc body used before selected SortNode
                // forget it, let`s look for better sort to use
                stopSearch = true;
              } else {
                // we are between limit and sort nodes
                // late materialization could still be applied but we must
                // insert MATERIALIZE node after sort not after limit
                stickToSortNode = true;
              }
            }
          }
        }
        if (stopSearch) {
          // this limit node affects only closest sort if this sort is invalid
          // we need to check other limit node
          sortNode = nullptr;
          break;
        }
        current = current->getFirstDependency();  // inspect next node
      }
      if (sortNode) {
        // we could apply late materialization
        // 1. Replace view variables in calculation node if need
        if (!calcNodes.empty()) {
          ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
          auto viewVariables =
              viewNodeState.replaceViewVariables(calcNodes, toUnlink);
          viewNode.setViewVariables(viewVariables);
          if (!toUnlink.empty()) {
            plan->unlinkNodes(toUnlink);
          }
        }
        // 2. We need to notify view - it should not materialize documents, but
        // produce only localDocIds
        // 3. We need to add materializer after limit node to do materialization
        auto* ast = plan->getAst();
        TRI_ASSERT(ast);
        auto* localDocIdTmp = ast->variables()->createTemporaryVariable();
        TRI_ASSERT(localDocIdTmp);
        auto* localColPtrTmp = ast->variables()->createTemporaryVariable();
        TRI_ASSERT(localColPtrTmp);
        viewNode.setLateMaterialized(*localColPtrTmp, *localDocIdTmp);
        // insert a materialize node
        auto* materializeNode = plan->registerNode(
            std::make_unique<materialize::MaterializeMultiNode>(
                plan.get(), plan->nextId(), *localColPtrTmp, *localDocIdTmp,
                var));
        TRI_ASSERT(materializeNode);

        auto* materializeDependency = stickToSortNode ? sortNode : limitNode;
        TRI_ASSERT(materializeDependency);
        auto* dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent);
        dependencyParent->replaceDependency(materializeDependency,
                                            materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}

void handleConstrainedSortInView(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  auto addPlan = irs::make_finally([opt, &plan, &rule, &modified]() noexcept {
    opt->addPlan(std::move(plan), rule, modified);
  });

  // cppcheck-suppress accessMoved
  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW) ||
      !plan->contains(ExecutionNode::SORT) ||
      !plan->contains(ExecutionNode::LIMIT)) {
    // no view && sort && limit present in the query,
    // so no need to do any expensive transformations
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> viewNodes;
  plan->findNodesOfType(viewNodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
                        true);
  for (auto* node : viewNodes) {
    TRI_ASSERT(node &&
               ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);
    if (viewNode.sort().first) {
      // this view already has PrimarySort - no sort for us.
      continue;
    }

    if (!viewNode.scorers().empty() && !viewNode.isInInnerLoop()) {
      // check if we can optimize away a sort that follows the EnumerateView
      // node this is only possible if the view node itself is not contained in
      // another loop
      modified |= optimizeScoreSort(viewNode, plan.get());
    }
  }
}

void handleViewsRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                     OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  auto addPlan = irs::make_finally([opt, &plan, &rule, &modified]() noexcept {
    opt->addPlan(std::move(plan), rule, modified);
  });

  // cppcheck-suppress accessMoved
  if (!plan->contains(ExecutionNode::ENUMERATE_IRESEARCH_VIEW)) {
    // no view present in the query, so no need to do any expensive
    // transformations
    return;
  }

  // register replaced scorers to be evaluated by corresponding view nodes
  containers::SmallVector<ExecutionNode*, 8> viewNodes;
  plan->findNodesOfType(viewNodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
                        true);

  // replace scorers in all calculation nodes with references
  containers::SmallVector<ExecutionNode*, 8> calcNodes;
  plan->findNodesOfType(calcNodes, ExecutionNode::CALCULATION, true);

  DedupSearchFuncs searchFuncs;

  for (auto* node : calcNodes) {
    TRI_ASSERT(node && ExecutionNode::CALCULATION == node->getType());
    replaceSearchFunc(*ExecutionNode::castTo<CalculationNode*>(node),
                      searchFuncs);
  }
  modified = !searchFuncs.empty();

  aql::QueryContext& query = plan->getAst()->query();
  std::vector<SearchFunc> scorers;

  for (auto* node : viewNodes) {
    TRI_ASSERT(node &&
               ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);

    if (!viewNode.isInInnerLoop()) {
      // check if we can optimize away sort that follows the EnumerateView
      // node this is only possible if the view node itself is not contained in
      // another loop
      modified |= optimizeSort(viewNode, plan.get());
    }

    // find scorers that have to be evaluated by a view
    extractScorers(viewNode, searchFuncs, scorers);
    viewNode.setScorers(std::move(scorers));

    if (!optimizeSearchCondition(viewNode, query, *plan)) {
      continue;
    }

    modified = true;
  }
  keepReplacementViewVariables(calcNodes, viewNodes);
  containers::HashSet<ExecutionNode*> toUnlink;
  modified |= noDocumentMaterialization(viewNodes, toUnlink);
  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  // ensure all replaced scorers are covered by corresponding view nodes
  for (auto& [func, _] : searchFuncs) {
    TRI_ASSERT(func.node);
    auto const& node = *func.node;

    if (isScorer(node)) {
      auto const funcName = iresearch::getFuncName(node);

      THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "Non ArangoSearch view variable '%s' is used in scorer function '%s'",
          func.var->name.c_str(), funcName.c_str());
    }
  }
}

void scatterViewInClusterRule(Optimizer* opt,
                              std::unique_ptr<ExecutionPlan> plan,
                              OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  containers::SmallVector<ExecutionNode*, 8> nodes;

  // find subqueries
  std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;
  plan->findNodesOfType(nodes, ExecutionNode::SUBQUERY, true);

  for (auto& it : nodes) {
    subqueries.try_emplace(
        ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery(), it);
  }

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateIResearchViewNode
  nodes.clear();
  plan->findNodesOfType(nodes, ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  for (auto* node : nodes) {
    TRI_ASSERT(node);
    auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);
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
    auto scatterNode = plan->registerNode(std::make_unique<ScatterNode>(
        plan.get(), plan->nextId(), ScatterNode::ScatterType::SHARD));
    TRI_ASSERT(!deps.empty());
    scatterNode->addDependency(deps[0]);

    // insert a remote node
    auto* remoteNode = plan->registerNode(std::make_unique<RemoteNode>(
        plan.get(), plan->nextId(), &vocbase, "", "", ""));
    TRI_ASSERT(scatterNode);
    remoteNode->addDependency(scatterNode);
    node->addDependency(remoteNode);  // re-link with the remote node

    // insert another remote node
    remoteNode = plan->registerNode(std::make_unique<RemoteNode>(
        plan.get(), plan->nextId(), &vocbase, "", "", ""));
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

}  // namespace arangodb::iresearch
