////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "PersistentIndexAttributeMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/StringRef.h"
#include "Indexes/Index.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SkiplistIndexAttributeMatcher.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

bool PersistentIndexAttributeMatcher::accessFitsIndex(
    arangodb::Index const* idx, arangodb::aql::AstNode const* access,
    arangodb::aql::AstNode const* other, arangodb::aql::AstNode const* op,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    std::unordered_set<std::string>& nonNullAttributes, bool isExecution) {

  // just forward to reference implementation
  return SkiplistIndexAttributeMatcher::accessFitsIndex(idx, access, other, op, reference, found, nonNullAttributes, isExecution);
}

void PersistentIndexAttributeMatcher::matchAttributes(
    arangodb::Index const* idx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    size_t& values, std::unordered_set<std::string>& nonNullAttributes,
    bool isExecution) {

  // just forward to reference implementation
  return SkiplistIndexAttributeMatcher::matchAttributes(idx, node, reference, found, values, nonNullAttributes, isExecution);
}

bool PersistentIndexAttributeMatcher::supportsFilterCondition(
    arangodb::Index const* idx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) {
  // mmfiles failure point compat
  if (idx->type() == Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_IF_FAILURE("SimpleAttributeMatcher::accessFitsIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  std::unordered_set<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(idx, node, reference, found, values, nonNullAttributes,
                  false);

  bool lastContainsEquality = true;
  size_t attributesCovered = 0;
  size_t attributesCoveredByEquality = 0;
  double equalityReductionFactor = 20.0;
  estimatedCost = static_cast<double>(itemsInIndex);

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    // check if the current condition contains an equality condition
    auto const& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    ++attributesCovered;
    if (containsEquality) {
      ++attributesCoveredByEquality;
      estimatedCost /= equalityReductionFactor;

      // decrease the effect of the equality reduction factor
      equalityReductionFactor *= 0.25;
      if (equalityReductionFactor < 2.0) {
        // equalityReductionFactor shouldn't get too low
        equalityReductionFactor = 2.0;
      }
    } else {
      // quick estimate for the potential reductions caused by the conditions
      if (nodes.size() >= 2) {
        // at least two (non-equality) conditions. probably a range with lower
        // and upper bound defined
        estimatedCost /= 7.5;
      } else {
        // one (non-equality). this is either a lower or a higher bound
        estimatedCost /= 2.0;
      }
    }

    lastContainsEquality = containsEquality;
  }

  if (values == 0) {
    values = 1;
  }

  if (attributesCoveredByEquality == idx->fields().size() && idx->unique()) {
    // index is unique and condition covers all attributes by equality
    if (itemsInIndex == 0) {
      estimatedItems = 0;
      estimatedCost = 0.0;
      return true;
    }

    estimatedItems = values;
    estimatedCost = static_cast<double>(estimatedItems * values);
    // cost is already low... now slightly prioritize unique indexes
    estimatedCost *= 0.995 - 0.05 * (idx->fields().size() - 1);
    return true;
  }

  if (attributesCovered > 0 &&
      (!idx->sparse() || attributesCovered == idx->fields().size())) {
    // if the condition contains at least one index attribute and is not
    // sparse,
    // or the index is sparse and all attributes are covered by the condition,
    // then it can be used (note: additional checks for condition parts in
    // sparse indexes are contained in Index::canUseConditionPart)
    estimatedItems = static_cast<size_t>((std::max)(
        static_cast<size_t>(estimatedCost * values), static_cast<size_t>(1)));

    // check if the index has a selectivity estimate ready
    if (idx->hasSelectivityEstimate() &&
        attributesCoveredByEquality == idx->fields().size()) {
      StringRef ignore;
      double estimate = idx->selectivityEstimate(&ignore);
      if (estimate > 0.0) {
        estimatedItems = static_cast<size_t>(1.0 / estimate);
      }
    }
    if (itemsInIndex == 0) {
      estimatedCost = 0.0;
    } else {
      // simon: neither RocksDBVPackIndex nor MMFilesPersistentIndex have caches
      /*if (useCache()) {
       estimatedCost = static_cast<double>(estimatedItems * values) -
       (_fields.size() - 1) * 0.01;
       } else {*/
      estimatedCost =
          (std::max)(static_cast<double>(1),
                     std::log2(static_cast<double>(itemsInIndex)) * values) -
          (idx->fields().size() - 1) * 0.01;
      //}
    }
    return true;
  }

  // index does not help for this condition
  estimatedItems = itemsInIndex;
  estimatedCost = static_cast<double>(estimatedItems);
  return false;
}

bool PersistentIndexAttributeMatcher::supportsSortCondition(
    arangodb::Index const* idx,
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) {
  TRI_ASSERT(sortCondition != nullptr);

  if (!idx->sparse()) {
    // only non-sparse indexes can be used for sorting
    if (!idx->hasExpansion() && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      coveredAttributes =
          sortCondition->coveredAttributes(reference, idx->fields());

      if (coveredAttributes >= sortCondition->numAttributes()) {
        // sort is fully covered by index. no additional sort costs!
        // forward iteration does not have high costs
        estimatedCost = itemsInIndex * 0.001;
        if (sortCondition->isDescending()) {
          // reverse iteration has higher costs than forward iteration
          estimatedCost *= 4;
        }
        return true;
      } else if (coveredAttributes > 0) {
        estimatedCost = (itemsInIndex / coveredAttributes) *
                        std::log2(static_cast<double>(itemsInIndex));
        if (sortCondition->isAscending()) {
          // reverse iteration is more expensive
          estimatedCost *= 4;
        }
        return true;
      }
    }
  }

  coveredAttributes = 0;
  // by default no sort conditions are supported
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
    // slightly penalize this type of index against other indexes which
    // are in memory
    estimatedCost *= 1.05;
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* PersistentIndexAttributeMatcher::specializeCondition(
    arangodb::Index const* idx, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) {
  // mmfiles failure compat
  if (idx->type() == Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_IF_FAILURE("SimpleAttributeMatcher::specializeAllChildrenEQ") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("SimpleAttributeMatcher::specializeAllChildrenIN") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  // just forward to reference implementation
  return SkiplistIndexAttributeMatcher::specializeCondition(idx, node, reference);
}

bool PersistentIndexAttributeMatcher::isDuplicateOperator(
    arangodb::aql::AstNode const* node,
    std::unordered_set<int> const& operatorsFound) {

  // just forward to reference implementation
  return SkiplistIndexAttributeMatcher::isDuplicateOperator(node, operatorsFound);
}
