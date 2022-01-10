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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SimpleAttributeEqualityMatcher.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/vocbase.h"

#include <cmath>

using namespace arangodb;

SimpleAttributeEqualityMatcher::SimpleAttributeEqualityMatcher(
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes)
    : _attributes(attributes) {}

/// @brief match a single of the attributes
/// this is used for the primary index and the edge index
Index::FilterCosts SimpleAttributeEqualityMatcher::matchOne(
    arangodb::Index const* index, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) {
  size_t postFilterConditions = 0;
  std::unordered_set<std::string> nonNullAttributes;
  _found.clear();

  Index::FilterCosts costs = Index::FilterCosts::defaultCosts(itemsInIndex);

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    arangodb::aql::AstNode const* which = nullptr;
    size_t values = 1;

    auto op = node->getMemberUnchecked(i);

    if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      TRI_ASSERT(op->numMembers() == 2);
      // EQ is symmetric
      if (accessFitsIndex(index, op->getMemberUnchecked(0),
                          op->getMemberUnchecked(1), op, reference,
                          nonNullAttributes, false)) {
        which = op->getMemberUnchecked(0);
      } else if (accessFitsIndex(index, op->getMemberUnchecked(1),
                                 op->getMemberUnchecked(0), op, reference,
                                 nonNullAttributes, false)) {
        which = op->getMemberUnchecked(1);
      }
    } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      TRI_ASSERT(op->numMembers() == 2);
      if (accessFitsIndex(index, op->getMemberUnchecked(0),
                          op->getMemberUnchecked(1), op, reference,
                          nonNullAttributes, false)) {
        // we can use the index
        // use slightly different cost calculation for IN than for EQ
        which = op->getMemberUnchecked(0);
        values = estimateNumberOfArrayMembers(op->getMemberUnchecked(1));
      }
    }

    if (which != nullptr) {
      // we can use the index for the condition
      costs = calculateIndexCosts(index, which, itemsInIndex, values, 1);
    } else {
      // we cannot use the index for the condition
      ++postFilterConditions;
    }
  }

  // honor the costs of post-index filter conditions
  costs.estimatedCosts += costs.estimatedItems * postFilterConditions;

  // set to defaults
  return costs;
}

/// @brief specialize the condition for the index
/// this is used for the primary index and the edge index
/// requires that a previous matchOne() returned true
arangodb::aql::AstNode* SimpleAttributeEqualityMatcher::specializeOne(
    arangodb::Index const* index, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) {
  std::unordered_set<std::string> nonNullAttributes;
  _found.clear();

  // must edit in place, no access to AST
  // TODO change so we can replace with copy
  TEMPORARILY_UNLOCK_NODE(node);

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto op = node->getMemberUnchecked(i);

    if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      TRI_ASSERT(op->numMembers() == 2);
      // EQ is symmetric
      if (accessFitsIndex(index, op->getMember(0), op->getMember(1), op,
                          reference, nonNullAttributes, false) ||
          accessFitsIndex(index, op->getMember(1), op->getMember(0), op,
                          reference, nonNullAttributes, false)) {
        // we can use the index
        // now return only the child node we need
        node->clearMembers();
        node->addMember(op);

        return node;
      }
    } else if (op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      TRI_ASSERT(op->numMembers() == 2);

      if (accessFitsIndex(index, op->getMember(0), op->getMember(1), op,
                          reference, nonNullAttributes, false)) {
        // we can use the index
        // now return only the child node we need
        node->clearMembers();
        node->addMember(op);

        return node;
      }
    }
  }

  TRI_ASSERT(false);
  return node;
}

/// @brief determine the costs of using this index and the number of items
/// that will return in average
/// cost values have no special meaning, except that multiple cost values are
/// comparable, and lower values mean lower costs
Index::FilterCosts SimpleAttributeEqualityMatcher::calculateIndexCosts(
    arangodb::Index const* idx, arangodb::aql::AstNode const* attribute,
    size_t itemsInIndex, size_t values, size_t coveredAttributes) const {
  // note: attribute will be set to the index attribute for single-attribute
  // indexes such as the primary and edge indexes, and is a nullptr for the
  // other indexes
  Index::FilterCosts costs = Index::FilterCosts::defaultCosts(itemsInIndex);
  costs.supportsCondition = true;
  costs.coveredAttributes = coveredAttributes;

  if (itemsInIndex > 0) {
    costs.estimatedItems = static_cast<size_t>(itemsInIndex * values);

    // the index mocks do not have a selectivity estimate...
    if (idx->hasSelectivityEstimate()) {
      // use index selectivity estimate
      std::string_view att;
      if (attribute != nullptr &&
          attribute->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
        att = attribute->getStringView();
      }
      double estimate = idx->selectivityEstimate(att);
      if (estimate > 0.0) {
        costs.estimatedItems = static_cast<size_t>(1.0 / estimate * values);
      }
    } else {
      // no selectivity estimate present. this should only happen for mock
      // indexes. anyway, use a hard-coded formula for determining the number of
      // results
      double equalityReductionFactor = 20.0;
      for (size_t i = 0; i < coveredAttributes; ++i) {
        costs.estimatedItems /= static_cast<size_t>(equalityReductionFactor);
        // decrease the effect of the equality reduction factor
        equalityReductionFactor *= 0.25;
        if (equalityReductionFactor < 2.0) {
          // equalityReductionFactor shouldn't get too low
          equalityReductionFactor = 2.0;
        }
      }
    }

    // normalizes costs
    Index::normalizeFilterCosts(costs, idx, itemsInIndex, values);
  }

  return costs;
}

