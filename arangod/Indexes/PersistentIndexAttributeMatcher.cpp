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
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::Index const* idx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) {
  // mmfiles failure point compat
  if (idx->type() == Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_IF_FAILURE("SimpleAttributeMatcher::accessFitsIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  
  // just forward to reference implementation
  return SkiplistIndexAttributeMatcher::supportsFilterCondition(allIndexes, idx, node, reference, itemsInIndex, estimatedItems, estimatedCost);
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
