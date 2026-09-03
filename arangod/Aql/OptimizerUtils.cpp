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
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerUtils.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/Projections.h"
#include "Aql/QueryContext.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Basics/StaticStrings.h"
#include "IResearch/IResearchFeature.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Containers/SmallUnorderedMap.h"

#include <absl/strings/str_cat.h>

namespace arangodb::aql {

namespace {
/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
/// the usedIndexes vector may also be re-sorted
bool sortOrs(Ast* ast, AstNode* root, Variable const* variable,
             std::vector<std::shared_ptr<Index>>& usedIndexes) {
  if (root == nullptr) {
    return true;
  }

  size_t const n = root->numMembers();

  if (n < 2) {
    return true;
  }

  if (n != usedIndexes.size()) {
    // sorting will break if the number of ORs is unequal to the number of
    // indexes but we shouldn't have got here then
    TRI_ASSERT(false);
    return false;
  }

  typedef std::pair<AstNode*, std::shared_ptr<Index>> ConditionData;
  containers::SmallVector<ConditionData*, 8> conditionData;

  auto sg = scopeGuard([&conditionData]() noexcept -> void {
    for (auto& it : conditionData) {
      delete it;
    }
  });

  std::vector<ConditionPart> parts;
  parts.reserve(n);

  std::pair<Variable const*, std::vector<basics::AttributeName>> result;

  for (size_t i = 0; i < n; ++i) {
    // sort the conditions of each AND
    AstNode* sub = root->getMemberUnchecked(i);

    TRI_ASSERT(sub != nullptr &&
               sub->type == AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    // cppcheck-suppress nullPointerRedundantCheck
    size_t const nAnd = sub->numMembers();

    if (nAnd != 1) {
      // we can't handle this one
      return false;
    }

    auto operand = sub->getMemberUnchecked(0);

    if (!operand->isComparisonOperator()) {
      return false;
    }

    if (operand->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      result.first = nullptr;
      result.second.clear();

      if (rhs->isConstant() && lhs->isAttributeAccessForVariable(result) &&
          result.first == variable &&
          (operand->type != AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN ||
           rhs->isArray())) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(result.first, result.second, operand,
                           AttributeSideType::ATTRIBUTE_LEFT, p);
      }
    }

    if (rhs->type == AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS ||
        rhs->type == AstNodeType::NODE_TYPE_EXPANSION) {
      result.first = nullptr;
      result.second.clear();

      if (lhs->isConstant() && rhs->isAttributeAccessForVariable(result) &&
          result.first == variable) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(result.first, result.second, operand,
                           AttributeSideType::ATTRIBUTE_RIGHT, p);
      }
    }
  }

  if (parts.size() != root->numMembers()) {
    return false;
  }

  // check if all parts use the same variable and attribute
  for (size_t i = 1; i < n; ++i) {
    auto const& lhs = parts[i - 1];
    auto const& rhs = parts[i];

    if (lhs.variable != rhs.variable ||
        lhs.attributeName != rhs.attributeName) {
      // oops, the different OR parts are on different variables or attributes
      return false;
    }
  }

  size_t previousIn = SIZE_MAX;

