////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <cmath>

#include "SortedIndexAttributeMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Quantifier.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "VocBase/vocbase.h"
#include "Logger/LogMacros.h"

using namespace arangodb;

namespace {

bool isSupportedArrayOperator(aql::AstNode const* op) noexcept {
  if (op->type != aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN) {
    return false;
  }

  TRI_ASSERT(op->numMembers() == 3);
  TRI_ASSERT(op->getMemberUnchecked(2)->type == aql::NODE_TYPE_QUANTIFIER);

  auto quantifier = static_cast<aql::Quantifier::Type>(
      op->getMemberUnchecked(2)->getIntValue(true));
  return quantifier == aql::Quantifier::Type::kAny;
}

std::tuple<size_t, size_t, double, double> analyzeConditions(
    Index const* idx,
    containers::FlatHashMap<size_t, std::vector<aql::AstNode const*>> const&
        found) {
  size_t attributesCovered = 0;
  size_t attributesCoveredByEquality = 0;
  double equalityReductionFactor = 1.0;
  double nonEqualityReductionFactor = 1.0;
  double nextEqualityReductionValue = 20.0;
  bool lastContainsEquality = true;

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    auto it = found.find(i);

    if (it == found.end() || !lastContainsEquality) {
      // index attribute not covered by condition, or unsupported condition.
      // must abort
      break;
    }

    ++attributesCovered;

    // check if the current condition contains an equality condition
    auto const& nodes = (*it).second;
    bool containsEquality = false;
    for (size_t j = 0; j < nodes.size(); ++j) {
      if (nodes[j]->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
          nodes[j]->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        containsEquality = true;
        break;
      }
    }

    if (containsEquality) {
      ++attributesCoveredByEquality;

      equalityReductionFactor *= nextEqualityReductionValue;
      // decrease the effect of the equality reduction factor
      nextEqualityReductionValue *= 0.25;
    } else {
      // quick estimate for the potential reductions caused by the conditions
      if (nodes.size() >= 2) {
        // at least two (non-equality) conditions. probably a range with lower
        // and upper bound defined
        nonEqualityReductionFactor *= 7.5;
      } else {
        // one (non-equality). this is either a lower or a higher bound
        nonEqualityReductionFactor *= 2.0;
      }
    }

    lastContainsEquality = containsEquality;
  }

  return std::make_tuple(attributesCovered, attributesCoveredByEquality,
                         equalityReductionFactor, nonEqualityReductionFactor);
}

}  // namespace

