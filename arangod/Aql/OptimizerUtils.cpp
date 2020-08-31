////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerUtils.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"

namespace arangodb {
namespace aql {
namespace utils {

namespace {
/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
/// the usedIndexes vector may also be re-sorted
bool sortOrs(arangodb::aql::Ast* ast, arangodb::aql::AstNode* root,
             arangodb::aql::Variable const* variable,
             std::vector<std::shared_ptr<arangodb::Index>>& usedIndexes) {
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

  typedef std::pair<arangodb::aql::AstNode*, std::shared_ptr<arangodb::Index>> ConditionData;
  ::arangodb::containers::SmallVector<ConditionData*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ConditionData*> conditionData{a};

  auto cleanup = [&conditionData]() -> void {
    for (auto& it : conditionData) {
      delete it;
    }
  };

  TRI_DEFER(cleanup());

  std::vector<arangodb::aql::ConditionPart> parts;
  parts.reserve(n);

  std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>> result;

  for (size_t i = 0; i < n; ++i) {
    // sort the conditions of each AND
    AstNode* sub = root->getMemberUnchecked(i);

    TRI_ASSERT(sub != nullptr && sub->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    size_t const nAnd = sub->numMembers();

    if (nAnd != 1) {
      // we can't handle this one
      return false;
    }

    auto operand = sub->getMemberUnchecked(0);

    if (!operand->isComparisonOperator()) {
      return false;
    }

    if (operand->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      result.first = nullptr;
      result.second.clear();

      if (rhs->isConstant() && lhs->isAttributeAccessForVariable(result) &&
          result.first == variable &&
          (operand->type != arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN ||
           rhs->isArray())) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(result.first, result.second, operand,
                           arangodb::aql::AttributeSideType::ATTRIBUTE_LEFT, p);
      }
    }

    if (rhs->type == arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS ||
        rhs->type == arangodb::aql::AstNodeType::NODE_TYPE_EXPANSION) {
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
                           arangodb::aql::AttributeSideType::ATTRIBUTE_RIGHT, p);
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

    if (lhs.variable != rhs.variable || lhs.attributeName != rhs.attributeName) {
      // oops, the different OR parts are on different variables or attributes
      return false;
    }
  }

  size_t previousIn = SIZE_MAX;

  for (size_t i = 0; i < n; ++i) {
    auto& p = parts[i];

    if (p.operatorType == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        p.valueNode->isArray()) {
      TRI_ASSERT(p.valueNode->isConstant());

      if (previousIn != SIZE_MAX) {
        // merge IN with IN
        TRI_ASSERT(previousIn < i);
        auto emptyArray = ast->createNodeArray();
        auto mergedIn =
            ast->createNodeUnionizedArray(parts[previousIn].valueNode, p.valueNode);

        arangodb::aql::AstNode* clone = ast->clone(root->getMember(previousIn));
        root->changeMember(previousIn, clone);
        static_cast<ConditionData*>(parts[previousIn].data)->first = clone;

        clone = ast->clone(root->getMember(i));
        root->changeMember(i, clone);
        static_cast<ConditionData*>(parts[i].data)->first = clone;

        // can now edit nodes in place...
        parts[previousIn].valueNode = mergedIn;
        {
          auto n1 = root->getMember(previousIn)->getMember(0);
          TRI_ASSERT(n1->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
          TEMPORARILY_UNLOCK_NODE(n1);
          n1->changeMember(1, mergedIn);
        }

        p.valueNode = emptyArray;
        {
          auto n2 = root->getMember(i)->getMember(0);
          TRI_ASSERT(n2->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
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
            [](arangodb::aql::ConditionPart const& lhs,
               arangodb::aql::ConditionPart const& rhs) -> bool {
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
                res = CompareAstNodes(ll, lr, true);

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
  std::unordered_set<std::string> seenIndexConditions;

  // and rebuild
  for (size_t i = 0; i < n; ++i) {
    if (parts[i].operatorType == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        parts[i].valueNode->isArray() && parts[i].valueNode->numMembers() == 0) {
      // can optimize away empty IN array
      continue;
    }

    auto conditionData = static_cast<ConditionData*>(parts[i].data);
    bool isUnique = true;

    if (!usedIndexes.empty()) {
      // try to find duplicate condition parts, and only return each
      // unique condition part once
      try {
        std::string conditionString =
            conditionData->first->toString() + " - " +
            std::to_string(conditionData->second->id().id());
        isUnique = seenIndexConditions.emplace(std::move(conditionString)).second;
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
    std::vector<std::shared_ptr<Index>> const& indexes,
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const& sortCondition, size_t itemsInCollection,
    aql::IndexHint const& hint, std::vector<transaction::Methods::IndexHandle>& usedIndexes,
    arangodb::aql::AstNode*& specializedCondition, bool& isSparse) {
  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort = false;

  auto considerIndex = [&bestIndex, &bestCost, &bestSupportsFilter, &bestSupportsSort,
                        &indexes, node, reference, itemsInCollection,
                        &sortCondition](std::shared_ptr<Index> const& idx) -> void {
    TRI_ASSERT(!idx->inProgress());

    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;
    size_t coveredAttributes = 0;

    bool supportsFilter = false;
    bool supportsSort = false;

    // check if the index supports the filter condition
    Index::FilterCosts costs =
        idx->supportsFilterCondition(indexes, node, reference, itemsInIndex);

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

    if (sortCondition.isUnidirectional()) {
      // only go in here if we actually have a sort condition and it can in
      // general be supported by an index. for this, a sort condition must not
      // be empty, must consist only of attribute access, and all attributes
      // must be sorted in the direction
      Index::SortCosts sc =
          idx->supportsSortCondition(&sortCondition, reference, itemsInIndex);
      if (sc.supportsCondition) {
        supportsSort = true;
      }
      sortCost = sc.estimatedCosts;
      coveredAttributes = sc.coveredAttributes;
    }

    if (!supportsSort && isOnlyAttributeAccess && node->isOnlyEqualityMatch()) {
      // index cannot be used for sorting, but the filter condition consists
      // only of equality lookups (==)
      // now check if the index fields are the same as the sort condition fields
      // e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1, c.value2
      if (coveredAttributes == sortCondition.numAttributes() &&
          (idx->isSorted() || idx->fields().size() == sortCondition.numAttributes())) {
        // no sorting needed
        sortCost = 0.0;
      }
    }

    if (!supportsFilter && !supportsSort) {
      return;
    }

    double totalCost = filterCost;
    if (!sortCondition.isEmpty()) {
      // only take into account the costs for sorting if there is actually
      // something to sort
      if (supportsSort) {
        totalCost += sortCost;
      } else {
        totalCost +=
            Index::SortCosts::defaultCosts(itemsInIndex, idx->isPersistent()).estimatedCosts;
      }
    }

    LOG_TOPIC("7278d", TRACE, Logger::FIXME)
        << "looking at index: " << idx.get()
        << ", isSorted: " << idx->isSorted()
        << ", isSparse: " << idx->sparse()
        << ", fields: " << idx->fields().size()
        << ", supportsFilter: " << supportsFilter
        << ", supportsSort: " << supportsSort
        << ", filterCost: " << (supportsFilter ? filterCost : 0.0)
        << ", sortCost: " << (supportsSort ? sortCost : 0.0)
        << ", totalCost: " << totalCost
        << ", isOnlyAttributeAccess: " << isOnlyAttributeAccess
        << ", isUnidirectional: " << sortCondition.isUnidirectional()
        << ", isOnlyEqualityMatch: " << node->isOnlyEqualityMatch()
        << ", itemsInIndex/estimatedItems: " << itemsInIndex;

    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
      bestSupportsFilter = supportsFilter;
      bestSupportsSort = supportsSort;
    }
  };

  if (hint.type() == aql::IndexHint::HintType::Simple) {
    std::vector<std::string> const& hintedIndices = hint.hint();
    for (std::string const& hinted : hintedIndices) {
      std::shared_ptr<Index> matched;
      for (std::shared_ptr<Index> const& idx : indexes) {
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

    if (hint.isForced() && bestIndex == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
          "could not use index hint to serve query; " + hint.toString());
    }
  }

  if (bestIndex == nullptr) {
    for (auto const& idx : indexes) {
      considerIndex(idx);
    }
  }

  if (bestIndex == nullptr) {
    return std::make_pair(false, false);
  }

  specializedCondition = bestIndex->specializeCondition(node, reference);

  usedIndexes.emplace_back(bestIndex);
  isSparse = bestIndex->sparse();

  return std::make_pair(bestSupportsFilter, bestSupportsSort);
}

} // namespace


/// @brief Gets the best fitting index for one specific condition.
///        Difference to IndexHandles: Condition is only one NARY_AND
///        and the Condition stays unmodified. Also does not care for sorting
///        Returns false if no index could be found.

bool getBestIndexHandleForFilterCondition(aql::Collection const& collection,
                                          arangodb::aql::AstNode*& node,
                                          arangodb::aql::Variable const* reference,
                                          size_t itemsInCollection,
                                          aql::IndexHint const& hint,
                                          std::shared_ptr<Index>& usedIndex,
                                          bool onlyEdgeIndexes) {
  // We can only start after DNF transformation and only a single AND
  TRI_ASSERT(node->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
  if (node->numMembers() == 0) {
    // Well no index can serve no condition.
    return false;
  }

  auto indexes = collection.indexes();
  if (onlyEdgeIndexes) {
    indexes.erase(std::remove_if(indexes.begin(), indexes.end(),
                                 [](auto&& idx) {
                                   return idx->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX;
                                 }),
                  indexes.end());
  }

  arangodb::aql::SortCondition sortCondition;    // always empty here
  arangodb::aql::AstNode* specializedCondition;  // unused
  bool isSparse;                                 // unused
  std::vector<std::shared_ptr<Index>> usedIndexes;
  if (findIndexHandleForAndNode(indexes, node,
                                reference, sortCondition, itemsInCollection,
                                hint, usedIndexes, specializedCondition, isSparse)
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
    aql::Collection const& coll, arangodb::aql::Ast* ast,
    arangodb::aql::AstNode* root, arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    aql::IndexHint const& hint, std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted) {
  // We can only start after DNF transformation
  TRI_ASSERT(root->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR);
  auto indexes = coll.indexes();

  // must edit root in place; TODO change so we can replace with copy
  TEMPORARILY_UNLOCK_NODE(root);

  bool canUseForFilter = (root->numMembers() > 0);
  bool canUseForSort = false;
  bool isSparse = false;

  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto node = root->getMemberUnchecked(i);
    arangodb::aql::AstNode* specializedCondition = nullptr;
    auto canUseIndex = findIndexHandleForAndNode(indexes, node, reference, *sortCondition,
                                                 itemsInCollection, hint, usedIndexes,
                                                 specializedCondition, isSparse);

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

/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool getIndexForSortCondition(
    aql::Collection const& coll, arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    aql::IndexHint const& hint, std::vector<std::shared_ptr<Index>>& usedIndexes,
    size_t& coveredAttributes) {
  // We do not have a condition. But we have a sort!
  if (!sortCondition->isEmpty() && sortCondition->isOnlyAttributeAccess() &&
      sortCondition->isUnidirectional()) {
    double bestCost = 0.0;
    std::shared_ptr<Index> bestIndex;

    auto considerIndex = [reference, sortCondition, itemsInIndex, &bestCost, &bestIndex,
                          &coveredAttributes](std::shared_ptr<Index> const& idx) -> void {
      TRI_ASSERT(!idx->inProgress());

      Index::SortCosts costs =
          idx->supportsSortCondition(sortCondition, reference, itemsInIndex);
      if (costs.supportsCondition &&
          (bestIndex == nullptr || costs.estimatedCosts < bestCost)) {
        bestCost = costs.estimatedCosts;
        bestIndex = idx;
        coveredAttributes = costs.coveredAttributes;
      }
    };

    auto indexes = coll.indexes();

    if (hint.type() == aql::IndexHint::HintType::Simple) {
      std::vector<std::string> const& hintedIndices = hint.hint();
      for (std::string const& hinted : hintedIndices) {
        std::shared_ptr<Index> matched;
        for (std::shared_ptr<Index> const& idx : indexes) {
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

      if (hint.isForced() && bestIndex == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
            "could not use index hint to serve query; " + hint.toString());
      }
    }

    if (bestIndex == nullptr) {
      for (auto const& idx : indexes) {
        considerIndex(idx);
      }
    }

    if (bestIndex != nullptr) {
      usedIndexes.emplace_back(bestIndex);
    }

    return bestIndex != nullptr;
  }

  // No Index and no sort condition that
  // can be supported by an index.
  // Nothing to do here.
  return false;
}


} // namespace utils
} // namespace aql
} // namespace arangodb
