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

#include "HandleViews.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/CalculationNodeVarFinder.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/NoResultsNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/IResearchViewSortHelpers.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/Search.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/Optimizer/Utils/ReplaceOffsetInfo.h"
#endif

#include <utils/misc.hpp>
#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {

#ifndef USE_ENTERPRISE
Variable const* replaceOffsetInfo(aql::CalculationNode& calcNode,
                                  aql::Variable const& ref,
                                  aql::AstNode const& node);
Variable const* replaceOffsetInfo(aql::CalculationNode& calcNode,
                                  aql::Variable const& ref,
                                  aql::AstNode const& node) {
  functions::NotImplementedEE(nullptr, node, {});  // will throw
  return nullptr;
}
#endif

namespace {

using EN = aql::ExecutionNode;

/// @brief Moves all FCALLs in every AND node to the bottom of the AND node
/// @param condition SEARCH condition node
/// @param starts_with Function to push
void pushFuncToBack(aql::AstNode& condition,
                    aql::Function const* starts_with) {
  auto numMembers = condition.numMembers();
  for (size_t memberIdx = 0; memberIdx < numMembers; ++memberIdx) {
    auto current = condition.getMemberUnchecked(memberIdx);
    TRI_ASSERT(current);
    auto const numAndMembers = current->numMembers();
    if (current->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND &&
        numAndMembers > 1) {
      size_t movePoint = numAndMembers - 1;
      auto isFunctionCall = [starts_with](aql::AstNode const* node) -> bool {
        if (node->type != aql::AstNodeType::NODE_TYPE_FCALL) return false;
        aql::ast::FunctionCallNode fcall(node);
        return fcall.getFunction() == starts_with;
      };
      do {
        auto candidate = current->getMemberUnchecked(movePoint);
        if (!isFunctionCall(candidate)) {
          break;
        }
      } while ((--movePoint) != 0);
      for (size_t andMemberIdx = 0; andMemberIdx < movePoint; ++andMemberIdx) {
        auto andMember = current->getMemberUnchecked(andMemberIdx);
        if (isFunctionCall(andMember)) {
          TEMPORARILY_UNLOCK_NODE(current);
          auto tmp = current->getMemberUnchecked(movePoint);
          current->changeMember(movePoint--, andMember);
          current->changeMember(andMemberIdx, tmp);
          while (movePoint > andMemberIdx &&
                 isFunctionCall(current->getMemberUnchecked(movePoint))) {
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

bool addView(LogicalView const& view, aql::QueryContext& query) {
  auto& collections = query.collections();

  // linked collections
  auto visitor = [&collections](DataSourceId cid, LogicalView::Indexes*) {
    collections.add(basics::StringUtils::itoa(cid.id()),
                    AccessMode::Type::READ,
                    aql::Collection::Hint::Collection);
    return true;
  };

  return view.visitCollections(visitor);
}

bool optimizeSearchCondition(IResearchViewNode& viewNode,
                             aql::QueryContext& query,
                             aql::ExecutionPlan& plan) {
  auto view = viewNode.view();

  // add view and linked collections to the query
  if (!addView(*view, query)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_PARSE,
        absl::StrCat("failed to process all collections linked with the view '",
                     view->name(), "'"));
  }

  // build search condition
  aql::Condition searchCondition(plan.getAst());

  auto& nodeFilter = viewNode.filterCondition();
  if (!isFilterConditionEmpty(&nodeFilter)) {
    searchCondition.andCombine(&nodeFilter);
    searchCondition.normalize(&plan, true,
                              viewNode.options().conditionOptimization);

    if (searchCondition.isEmpty()) {
      // condition is always false
      for (auto const& x : viewNode.getParents()) {
        plan.insertDependency(
            x, plan.registerNode(
                   std::make_unique<aql::NoResultsNode>(&plan, plan.nextId())));
      }
      return false;
    }

    auto const& varsValid = viewNode.getVarsValid();

    // remove all invalid variables from the condition
    [[maybe_unused]] bool noRemoves = true;
    if (searchCondition.removeInvalidVariables(varsValid, noRemoves)) {
      // removing left a previously non-empty OR block empty...
      // this means we can't use the index to restrict the results
      return false;
    }
  }

  // check filter condition if present
  if (searchCondition.root()) {
    if (viewNode.filterOptimization() != FilterOptimization::NONE) {
      // we could benefit from merging STARTS_WITH and LEVENSHTEIN_MATCH
      auto& server = plan.getAst()->query().vocbase().server();
      auto starts_with =
          server.getFeature<aql::AqlFunctionFeature>().byName("STARTS_WITH");
      TRI_ASSERT(starts_with);
      pushFuncToBack(*searchCondition.root(), starts_with);
    }

    QueryContext ctx{
        .trx = &query.trxForOptimization(),
        .ref = &viewNode.outVariable(),
        // we don't care here as we are checking condition in general
        .namePrefix = nestedRoot(false),
        .isSearchQuery = true,
        .isOldMangling = (viewNode.meta() == nullptr)};

    // The analyzer is referenced in the FilterContext and used during the
    // following ::makeFilter() call, so may not be a temporary.
    FilterContext const filterCtx{.query = ctx,
                                  .contextAnalyzer = FieldMeta::identity()};

    auto filterCreated =
        FilterFactory::filter(nullptr, filterCtx, *searchCondition.root());

    if (filterCreated.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          filterCreated.errorNumber(),
          absl::StrCat("unsupported SEARCH condition: ",
                       filterCreated.errorMessage()));
    }
  }

  if (!searchCondition.isEmpty()) {
    viewNode.setFilterCondition(searchCondition.root());
  }

  return true;
}

bool optimizeSort(IResearchViewNode& viewNode, aql::ExecutionPlan* plan) {
  auto const& primarySort = getPrimarySort(viewNode.meta(), viewNode.view());

  if (primarySort.empty()) {
    // use system sort
    return false;
  }

  std::unordered_map<aql::VariableId, aql::AstNode const*>
      variableDefinitions;

  EN* current = static_cast<EN*>(&viewNode);

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
        current->getType() == EN::ENUMERATE_PATHS ||
        current->getType() == EN::INDEX ||
        current->getType() == EN::JOIN ||
        current->getType() == EN::COLLECT) {
      // any of these node types will lead to more/less results in the output,
      // and may as well change the sort order, so let's better abort here
      return false;
    }

    if (current->getType() == EN::CALCULATION) {
      // pick up the meanings of variables as we walk the plan
      variableDefinitions.try_emplace(
          EN::castTo<aql::CalculationNode const*>(current)
              ->outVariable()
              ->id,
          EN::castTo<aql::CalculationNode const*>(current)
              ->expression()
              ->node());
    }

    if (current->getType() != EN::SORT) {
      // from here on, we are only interested in sorts
      continue;
    }

    std::vector<std::pair<aql::Variable const*, bool>> sorts;

    auto* sortNode = EN::castTo<aql::SortNode*>(current);
    auto const& sortElements = sortNode->elements();

    sorts.reserve(sortElements.size());
    for (auto& it : sortElements) {
      // note: in contrast to regular indexes, views support sorting in
      // different directions for multiple fields (e.g. SORT doc.a ASC, doc.b
      // DESC). this is not supported by indexes
      sorts.emplace_back(it.var, it.ascending);
    }

    aql::SortCondition sortCondition(
        plan, sorts,
        std::vector<std::vector<basics::AttributeName>>(),
        ::arangodb::containers::HashSet<
            std::vector<basics::AttributeName>>(),
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
      if (current->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
          current->getType() == EN::ENUMERATE_COLLECTION ||
          current->getType() == EN::TRAVERSAL ||
          current->getType() == EN::SHORTEST_PATH ||
          current->getType() == EN::ENUMERATE_PATHS ||
          current->getType() == EN::INDEX ||
          current->getType() == EN::JOIN ||
          current->getType() == EN::COLLECT ||
          current->getType() == EN::SORT) {
        // any of these node types will lead to more/less results in the
        // output, and may as well change the sort order, so let's better
        // abort here
        return false;
      }
    }

    assert(!primarySort.empty());
    viewNode.setSort(primarySort, sortElements.size());

    sortNode->dontReinsertInCluster();
    if (!ServerState::instance()->isCoordinator()) {
      // in cluster node will be unlinked later by
      // 'distributeSortToClusterRule'
      plan->unlinkNode(sortNode);
    }

    return true;
  }
}

void keepReplacementViewVariables(std::span<EN* const> calcNodes,
                                  std::span<EN* const> viewNodes) {
  std::vector<aql::latematerialized::NodeWithAttrsColumn> nodesToChange;
  std::vector<std::vector<aql::latematerialized::ColumnVariant<false>>>
      usedColumnsCounter;
  for (auto* vNode : viewNodes) {
    TRI_ASSERT(vNode && EN::ENUMERATE_IRESEARCH_VIEW == vNode->getType());
    auto& viewNode = *EN::castTo<IResearchViewNode*>(vNode);
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

    // TODO: this optimization is somewhat insufficient:
    // we are only checking here if the attributes extracted from the view
    // are used in following nodes of type CalculationNode. only the usage
    // of view attributes in such nodes will be detected here.
    // usage of view attributes in other nodes will not be detected.
    // for example, the following usage of `doc._id` will not be detected:
    //   FOR doc IN view
    //     FOR v, e, p IN 1..1 OUTBOUND doc ...
    // but this one will:
    //   FOR doc IN view
    //     FOR v, e, p IN 1..1 OUTBOUND doc._id ...
    // detecting the usage of view attributes is important to pull of the
    // storedValues optimization and not materialize the full documents.
    for (auto* cNode : calcNodes) {
      TRI_ASSERT(cNode && EN::CALCULATION == cNode->getType());
      auto& calcNode = *EN::castTo<aql::CalculationNode*>(cNode);
      auto* astNode = calcNode.expression()->nodeForModification();
      TRI_ASSERT(astNode);
      aql::latematerialized::NodeWithAttrsColumn node;
      node.node = &calcNode;
      // find attributes referenced to view node out variable
      if (aql::latematerialized::getReferencedAttributes(astNode, &var, node)) {
        if (!node.attrs.empty()) {
          if (aql::latematerialized::attributesMatch(
                  primarySort, storedValues, node.attrs, usedColumnsCounter,
                  columnsCount)) {
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
      aql::latematerialized::setAttributesMaxMatchedColumns(usedColumnsCounter,
                                                            columnsCount);
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
    std::span<EN* const> viewNodes,
    containers::HashSet<EN*>& toUnlink) {
  auto modified = false;
  aql::VarSet currentUsedVars;
  for (auto* node : viewNodes) {
    TRI_ASSERT(node && EN::ENUMERATE_IRESEARCH_VIEW == node->getType());
    auto& viewNode = *EN::castTo<IResearchViewNode*>(node);
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
          case EN::CALCULATION:
            isCalcNodesFound = true;
            break;
          case EN::SUBQUERY: {
            auto& subqueryNode =
                *EN::castTo<aql::SubqueryNode*>(current);
            auto* subquery = subqueryNode.getSubquery();
            TRI_ASSERT(subquery);
            aql::CalculationNodeVarExistenceFinder finder(&var);
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
    aql::ast::FunctionCallNode fcall(&node);
    auto* impl = fcall.getFunction();

    if (isScorer(*impl)) {
      return {getSearchFuncRef(fcall.getArguments()), SearchFuncType::kScorer};
    } else if (isOffsetInfo(*impl)) {
      return {getSearchFuncRef(fcall.getArguments()),
              SearchFuncType::kOffsetInfo};
    }
  }

  return {nullptr, SearchFuncType::kInvalid};
}

aql::Variable const* replaceScorer(aql::CalculationNode& calcNode,
                                   aql::Variable const& ref,
                                   aql::AstNode const& node) {
  QueryContext const ctx{.ref = &ref};

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
  } else if (bool const hasFunc = !visit<true>(
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

void extractScorers(IResearchViewNode const& viewNode,
                    DedupSearchFuncs& dedup,
                    std::vector<SearchFunc>& funcs) {
  auto* viewVar = &viewNode.outVariable();
  aql::VarSet usedVars;

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
          auto const funcName = getFuncName(*func.node);

          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_BAD_PARAMETER,
              "Inaccesible non-ArangoSearch view variable '%s' is used in "
              "search function '%s'",
              v->name.c_str(), funcName.data());
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

void handleViewsRule(aql::Optimizer* opt,
                     std::unique_ptr<aql::ExecutionPlan> plan,
                     aql::OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  irs::Finally addPlan = [opt, &plan, &rule, &modified]() noexcept {
    opt->addPlan(std::move(plan), rule, modified);
  };

  // cppcheck-suppress accessMoved
  if (!plan->contains(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW)) {
    // no view present in the query, so no need to do any expensive
    // transformations
    return;
  }

  // register replaced scorers to be evaluated by corresponding view nodes
  containers::SmallVector<aql::ExecutionNode*, 8> viewNodes;
  plan->findNodesOfType(viewNodes,
                        aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  // replace scorers in all calculation nodes with references
  containers::SmallVector<aql::ExecutionNode*, 8> calcNodes;
  plan->findNodesOfType(calcNodes, aql::ExecutionNode::CALCULATION, true);

  DedupSearchFuncs searchFuncs;

  for (auto* node : calcNodes) {
    TRI_ASSERT(node &&
               aql::ExecutionNode::CALCULATION == node->getType());
    replaceSearchFunc(
        *aql::ExecutionNode::castTo<aql::CalculationNode*>(node), searchFuncs);
  }
  modified = !searchFuncs.empty();

  aql::QueryContext& query = plan->getAst()->query();
  std::vector<SearchFunc> scorers;

  for (auto* node : viewNodes) {
    TRI_ASSERT(node);
    TRI_ASSERT(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW ==
               node->getType());
    auto& viewNode =
        *aql::ExecutionNode::castTo<IResearchViewNode*>(node);

    if (viewNode.isBuilding()) {
      query.warnings().registerWarning(
          TRI_ERROR_ARANGO_INCOMPLETE_READ,
          absl::StrCat(
              "ArangoSearch view '", viewNode.view()->name(),
              "' building is in progress. Results can be incomplete."));
    }

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
  containers::HashSet<aql::ExecutionNode*> toUnlink;
  modified |= noDocumentMaterialization(viewNodes, toUnlink);
  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  // ensure all replaced scorers are covered by corresponding view nodes
  for (auto& [func, _] : searchFuncs) {
    TRI_ASSERT(func.node);
    auto const& node = *func.node;

    if (isScorer(node)) {
      auto const funcName = getFuncName(node);

      THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "Non ArangoSearch view variable '%s' is used in scorer function '%s'",
          func.var->name.c_str(), funcName.data());
    }
  }
}

}  // namespace arangodb::iresearch