  for (size_t i = 0; i < n; ++i) {
    auto& p = parts[i];

    if (p.operatorType == AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        p.valueNode->isArray()) {
      TRI_ASSERT(p.valueNode->isConstant());

      if (previousIn != SIZE_MAX) {
        // merge IN with IN
        TRI_ASSERT(previousIn < i);
        auto emptyArray = ast->createNodeArray();
        auto mergedIn = ast->createNodeUnionizedArray(
            parts[previousIn].valueNode, p.valueNode);

        AstNode* clone = ast->clone(root->getMember(previousIn));
        root->changeMember(previousIn, clone);
        static_cast<ConditionData*>(parts[previousIn].data)->first = clone;

        clone = ast->clone(root->getMember(i));
        root->changeMember(i, clone);
        static_cast<ConditionData*>(parts[i].data)->first = clone;

        // can now edit nodes in place...
        parts[previousIn].valueNode = mergedIn;
        {
          auto n1 = root->getMember(previousIn)->getMember(0);
          TRI_ASSERT(n1->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
          TEMPORARILY_UNLOCK_NODE(n1);
          n1->changeMember(1, mergedIn);
        }

        p.valueNode = emptyArray;
        {
          auto n2 = root->getMember(i)->getMember(0);
          TRI_ASSERT(n2->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
          TEMPORARILY_UNLOCK_NODE(n2);
          n2->changeMember(1, emptyArray);
        }

      } else {
        // note first IN
        previousIn = i;
      }
    }
  }

  // now sort all conditions by variable name, attribute name, attribute value
  std::sort(parts.begin(), parts.end(),
            [](ConditionPart const& lhs, ConditionPart const& rhs) -> bool {
              // compare variable names first
              auto res = lhs.variable->name.compare(rhs.variable->name);

              if (res != 0) {
                return res < 0;
              }

              // compare attribute names next
              res = lhs.attributeName.compare(rhs.attributeName);

              if (res != 0) {
                return res < 0;
              }

              // compare attribute values next
              auto ll = lhs.lowerBound();
              auto lr = rhs.lowerBound();

              if (ll == nullptr && lr != nullptr) {
                // left lower bound is not set but right
                return true;
              } else if (ll != nullptr && lr == nullptr) {
                // left lower bound is set but not right
                return false;
              }

              if (ll != nullptr && lr != nullptr) {
                // both lower bounds are set
                res = compareAstNodes(ll, lr, true);

                if (res != 0) {
                  return res < 0;
                }
              }

              if (lhs.isLowerInclusive() && !rhs.isLowerInclusive()) {
                return true;
              }
              if (rhs.isLowerInclusive() && !lhs.isLowerInclusive()) {
                return false;
              }

              // all things equal
              return false;
            });

  TRI_ASSERT(parts.size() == conditionData.size());

  // clean up
  root->clearMembers();

  usedIndexes.clear();
  containers::FlatHashSet<std::string> seenIndexConditions;

  // and rebuild
  for (size_t i = 0; i < n; ++i) {
    if (parts[i].operatorType == AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        parts[i].valueNode->isArray() &&
        parts[i].valueNode->numMembers() == 0) {
      // can optimize away empty IN array
      continue;
    }

    auto conditionData = static_cast<ConditionData*>(parts[i].data);
    bool isUnique = true;

    if (!usedIndexes.empty()) {
      // try to find duplicate condition parts, and only return each
      // unique condition part once
      try {
        auto conditionString =
            absl::StrCat(conditionData->first->toString(), " - ",
                         conditionData->second->id().id());
        isUnique =
            seenIndexConditions.emplace(std::move(conditionString)).second;
        // we already saw the same combination of index & condition
        // don't add it again
      } catch (...) {
        // condition stringification may fail. in this case, we simply carry own
        // without simplifying the condition
      }
    }

    if (isUnique) {
      root->addMember(conditionData->first);
      usedIndexes.emplace_back(conditionData->second);
    }
  }

  return true;
}

std::pair<bool, bool> findIndexHandleForAndNode(
    transaction::Methods& trx,
    std::vector<std::shared_ptr<Index>> const& indexes, AstNode* node,
    Variable const* reference, SortCondition const& sortCondition,
    size_t itemsInCollection, IndexHint const& hint,
    std::vector<transaction::Methods::IndexHandle>& usedIndexes,
    AstNode*& specializedCondition, bool& isSparse, bool failOnForcedHint,
    ReadOwnWrites readOwnWrites) {
  if (hint.isDisabled()) {
    // usage of index disabled via index hint: disableIndex: true
    return std::make_pair(false, false);
  }

  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort = false;

  auto considerIndex =
      [&trx, &bestIndex, &bestCost, &bestSupportsFilter, &bestSupportsSort,
       &indexes, node, reference, itemsInCollection, readOwnWrites,
       &sortCondition](std::shared_ptr<Index> const& idx) -> void {
    TRI_ASSERT(!idx->inProgress());

    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;
    size_t coveredAttributes = 0;

    bool supportsFilter = false;
    bool supportsSort = false;

    if (readOwnWrites == ReadOwnWrites::yes &&
        idx->type() == IndexType::Inverted) {
      // inverted index does not support ReadOwnWrites
      return;
    }

    // check if the index supports the filter condition
    Index::FilterCosts costs = idx->supportsFilterCondition(
        trx, indexes, node, reference, itemsInIndex);

    if (costs.supportsCondition) {
      // index supports the filter condition
      filterCost = costs.estimatedCosts;
      // this reduces the number of items left
      itemsInIndex = costs.estimatedItems;
      supportsFilter = true;
    } else {
      // index does not support the filter condition
      filterCost = itemsInIndex * 1.5;
    }

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    Index::SortCosts sc =
        idx->supportsSortCondition(&sortCondition, reference, itemsInIndex);
    if (sc.supportsCondition) {
      supportsSort = true;
    }
    sortCost = sc.estimatedCosts;
    coveredAttributes = sc.coveredAttributes;

    if (!supportsSort && isOnlyAttributeAccess && node->isOnlyEqualityMatch()) {
      // index cannot be used for sorting, but the filter condition consists
      // only of equality lookups (==)
      // now check if the index fields are the same as the sort condition fields
      // e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1, c.value2
      if (coveredAttributes == sortCondition.numAttributes() &&
          (idx->isSorted() ||
           idx->fields().size() == sortCondition.numAttributes())) {
        // no sorting needed
        sortCost = 0.0;
      }
    }

    if (!supportsFilter && !supportsSort) {
      return;
    }

    if (!sortCondition.isEmpty()) {
      // only take into account the costs for sorting if there is actually
      // something to sort
      if (!supportsSort) {
        sortCost = Index::SortCosts::defaultCosts(itemsInIndex).estimatedCosts;
      }
    } else {
      sortCost = 0.0;
    }

    double totalCost = filterCost + sortCost;

    // the more attributes an index contains, the more useful it will be for
    // projections.
    double projectionsFactor = 1.0 - ((idx->fields().size() - 1) * 0.02);
    totalCost *= projectionsFactor;

    LOG_TOPIC("7278d", TRACE, Logger::FIXME)
        << "looked at candidate index: " << idx->name()
        << ", isSorted: " << idx->isSorted() << ", isSparse: " << idx->sparse()
        << ", fields: " << idx->fields()
        << ", num fields: " << idx->fields().size()
        << ", hasSelectivityEstimate: " << idx->hasSelectivityEstimate()
        << ", selectivityEstimate: "
        << (idx->hasSelectivityEstimate()
                ? std::to_string(idx->selectivityEstimate())
                : "n/a")
        << ", supportsFilter: " << supportsFilter
        << ", supportsSort: " << supportsSort
        << ", projectionsFactor: " << projectionsFactor
        << ", isOnlyAttributeAccess: " << isOnlyAttributeAccess
        << ", isUnidirectional: " << sortCondition.isUnidirectional()
        << ", isOnlyEqualityMatch: " << node->isOnlyEqualityMatch()
        << ", itemsInIndex/estimatedItems: " << itemsInIndex
        << ", filterCost: " << filterCost << ", sortCost: " << sortCost
        << ", totalCost: " << totalCost;

    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
      bestSupportsFilter = supportsFilter;
      bestSupportsSort = supportsSort;
    }
  };

  if (hint.isSimple()) {
    std::vector<std::string> const& hintedIndices = hint.candidateIndexes();
    for (std::string const& hinted : hintedIndices) {
      std::shared_ptr<Index> matched;
      for (std::shared_ptr<Index> const& idx : indexes) {
        if (idx->inProgress()) {
          continue;
        }
        if (idx->name() == hinted) {
          matched = idx;
          break;
        }
      }

      if (matched != nullptr) {
        considerIndex(matched);
        if (bestIndex != nullptr) {
          break;
        }
      }
    }

    if (hint.isForced() && bestIndex == nullptr && failOnForcedHint) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
          absl::StrCat("could not use index hint to serve query; ",
                       hint.toString()));
    }
  }

