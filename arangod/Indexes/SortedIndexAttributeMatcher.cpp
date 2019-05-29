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

#include "SortedIndexAttributeMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "VocBase/vocbase.h"

#include <velocypack/StringRef.h>

using namespace arangodb;

bool SortedIndexAttributeMatcher::accessFitsIndex(
    arangodb::Index const* idx,            // index
    arangodb::aql::AstNode const* access,  // attribute access
    arangodb::aql::AstNode const* other,   // eg const value
    arangodb::aql::AstNode const* op,  // binary operation that is parent of access and other
    arangodb::aql::Variable const* reference,  // variable used in access(es)
    std::unordered_map<size_t /*offset in idx->fields()*/, std::vector<arangodb::aql::AstNode const*> /*conjunct - operation*/>& found,  // marks operations covered by index-fields
    std::unordered_set<std::string>& nonNullAttributes,  // set of stringified op-childeren (access other) that may not be null
    bool isExecution  // skip usage check in execution phase
) {
  if (!idx->canUseConditionPart(access, other, op, reference, nonNullAttributes, isExecution)) {
    return false;
  }

  std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>> attributeData;
  bool const isPrimaryIndex = idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX;
      
  if (idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX && 
      (!other->isConstant() || !(other->isIntValue() || other->isDoubleValue()))) {
    // TTL index can only be used for numeric lookup values, no date strings or
    // anything else
    return false;
  }

  if (op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    if (!access->isAttributeAccessForVariable(attributeData) || attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }
    if (arangodb::basics::TRI_AttributeNamesHaveExpansion(attributeData.second)) {
      // doc.value[*] == 'value'
      return false;
    }
    if (idx->isAttributeExpanded(attributeData.second)) {
      // doc.value == 'value' (with an array index)
      return false;
    }
  } else {
    // ok, we do have an IN here... check if it's something like 'value' IN
    // doc.value[*]
    TRI_ASSERT(op->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);

    if (access->isAttributeAccessForVariable(attributeData) && attributeData.first == reference &&
        !arangodb::basics::TRI_AttributeNamesHaveExpansion(attributeData.second) &&
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
  }

  std::vector<arangodb::basics::AttributeName> const& fieldNames = attributeData.second;

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    if (idx->fields()[i].size() != fieldNames.size()) {
      // attribute path length differs
      continue;
    }

    if (idx->isAttributeExpanded(i) && op->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // If this attribute is correct or not, it could only serve for IN
      continue;
    }

    bool match = arangodb::basics::AttributeName::isIdentical(idx->fields()[i],
                                                              fieldNames, true);

    // make exception for primary index as we do not need to match "_key, _id"
    // but can go directly for "_id"
    if (!match && 
        isPrimaryIndex && 
        i == 0 &&
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
    arangodb::Index const* idx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>& found,
    size_t& values, std::unordered_set<std::string>& nonNullAttributes, bool isExecution) {
  // assert we have a proper formed conditiona - naray conjunction
  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  // inspect the the conjuncts - allowed are binary comparisons and a contains check
  for (size_t i = 0; i < node->numMembers(); ++i) {
    auto op = node->getMember(i);

    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        TRI_ASSERT(op->numMembers() == 2);
        accessFitsIndex(idx, op->getMember(0), op->getMember(1), op, reference,
                        found, nonNullAttributes, isExecution);
        accessFitsIndex(idx, op->getMember(1), op->getMember(0), op, reference,
                        found, nonNullAttributes, isExecution);
        break;

      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
        if (accessFitsIndex(idx, op->getMember(0), op->getMember(1), op,
                            reference, found, nonNullAttributes, isExecution)) {
          if (op->getMember(1)->isAttributeAccessForVariable(reference, /*indexed access*/ false)) {
            // 'abc' IN doc.attr[*]
            ++values;
          } else {
            size_t av = SimpleAttributeEqualityMatcher::estimateNumberOfArrayMembers(
                op->getMember(1));
            if (av > 1) {
              // attr IN [ a, b, c ]  =>  this will produce multiple items, so
              // count them!
              values += av - 1;
            }
          }
        }
        break;

      default:
        break;
    }
  }
}

bool SortedIndexAttributeMatcher::supportsFilterCondition(
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

  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  std::unordered_set<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(idx, node, reference, found, values, nonNullAttributes, false);

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

  if (attributesCoveredByEquality == idx->fields().size() &&
      (idx->unique() || idx->implicitlyUnique())) {
    // index is unique and condition covers all attributes by equality
    if (itemsInIndex == 0) {
      estimatedItems = 0;
      estimatedCost = 0.0;
      return true;
    }

    estimatedItems = values;
    // ALTERNATIVE: estimatedCost = static_cast<double>(estimatedItems * values);
    estimatedCost = (std::max)(static_cast<double>(1),
                               std::log2(static_cast<double>(itemsInIndex)) * values);

    // cost is already low... now slightly prioritize unique indexes
    estimatedCost *= 0.995 - 0.05 * (idx->fields().size() - 1);
    return true;
  }

  if (attributesCovered > 0 &&
      (!idx->sparse() || attributesCovered == idx->fields().size())) {
    // if the condition contains at least one index attribute and is not sparse,
    // or the index is sparse and all attributes are covered by the condition,
    // then it can be used (note: additional checks for condition parts in
    // sparse indexes are contained in Index::canUseConditionPart)
    estimatedItems = static_cast<size_t>(
        (std::max)(static_cast<size_t>(estimatedCost * values), static_cast<size_t>(1)));

    // check if the index has a selectivity estimate ready
    if (idx->hasSelectivityEstimate() &&
        attributesCoveredByEquality == idx->fields().size()) {
      double estimate = idx->selectivityEstimate();
      if (estimate > 0.0) {
        estimatedItems = static_cast<size_t>(1.0 / estimate);
      }
    } else if (attributesCoveredByEquality > 0) {
      TRI_ASSERT(attributesCovered > 0);
      // the index either does not have a selectivity estimate, or not all
      // of its attributes are covered by the condition using an equality lookup
      // however, if the search condition uses equality lookups on the prefix
      // of the index, then we can check if there is another index which is just
      // indexing the prefix, and "steal" the selectivity estimate from that
      // index for example, if the condition is "doc.a == 1 && doc.b > 2", and
      // the current index is created on ["a", "b"], then we will not use the
      // selectivity estimate of the current index (due to the range condition
      // used for the second index attribute). however, if there is another
      // index on just "a", we know that the current index is at least as
      // selective as the index on the single attribute. and that the extra
      // condition we have will make it even more selectivity. so in this case
      // we will re-use the selectivity estimate from the other index, and are
      // happy.
      for (auto const& otherIdx : allIndexes) {
        auto const* other = otherIdx.get();
        if (other == idx || !other->hasSelectivityEstimate()) {
          continue;
        }
        auto const& otherFields = other->fields();
        if (otherFields.size() >= attributesCovered) {
          // other index has more fields than we have, or the same amount.
          // then it will not be helpful
          continue;
        }
        size_t matches = 0;
        for (size_t i = 0; i < otherFields.size(); ++i) {
          if (otherFields[i] != idx->fields()[i]) {
            break;
          }
          ++matches;
        }
        if (matches == otherFields.size()) {
          double estimate = other->selectivityEstimate();
          if (estimate > 0.0) {
            // reuse the estimate from the other index
            estimatedItems = static_cast<size_t>(1.0 / estimate);
            break;
          }
        }
      }
    }

    if (itemsInIndex == 0) {
      estimatedCost = 0.0;
    } else {
      // lookup cost is O(log(n))
      estimatedCost = (std::max)(static_cast<double>(1),
                                 std::log2(static_cast<double>(itemsInIndex)) * values);
      // slightly prefer indexes that cover more attributes
      estimatedCost -= (attributesCovered - 1) * 0.02;
    }
    return true;
  }

  // index does not help for this condition
  estimatedItems = itemsInIndex;
  estimatedCost = static_cast<double>(estimatedItems);
  return false;
}

bool SortedIndexAttributeMatcher::supportsSortCondition(
    arangodb::Index const* idx, arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost, size_t& coveredAttributes) {
  TRI_ASSERT(sortCondition != nullptr);

  if (!idx->sparse() ||
      sortCondition->onlyUsesNonNullSortAttributes(idx->fields())) {
    // non-sparse indexes can be used for sorting, but sparse indexes can only be
    // used if we can prove that we only need to return non-null index attribute values
    if (!idx->hasExpansion() && sortCondition->isUnidirectional() &&
        sortCondition->isOnlyAttributeAccess()) {
      coveredAttributes = sortCondition->coveredAttributes(reference, idx->fields());

      if (coveredAttributes >= sortCondition->numAttributes()) {
        // sort is fully covered by index. no additional sort costs!
        // forward iteration does not have high costs
        estimatedCost = itemsInIndex * 0.001;
        if (idx->isPersistent() && sortCondition->isDescending()) {
          // reverse iteration has higher costs than forward iteration
          estimatedCost *= 4;
        }
        return true;
      } else if (coveredAttributes > 0) {
        estimatedCost = (itemsInIndex / coveredAttributes) *
                        std::log2(static_cast<double>(itemsInIndex));
        if (idx->isPersistent() && sortCondition->isDescending()) {
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
    if (idx->isPersistent()) {
      estimatedCost *= 1.05;
    }
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* SortedIndexAttributeMatcher::specializeCondition(
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

  std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>> found;
  std::unordered_set<std::string> nonNullAttributes;
  size_t values = 0;
  matchAttributes(idx, node, reference, found, values, nonNullAttributes, false);

  std::vector<arangodb::aql::AstNode const*> children;
  bool lastContainsEquality = true;

  for (size_t i = 0; i < idx->fields().size(); ++i) {
    auto it = found.find(i);

    if (it == found.end()) {
      // index attribute not covered by condition
      break;
    }

    if (!lastContainsEquality) {
      // unsupported condition. must abort
      break;
    }

    // check if the current condition contains an equality condition
    auto& nodes = (*it).second;
    lastContainsEquality =
        (std::find_if(nodes.begin(), nodes.end(), [](arangodb::aql::AstNode const* node) {
           return (node->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
                   node->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN);
         }) != nodes.end());

    std::sort(nodes.begin(), nodes.end(),
              [](arangodb::aql::AstNode const* lhs, arangodb::aql::AstNode const* rhs) -> bool {
                return Index::sortWeight(lhs) < Index::sortWeight(rhs);
              });

    arangodb::HashSet<int> operatorsFound;
    for (auto& it : nodes) {
      if (it->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE) {
        // ignore all != operators here
        continue;
      }

      // do not let duplicate or related operators pass
      if (isDuplicateOperator(it, operatorsFound)) {
        continue;
      }

      TRI_ASSERT(it->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE);
      operatorsFound.emplace(static_cast<int>(it->type));
      children.emplace_back(it);
    }
  }

  // must edit in place, no access to AST; TODO change so we can replace with
  // copy
  TEMPORARILY_UNLOCK_NODE(node);
  node->removeMembers();

  for (auto& it : children) {
    TRI_ASSERT(it->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE);
    node->addMember(it);
  }

  return node;
}

bool SortedIndexAttributeMatcher::isDuplicateOperator(arangodb::aql::AstNode const* node,
                                                      arangodb::HashSet<int> const& operatorsFound) {
  auto type = node->type;
  if (operatorsFound.find(static_cast<int>(type)) != operatorsFound.end()) {
    // duplicate operator
    return true;
  }

  if (operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
          operatorsFound.end() ||
      operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
          operatorsFound.end()) {
    return true;
  }

  bool duplicate = false;
  switch (type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) !=
                  operatorsFound.end();
      break;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
      duplicate = operatorsFound.find(static_cast<int>(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ)) !=
                  operatorsFound.end();
      break;
    default: {
      // ignore
    }
  }

  return duplicate;
}