/// @brief whether or not the access fits
bool SimpleAttributeEqualityMatcher::accessFitsIndex(
    arangodb::Index const* index, arangodb::aql::AstNode const* access,
    arangodb::aql::AstNode const* other, arangodb::aql::AstNode const* op,
    arangodb::aql::Variable const* reference,
    std::unordered_set<std::string>& nonNullAttributes, bool isExecution) {
  // op can be  ==, IN, >, <, !=, even though we do not support all of these
  // operators however, canUseConditionPart will help us fill the
  // "nonNullAttributes" set even for the not-supported operators, and we want
  // to make use of that so we can simply exit after canUseConditionPart for all
  // operators we actually don't support
  if (!index->canUseConditionPart(access, other, op, reference,
                                  nonNullAttributes, isExecution)) {
    return false;
  }

  if (op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ &&
      op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // we can only handle  ==  and  IN.
    // we can stop at any other comparison operator
    return false;
  }

  arangodb::aql::AstNode const* what = access;
  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>>
      attributeData;

  if (op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    if (!what->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }
    if (arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second)) {
      // doc.value[*] == 'value'
      return false;
    }
    if (index->isAttributeExpanded(attributeData.second)) {
      // doc.value == 'value' (with an array index)
      return false;
    }
  } else {
    // ok, we do have an IN here... check if it's something like 'value' IN
    // doc.value[*]
    TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
    bool canUse = false;

    if (what->isAttributeAccessForVariable(attributeData) &&
        attributeData.first == reference &&
        !arangodb::basics::TRI_AttributeNamesHaveExpansion(
            attributeData.second)) {
      // doc.value IN 'value'
      // can use this index
      canUse = true;
    } else {
      // check for  'value' IN doc.value  AND  'value' IN doc.value[*]
      what = other;
      if (what->isAttributeAccessForVariable(attributeData) &&
          attributeData.first == reference &&
          index->isAttributeExpanded(attributeData.second) &&
          index->attributeMatches(attributeData.second)) {
        canUse = true;
      }
    }

    if (!canUse) {
      return false;
    }
  }

  std::vector<arangodb::basics::AttributeName> const& fieldNames =
      attributeData.second;

  for (size_t i = 0; i < _attributes.size(); ++i) {
    if (_attributes[i].size() != fieldNames.size()) {
      // attribute path length differs
      continue;
    }
    if (index->isAttributeExpanded(i) &&
        op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // If this attribute is correct or not, it could only serve for IN
      continue;
    }

    bool match = true;
    for (size_t j = 0; j < _attributes[i].size(); ++j) {
      if (_attributes[i][j] != fieldNames[j]) {
        // special case: a[*] is identical to a, and a.b[*] is identical to a.b
        // general rule: if index attribute is expanded and last part in index,
        // then it can
        // be used in a query without expansion operator
        bool const isLast = (j == _attributes[i].size() - 1);

        if (!isLast || (!_attributes[i][j].shouldExpand) ||
            _attributes[i][j].name != fieldNames[j].name) {
          match = false;
          break;
        }
      }
    }

    if (match) {
      // mark ith attribute as being covered
      _found.try_emplace(i, op);
      TRI_IF_FAILURE("SimpleAttributeMatcher::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      return true;
    }
  }

  return false;
}

size_t SimpleAttributeEqualityMatcher::estimateNumberOfArrayMembers(
    aql::AstNode const* value) {
  if (value->isArray()) {
    // attr IN [ a, b, c ]  =>  this will produce multiple items, so count
    // them!
    return value->numMembers();
  }

  return defaultEstimatedNumberOfArrayMembers;  // just an estimate
}