bool SortedIndexAttributeMatcher::accessFitsIndex(
    Index const* idx,            // index
    aql::AstNode const* access,  // attribute access
    aql::AstNode const* other,   // eg const value
    aql::AstNode const*
        op,  // binary operation that is parent of access and other
    aql::Variable const* reference,  // variable used in access(es)
    containers::FlatHashMap<
        size_t /*offset in idx->fields()*/,
        std::vector<aql::AstNode const*> /*conjunct - operation*/>&
        found,  // marks operations covered by index-fields
    containers::FlatHashSet<std::string>&
        nonNullAttributes,  // set of stringified op-children (access other)
                            // that may not be null
    bool isExecution,       // skip usage check in execution phase
    bool supportsArrayOperations) {
  if (!idx->canUseConditionPart(access, other, op, reference, nonNullAttributes,
                                isExecution)) {
    return false;
  }

  std::pair<aql::Variable const*, std::vector<basics::AttributeName>>
      attributeData;
  bool const isPrimaryIndex =
      idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX;

  if (idx->type() == Index::IndexType::TRI_IDX_TYPE_TTL_INDEX &&
      (!other->isConstant() ||
       !(other->isIntValue() || other->isDoubleValue()))) {
    // TTL index can only be used for numeric lookup values, no date strings or
    // anything else
    // TODO: move this into the specific index class
    return false;
  }

  if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // ok, we do have an IN here... check if it's something like 'value' IN
    // doc.value[*]
    if (access->isAttributeAccessForVariable(attributeData) &&
        attributeData.first == reference &&
        !basics::TRI_AttributeNamesHaveExpansion(attributeData.second) &&
        idx->attributeMatches(attributeData.second, isPrimaryIndex)) {
      // doc.value IN 'value'
      // can use this index
    } else if (other->isAttributeAccessForVariable(attributeData) &&
               attributeData.first == reference &&
               idx->isAttributeExpanded(attributeData.second) &&
               idx->attributeMatches(attributeData.second, isPrimaryIndex)) {
      // check for  'value' IN doc.value  AND  'value' IN doc.value[*]
    } else {
      return false;
    }
  } else if (supportsArrayOperations && isSupportedArrayOperator(op)) {
    // ok, we do have an IN here... check if it's something like 'value'
    // <quantifier> IN doc.value
    if (access->isAttributeAccessForVariable(attributeData) &&
        attributeData.first == reference &&
        idx->attributeMatches(attributeData.second, isPrimaryIndex)) {
      // doc.value IN 'value'
      // can use this index
    } else {
      return false;
    }
  } else {
    if (!access->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }
    if (basics::TRI_AttributeNamesHaveExpansion(attributeData.second)) {
      // doc.value[*] == 'value'
      return false;
    }
    if (idx->isAttributeExpanded(attributeData.second)) {
      // doc.value == 'value' (with an array index)
      return false;
    }
  }

  std::vector<basics::AttributeName> const& fieldNames = attributeData.second;

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    if (idx->fields()[i].size() != fieldNames.size()) {
      // attribute path length differs
      continue;
    }

    if (idx->isAttributeExpanded(i) &&
        (op->type != aql::NODE_TYPE_OPERATOR_BINARY_IN &&
         op->type != aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN)) {
      // If this attribute is correct or not, it could only serve for IN
      continue;
    }

    bool match =
        basics::AttributeName::isIdentical(idx->fields()[i], fieldNames, true);

    // make exception for primary index as we do not need to match "_key, _id"
    // but can go directly for "_id"
    if (!match && isPrimaryIndex && i == 0 &&
        fieldNames[i].name == StaticStrings::IdString) {
      match = true;
    }

    if (match) {
      // mark ith attribute as being covered
      found[i].emplace_back(op);
      TRI_IF_FAILURE("PersistentIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      TRI_IF_FAILURE("SkiplistIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      TRI_IF_FAILURE("HashIndex::accessFitsIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      return true;
    }
  }

  return false;
}

void SortedIndexAttributeMatcher::matchAttributes(
    Index const* idx, aql::AstNode const* node, aql::Variable const* reference,
    containers::FlatHashMap<size_t, std::vector<aql::AstNode const*>>& found,
    size_t& values, containers::FlatHashSet<std::string>& nonNullAttributes,
    bool isExecution, bool supportsArrayOperations) {
  // assert we have a properly formed condition - nary conjunction
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  // inspect the conjuncts - allowed are binary comparisons and a contains check
  for (size_t i = 0; i < node->numMembers(); ++i) {
    auto op = node->getMemberUnchecked(i);

    switch (op->type) {
      case aql::NODE_TYPE_OPERATOR_BINARY_NE:
      case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case aql::NODE_TYPE_OPERATOR_BINARY_GE: {
        TRI_ASSERT(op->numMembers() == 2);
        accessFitsIndex(idx, op->getMemberUnchecked(0),
                        op->getMemberUnchecked(1), op, reference, found,
                        nonNullAttributes, isExecution,
                        supportsArrayOperations);
        accessFitsIndex(idx, op->getMemberUnchecked(1),
                        op->getMemberUnchecked(0), op, reference, found,
                        nonNullAttributes, isExecution,
                        supportsArrayOperations);
        break;
      }

      case aql::NODE_TYPE_OPERATOR_BINARY_IN: {
        if (accessFitsIndex(idx, op->getMemberUnchecked(0),
                            op->getMemberUnchecked(1), op, reference, found,
                            nonNullAttributes, isExecution,
                            supportsArrayOperations)) {
          if (op->getMemberUnchecked(1)->isAttributeAccessForVariable(
                  reference, /*indexed access*/ false)) {
            // 'abc' IN doc.attr[*]
            ++values;
          } else {
            size_t av =
                SimpleAttributeEqualityMatcher::estimateNumberOfArrayMembers(
                    op->getMemberUnchecked(1));
            if (av > 1) {
              // attr IN [ a, b, c ]  =>  this will produce multiple items, so
              // count them!
              values += av - 1;
            }
          }
        }
        break;
      }

      case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN: {
        if (supportsArrayOperations && isSupportedArrayOperator(op) &&
            accessFitsIndex(idx, op->getMemberUnchecked(1),
                            op->getMemberUnchecked(0), op, reference, found,
                            nonNullAttributes, isExecution,
                            supportsArrayOperations)) {
          if (op->getMemberUnchecked(1)->isAttributeAccessForVariable(
                  reference, /*indexed access*/ false)) {
            // 'abc' IN doc.attr[*]
            ++values;
          } else {
            size_t av =
                SimpleAttributeEqualityMatcher::estimateNumberOfArrayMembers(
                    op->getMemberUnchecked(1));
            if (av > 1) {
              // attr IN [ a, b, c ]  =>  this will produce multiple items, so
              // count them!
              values += av - 1;
            }
          }
        }
        break;
      }

      default:
        break;
    }
  }
}

