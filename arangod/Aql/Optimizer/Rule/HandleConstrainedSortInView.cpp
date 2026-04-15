////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "HandleConstrainedSortInView.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/IResearchViewSortHelpers.h"
#include "Aql/Optimizer/Utils/LateMaterializedCommon.h"
#include "Aql/OptimizerRule.h"
#include "Aql/TypedAstNodes.h"
#include "Cluster/ServerState.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/Search.h"

#include <utils/misc.hpp>

namespace arangodb::iresearch {

namespace {

using EN = aql::ExecutionNode;

bool optimizeScoreSort(IResearchViewNode& viewNode, aql::ExecutionPlan* plan) {
  auto current = static_cast<EN*>(&viewNode);
  auto const& viewVariable = viewNode.outVariable();
  auto& scorers = viewNode.scorers();
  aql::SortNode* sortNode = nullptr;
  aql::LimitNode const* limitNode = nullptr;
  QueryContext ctx{
      .ast = plan->getAst(), .ref = &viewVariable, .isSearchQuery = true};
  while ((current = current->getFirstParent())) {
    switch (current->getType()) {
      case EN::SORT:
        sortNode = EN::castTo<aql::SortNode*>(current);
        break;
      case EN::LIMIT:
        if (sortNode == nullptr) {
          return false;
        }
        limitNode = EN::castTo<aql::LimitNode*>(current);
        break;
      case EN::OFFSET_INFO_MATERIALIZE:
      case EN::CALCULATION:
        // Only deterministic calcs allowed
        // Otherwise optimization should be forbidden
        // as number of calls will be changed!
        if (!current->isDeterministic()) {
          return false;
        }
        break;
      default:
        return false;
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
  TRI_ASSERT(!sortElements.empty());
  std::vector<HeapSortElement> heapSort;
  std::vector<std::vector<aql::latematerialized::ColumnVariant<true>>>
      usedColumns;
  auto const& primarySort = getPrimarySort(viewNode.meta(), viewNode.view());
  auto const& storedValues = getStoredValues(viewNode.meta(), viewNode.view());
  auto const columnsCount = storedValues.columns().size() + 1;
  usedColumns.resize(columnsCount);
  std::vector<aql::latematerialized::AttributeAndField<
      aql::latematerialized::IndexFieldData>>
      attrs;
  // maps attribute bucket to actual sort bucket
  containers::FlatHashMap<size_t, size_t> storedMaps;
  for (auto const& sort : sortElements) {
    TRI_ASSERT(sort.var);
    auto const* varSetBy = plan->getVarSetBy(sort.var->id);
    TRI_ASSERT(varSetBy);
    switch (varSetBy->getType()) {
      case EN::ENUMERATE_IRESEARCH_VIEW: {
        if (&viewNode != varSetBy) {
          return false;
        }
        auto source = viewNode.getSourceColumnInfo(sort.var->id);
        TRI_ASSERT(source.first != std::numeric_limits<ptrdiff_t>::max());
        heapSort.push_back(HeapSortElement{.postfix = {},
                                           .source = source.first,
                                           .fieldNumber = source.second,
                                           .ascending = sort.ascending});
      } break;
      case EN::CALCULATION: {
        auto* calc =
            EN::castTo<aql::CalculationNode const*>(varSetBy);
        TRI_ASSERT(calc->expression());

        auto const* astCalcNode = calc->expression()->node();
        if (!ADB_UNLIKELY(astCalcNode)) {
          return false;
        }
        switch (astCalcNode->type) {
          case aql::AstNodeType::NODE_TYPE_REFERENCE: {
            // something produced by during search function replacement.
            // e.g. it is expected to be LET sortVar = scorerVar;
            auto sortVariable =
                aql::ast::ReferenceNode(astCalcNode).getVariable();
            TRI_ASSERT(sortVariable);
            auto const s = std::find_if(
                std::begin(scorers), std::end(scorers),
                [sortVariableId = sortVariable->id](auto const& t) noexcept {
                  return t.var->id == sortVariableId;
                });
            if (s == std::end(scorers)) {
              return false;
            }
            heapSort.push_back(HeapSortElement{
                .postfix = {},
                .source = std::distance(scorers.begin(), s),
                .ascending = sort.ascending,
            });
          } break;
          case aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS:
            if (checkAttributeAccess(astCalcNode, viewVariable, false)) {
              // direct access to view variable
              aql::latematerialized::AttributeAndField<
                  aql::latematerialized::IndexFieldData>
                  af;
              // sort.attributePath is empty so we extract the name ourselves
              std::string name;
              if (!nameFromAttributeAccess(name, *astCalcNode, ctx, false)) {
                return false;
              }
              try {
                TRI_ParseAttributeString(name, af.attr, false);
              } catch (basics::Exception const&) {
                return false;
              }
              heapSort.push_back(
                  HeapSortElement{.postfix = {}, .ascending = sort.ascending});
              attrs.push_back(std::move(af));
              storedMaps.insert({attrs.size() - 1, heapSort.size() - 1});
            } else {
              // this could be stored column access but with postfix.
              HeapSortElement element;
              size_t usedVars{0};
              visitReferencedVariables(
                  *astCalcNode, [&](aql::Variable const& var) {
                    if (++usedVars < 2) {
                      // only one variable allowed e.g. LET x = y.foo
                      auto column = viewNode.getSourceColumnInfo(var.id);
                      std::string name;
                      if (column.first !=
                              std::numeric_limits<ptrdiff_t>::max() &&
                          nameFromAttributeAccess(name, *astCalcNode, ctx,
                                                  false)) {
                        element.source = column.first;
                        element.fieldNumber = column.second;
                        element.postfix = name;
                        element.ascending = sort.ascending;
                      }
                    }
                  });
              if (usedVars != 1 || element.isScore()) {
                return false;
              }
              heapSort.push_back(std::move(element));
            }
            break;
          default:
            return false;
        }
      } break;
      default:
        TRI_ASSERT(false);
        return false;
    }
  }

  if (!attrs.empty()) {
    if (aql::latematerialized::attributesMatch<true>(
            primarySort, storedValues, attrs, usedColumns, columnsCount)) {
      aql::latematerialized::setAttributesMaxMatchedColumns<true>(usedColumns,
                                                                  columnsCount);
      auto const attrCount = attrs.size();
      for (size_t i = 0; i < attrCount; ++i) {
        auto const& a = attrs[i];
        TRI_ASSERT(storedMaps.contains(i));
        auto& sortBucket = heapSort[storedMaps[i]];
        sortBucket.source = a.afData.columnNumber;
        sortBucket.fieldNumber = a.afData.fieldNumber;
        TRI_ASSERT(sortBucket.postfix.empty());
        TRI_ASSERT(a.afData.field);
        auto const fieldSize = a.afData.field->size();

        if (fieldSize < a.attr.size()) {
          sortBucket.postfix = a.attr[fieldSize].name;
          for (size_t i = fieldSize + 1; i < a.attr.size(); i++) {
            sortBucket.postfix += ("." + a.attr[i].name);
          }
        }
      }
    } else {
      return false;
    }
  }
  if (auto& front = heapSort.front(); front.isScore() && front.source != 0) {
    auto idx = front.source;
    std::swap(scorers.front(), scorers[idx]);
    for (auto it = heapSort.begin(); it != heapSort.end(); ++it) {
      if (it->isScore() && it->source == 0) {
        it->source = idx;
      } else if (it->isScore() && it->source == idx) {
        it->source = 0;
      }
    }
  }
  TRI_ASSERT(!viewNode.isHeapSort());
  // all sort elements are covered by view's scorers / stored values
  viewNode.setHeapSort(std::move(heapSort),
                       limitNode->offset() + limitNode->limit());
  sortNode->dontReinsertInCluster();
  if (!ServerState::instance()->isCoordinator()) {
    // in cluster node will be unlinked later by 'distributeSortToClusterRule'
    plan->unlinkNode(sortNode);
  }
  return true;
}

}  // namespace

void handleConstrainedSortInView(aql::Optimizer* opt,
                                 std::unique_ptr<aql::ExecutionPlan> plan,
                                 aql::OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  irs::Finally addPlan = [opt, &plan, &rule, &modified]() noexcept {
    opt->addPlan(std::move(plan), rule, modified);
  };

  // cppcheck-suppress accessMoved
  if (!plan->contains(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW) ||
      !plan->contains(aql::ExecutionNode::SORT) ||
      !plan->contains(aql::ExecutionNode::LIMIT)) {
    // no view && sort && limit present in the query,
    // so no need to do any expensive transformations
    return;
  }

  containers::SmallVector<aql::ExecutionNode*, 8> viewNodes;
  plan->findNodesOfType(viewNodes,
                        aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
  for (auto* node : viewNodes) {
    TRI_ASSERT(node);
    TRI_ASSERT(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW ==
               node->getType());
    auto& viewNode =
        *aql::ExecutionNode::castTo<IResearchViewNode*>(node);
    if (viewNode.sort().first) {
      // this view already has PrimarySort - no sort for us.
      continue;
    }

    if (!viewNode.isInInnerLoop()) {
      // check if we can optimize away a sort that follows the EnumerateView
      // node this is only possible if the view node itself is not contained in
      // another loop
      modified |= optimizeScoreSort(viewNode, plan.get());
    }
  }
}

}  // namespace arangodb::iresearch