  if (bestIndex == nullptr) {
    for (auto const& idx : indexes) {
      if (idx->inProgress()) {
        continue;
      }
      if (!Index::onlyHintForced(idx->type())) {
        considerIndex(idx);
      }
    }
  }

  if (bestIndex == nullptr) {
    // intentionally commented out here. can be enabled during development
    // LOG_TOPIC("3aac4", TRACE, Logger::FIXME) << "- no index used";
    return std::make_pair(false, false);
  }

  LOG_TOPIC("1d732", TRACE, Logger::FIXME)
      << "selected index: " << bestIndex.get()
      << ", isSorted: " << bestIndex->isSorted()
      << ", isSparse: " << bestIndex->sparse()
      << ", fields: " << bestIndex->fields().size();

  // intentionally commented out here. can be enabled during development
  // LOG_TOPIC("4b655", TRACE, Logger::FIXME) << "- picked: " <<
  // bestIndex.get();

  specializedCondition = bestIndex->specializeCondition(trx, node, reference);

  usedIndexes.emplace_back(bestIndex);
  isSparse = bestIndex->sparse();

  return std::make_pair(bestSupportsFilter, bestSupportsSort);
}

}  // namespace

namespace utils {

/// @brief Gets the best fitting index for one specific condition.
///        Difference to IndexHandles: Condition is only one NARY_AND
///        and the Condition stays unmodified. Also does not care for sorting
///        Returns false if no index could be found.

bool getBestIndexHandleForFilterCondition(
    transaction::Methods& trx, Collection const& collection, AstNode* node,
    Variable const* reference, size_t itemsInCollection, IndexHint const& hint,
    std::shared_ptr<Index>& usedIndex, ReadOwnWrites readOwnWrites,
    bool onlyEdgeIndexes) {
  // We can only start after DNF transformation and only a single AND
  TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
  if (node->numMembers() == 0) {
    // Well no index can serve no condition.
    return false;
  }

  auto indexes = collection.indexes();
  if (onlyEdgeIndexes) {
    indexes.erase(std::remove_if(indexes.begin(), indexes.end(),
                                 [](auto&& idx) {
                                   return idx->type() != IndexType::Edge;
                                 }),
                  indexes.end());
  }

  SortCondition sortCondition;    // always empty here
  AstNode* specializedCondition;  // unused
  bool isSparse;                  // unused
  std::vector<std::shared_ptr<Index>> usedIndexes;
  if (findIndexHandleForAndNode(trx, indexes, node, reference, sortCondition,
                                itemsInCollection, hint, usedIndexes,
                                specializedCondition, isSparse,
                                true /*failOnForcedHint*/, readOwnWrites)
          .first) {
    TRI_ASSERT(!usedIndexes.empty());
    usedIndex = usedIndexes[0];
    return true;
  }
  return false;
}

/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
    transaction::Methods& trx, Collection const& coll, Ast* ast, AstNode* root,
    Variable const* reference, SortCondition const* sortCondition,
    size_t itemsInCollection, IndexHint const& hint,
    std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted,
    bool& isAllCoveredByIndex, ReadOwnWrites readOwnWrites) {
  // We can only start after DNF transformation
  TRI_ASSERT(root->type == AstNodeType::NODE_TYPE_OPERATOR_NARY_OR);
  auto indexes = coll.indexes();

  // must edit root in place; TODO change so we can replace with copy
  TEMPORARILY_UNLOCK_NODE(root);

  bool canUseForFilter = (root->numMembers() > 0);
  bool canUseForSort = false;
  bool isSparse = false;

  TRI_ASSERT(usedIndexes.empty());

  // we might have an inverted index - it could cover whole condition at once.
  // Give it a try
  if (std::exchange(isAllCoveredByIndex, false)) {
    for (auto& index : indexes) {
      if (index->inProgress()) {
        continue;
      }
      if (readOwnWrites == ReadOwnWrites::yes &&
          index->type() == IndexType::Inverted) {
        // inverted index does not support ReadOwnWrites
        continue;
      }
      if (index->type() == IndexType::Inverted &&
          // apply this index only if hinted
          hint.isSimple() &&
          std::find(hint.candidateIndexes().begin(),
                    hint.candidateIndexes().end(),
                    index->name()) != hint.candidateIndexes().end()) {
        auto costs = index->supportsFilterCondition(
            trx, indexes, root, reference, itemsInCollection);
        if (costs.supportsCondition) {
          // we need to find 'root' in 'ast' and replace it with specialized
          // version but for now we know that index will not alter the node, so
          // just an assert
          index->specializeCondition(trx, root, reference);
          usedIndexes.emplace_back(index);
          isAllCoveredByIndex = true;
          // FIXME: we should somehow consider other indices and calculate here
          // "overall" score Also a question: if sort is covered but filter is
          // not ? What is more optimal?
          auto const sortSupport = index->supportsSortCondition(
              sortCondition, reference, itemsInCollection);
          return std::make_pair(true, sortSupport.supportsCondition);
        }
      }
    }
  }
  size_t const n = root->numMembers();
  for (size_t i = 0; i < n; ++i) {
    // BTS-398: if there are multiple OR-ed conditions, fail only for forced
    // index hints if no index can be found for _any_ condition part.
    auto node = root->getMemberUnchecked(i);
    AstNode* specializedCondition = nullptr;

    bool failOnForcedHint =
        (hint.isForced() && i + 1 == n && usedIndexes.empty());
    auto canUseIndex = findIndexHandleForAndNode(
        trx, indexes, node, reference, *sortCondition, itemsInCollection, hint,
        usedIndexes, specializedCondition, isSparse, failOnForcedHint,
        readOwnWrites);

    if (canUseIndex.second && !canUseIndex.first) {
      // index can be used for sorting only
      // we need to abort further searching and only return one index
      TRI_ASSERT(!usedIndexes.empty());
      if (usedIndexes.size() > 1) {
        auto sortIndex = usedIndexes.back();

        usedIndexes.clear();
        usedIndexes.emplace_back(sortIndex);
      }

      TRI_ASSERT(usedIndexes.size() == 1);

      if (isSparse) {
        // cannot use a sparse index for sorting alone
        usedIndexes.clear();
      }
      return std::make_pair(false, !usedIndexes.empty());
    }

    canUseForFilter &= canUseIndex.first;
    canUseForSort |= canUseIndex.second;

    root->changeMember(i, specializedCondition);
  }

  if (canUseForFilter) {
    isSorted = sortOrs(ast, root, reference, usedIndexes);
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return std::make_pair(canUseForFilter, canUseForSort);
}

Collection const* getCollection(ExecutionNode const* node) {
  using EN = ExecutionNode;

  switch (node->getType()) {
    case EN::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
          ->collection();
    case EN::INDEX:
      return ExecutionNode::castTo<IndexNode const*>(node)->collection();
    case EN::TRAVERSAL:
    case EN::ENUMERATE_PATHS:
    case EN::SHORTEST_PATH:
      return ExecutionNode::castTo<GraphNode const*>(node)->collection();

    default:
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have a collection");
  }
}

}  // namespace utils
}  // namespace arangodb::aql