Index::FilterCosts SortedIndexAttributeMatcher::supportsFilterCondition(
    std::vector<std::shared_ptr<Index>> const& allIndexes, Index const* idx,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex, bool supportsArrayOperations) {
  // mmfiles failure point compat
  if (idx->type() == Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_IF_FAILURE("SimpleAttributeMatcher::accessFitsIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  containers::FlatHashMap<size_t, std::vector<aql::AstNode const*>> found;
  containers::FlatHashSet<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(idx, node, reference, found, values, nonNullAttributes, false,
                  supportsArrayOperations);

  if (values == 0) {
    values = 1;
  }

  auto [attributesCovered, attributesCoveredByEquality, equalityReductionFactor,
        nonEqualityReductionFactor] = ::analyzeConditions(idx, found);

  TRI_ASSERT(equalityReductionFactor >= 1.0);
  TRI_ASSERT(nonEqualityReductionFactor >= 1.0);

  TRI_ASSERT(node->numMembers() >= attributesCovered);
  size_t postFilterConditions = node->numMembers() - attributesCovered;

  Index::FilterCosts costs =
      Index::FilterCosts::defaultCosts(itemsInIndex, values);
  costs.coveredAttributes = attributesCovered;

  // intentionally commented out here. can be enabled during development
  // LOG_TOPIC("592b7", TRACE, Logger::FIXME)
  //    << "itemsInIndex: " << itemsInIndex
  //    << ", values: " << values
  //    << ", postFilterConditions: " << postFilterConditions
  //    << ", attributesCovered: " << attributesCovered
  //    << ", attributesCoveredByEquality: " << attributesCoveredByEquality
  //    << ", equalityReductionFactor: " << equalityReductionFactor
  //    << ", nonEqualityReductionFactor: " << nonEqualityReductionFactor
  //    << ", defaults: " << costs.estimatedItems;

  if (attributesCovered > 0 &&
      (!idx->sparse() || attributesCovered == idx->fields().size())) {
    // if the condition contains at least one index attribute and is not sparse,
    // or the index is sparse and all attributes are covered by the condition,
    // then it can be used (note: additional checks for condition parts in
    // sparse indexes are contained in Index::canUseConditionPart)
    costs.supportsCondition = true;

    double estimatedItems = static_cast<double>(itemsInIndex * values);
    if (itemsInIndex > 0) {
      // check if the index has a selectivity estimate ready
      if (idx->hasSelectivityEstimate() &&
          attributesCoveredByEquality == idx->fields().size()) {
        // full match. all index attributes are looked up by equality
        double estimate = idx->selectivityEstimate();
        if (estimate > 0.0) {
          // use the seclitiy estimate
          estimatedItems = static_cast<double>(1.0 / estimate * values);
        } else {
          // use a guesstimate
          estimatedItems /= equalityReductionFactor;
        }
      } else if (attributesCoveredByEquality > 0) {
        TRI_ASSERT(attributesCovered > 0);
        // the index either does not have a selectivity estimate, or not all
        // of its attributes are covered by the condition using an equality
        // lookup however, if the search condition uses equality lookups on the
        // prefix of the index, then we can check if there is another index
        // which is just indexing the prefix, and "steal" the selectivity
        // estimate from that index for example, if the condition is "doc.a == 1
        // && doc.b > 2", and the current index is created on ["a", "b"], then
        // we will not use the selectivity estimate of the current index (due to
        // the range condition used for the second index attribute). however, if
        // there is another index on just "a", we know that the current index is
        // at least as selective as the index on the single attribute. and that
        // the extra condition we have will make it even more selectivity. so in
        // this case we will re-use the selectivity estimate from the other
        // index, and are happy.
        double otherEstimate = -1.0;

        for (auto const& otherIdx : allIndexes) {
          auto const* other = otherIdx.get();
          if (other == idx || !other->hasSelectivityEstimate()) {
            continue;
          }
          auto const& otherFields = other->fields();
          if (otherFields.size() > idx->fields().size()) {
            // filter out too long other indexes
            continue;
          }

          size_t matches = 0;
          for (size_t i = 0; i < otherFields.size(); ++i) {
            if (otherFields[i] != idx->fields()[i]) {
              break;
            }
            ++matches;

            if (matches > found.size()) {
              break;
            }
          }

          if (matches == otherFields.size()) {
            // the other index is a full prefix of our own index.
            // now check if the other index actually satisfies the filter
            // condition
            containers::FlatHashMap<size_t, std::vector<aql::AstNode const*>>
                foundOther;
            [[maybe_unused]] size_t valuesOther = 0;  // ignored here
            matchAttributes(otherIdx.get(), node, reference, foundOther,
                            valuesOther, nonNullAttributes, false,
                            supportsArrayOperations);

            auto [attributesCoveredOther, attributesCoveredByEqualityOther,
                  equalityReductionFactorOther,
                  nonEqualityReductionFactorOther] =
                ::analyzeConditions(otherIdx.get(), foundOther);

            // all attributes from the other index must be covered with equality
            // lookups, otherwise we cannot use the other index' selectivity
            // estimate
            if (foundOther.size() == matches &&
                attributesCoveredByEqualityOther == matches) {
              double estimate = other->selectivityEstimate();
              if (estimate > 0.0 && estimate > otherEstimate) {
                otherEstimate = estimate;
              }
            }
          }
        }

        if (otherEstimate > 0.0) {
          // reuse the estimate from the other index
          estimatedItems = static_cast<double>(1.0 / otherEstimate * values);
        } else {
          // use a guesstimate
          estimatedItems /= equalityReductionFactor;
        }

        estimatedItems /= nonEqualityReductionFactor;
      }

      costs.estimatedItems = static_cast<size_t>(estimatedItems);

      // normalizes costs
      Index::normalizeFilterCosts(costs, idx, itemsInIndex, values);
    }
  } else {
    // index does not help for this condition
    TRI_ASSERT(!costs.supportsCondition);
  }

  // honor the costs of post-index filter conditions
  costs.estimatedCosts += costs.estimatedItems * postFilterConditions;

  return costs;
}

Index::SortCosts SortedIndexAttributeMatcher::supportsSortCondition(
    Index const* idx, aql::SortCondition const* sortCondition,
    aql::Variable const* reference, size_t itemsInIndex) {
  TRI_ASSERT(sortCondition != nullptr);

  Index::SortCosts costs = Index::SortCosts::defaultCosts(itemsInIndex);

  if (!idx->sparse() ||
      sortCondition->onlyUsesNonNullSortAttributes(idx->fields())) {
    // non-sparse indexes can be used for sorting, but sparse indexes can only
    // be used if we can prove that we only need to return non-null index
    // attribute values
    if (!idx->hasExpansion() && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      costs.coveredAttributes =
          sortCondition->coveredAttributes(reference, idx->fields());

      if (costs.coveredAttributes >= sortCondition->numAttributes()) {
        // sort is fully covered by index. no additional sort costs!
        // forward iteration does not have high costs
        costs.supportsCondition = true;
        if (sortCondition->isDescending()) {
          // slightly penalize backward iteration
          costs.estimatedCosts = itemsInIndex * 0.001;
        } else {
          // forward iteration is cheap
          costs.estimatedCosts = 0.0;
        }
      } else if (costs.coveredAttributes > 0) {
        costs.estimatedCosts = (itemsInIndex / costs.coveredAttributes) *
                               std::log2(double(itemsInIndex));
        costs.supportsCondition = true;
      }
    }
  }

  return costs;
}

/// @brief specializes the condition for use with the index
aql::AstNode* SortedIndexAttributeMatcher::specializeCondition(
    Index const* idx, aql::AstNode* node, aql::Variable const* reference,
    bool supportsArrayOperations) {
  // mmfiles failure compat
  if (idx->type() == Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_IF_FAILURE("SimpleAttributeMatcher::specializeAllChildrenEQ") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("SimpleAttributeMatcher::specializeAllChildrenIN") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  containers::FlatHashMap<size_t, std::vector<aql::AstNode const*>> found;
  containers::FlatHashSet<std::string> nonNullAttributes;
  [[maybe_unused]] size_t values = 0;  // ignored here
  matchAttributes(idx, node, reference, found, values, nonNullAttributes, false,
                  supportsArrayOperations);

  std::vector<aql::AstNode const*> children;
  bool lastContainsEquality = true;

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    auto it = found.find(i);

    if (it == found.end() || !lastContainsEquality) {
      // index attribute not covered by condition, or unsupported condition.
      // must abort
      break;
    }

    // check if the current condition contains an equality condition
    auto& nodes = (*it).second;
    lastContainsEquality =
        (std::find_if(nodes.begin(), nodes.end(), [](aql::AstNode const* node) {
           return (node->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
                   node->type == aql::NODE_TYPE_OPERATOR_BINARY_IN);
         }) != nodes.end());

    std::sort(nodes.begin(), nodes.end(),
              [](aql::AstNode const* lhs, aql::AstNode const* rhs) -> bool {
                return Index::sortWeight(lhs) < Index::sortWeight(rhs);
              });

    containers::HashSet<int> operatorsFound;
    for (auto const& it : nodes) {
      if (it->type == aql::NODE_TYPE_OPERATOR_BINARY_NE) {
        // ignore all != operators here
        continue;
      }

      aql::AstNodeType type = it->type;
      if (aql::Ast::IsReversibleOperator(type) &&
          it->getMember(1)->isAttributeAccessForVariable(reference, false)) {
        type = aql::Ast::ReverseOperator(type);
      }

      // do not let duplicate or related operators pass
      if (isDuplicateOperator(type, operatorsFound)) {
        continue;
      }

      TRI_ASSERT(it->type != aql::NODE_TYPE_OPERATOR_BINARY_NE);
      operatorsFound.emplace(static_cast<int>(type));
      children.emplace_back(it);
    }
  }

  // must edit in place, no access to AST; TODO change so we can replace with
  // copy
  TEMPORARILY_UNLOCK_NODE(node);
  node->clearMembers();

  for (auto& it : children) {
    TRI_ASSERT(it->type != aql::NODE_TYPE_OPERATOR_BINARY_NE);
    node->addMember(it);
  }

  return node;
}

bool SortedIndexAttributeMatcher::isDuplicateOperator(
    aql::AstNodeType type, containers::HashSet<int> const& operatorsFound) {
  if (operatorsFound.find(static_cast<int>(type)) != operatorsFound.end()) {
    // duplicate operator
    return true;
  }

  if (operatorsFound.find(static_cast<int>(
          aql::NODE_TYPE_OPERATOR_BINARY_EQ)) != operatorsFound.end() ||
      operatorsFound.find(static_cast<int>(
          aql::NODE_TYPE_OPERATOR_BINARY_IN)) != operatorsFound.end()) {
    return true;
  }

  bool duplicate = false;
  switch (type) {
    case aql::NODE_TYPE_OPERATOR_BINARY_LT:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_LE)) !=
                  operatorsFound.end();
      break;
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_LT)) !=
                  operatorsFound.end();
      break;
    case aql::NODE_TYPE_OPERATOR_BINARY_GT:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_GE)) !=
                  operatorsFound.end();
      break;
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_GT)) !=
                  operatorsFound.end();
      break;
    case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
                  operatorsFound.end();
      break;
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:
      duplicate = operatorsFound.find(
                      static_cast<int>(aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
                  operatorsFound.end();
      break;
    default: {
      // ignore
    }
  }

  return duplicate;
}
