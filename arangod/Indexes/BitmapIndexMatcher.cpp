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

#include "BitmapIndexMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Quantifier.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Containers/HashSet.h"

#include <algorithm>
#include <cmath>

namespace arangodb::bitmap_index {

namespace {

/// returns true if `node` is an attribute access on `reference` that is
/// identical to the given index field
bool isFieldAccess(aql::AstNode const* node, aql::Variable const* reference,
                   std::vector<basics::AttributeName> const& field) {
  std::pair<aql::Variable const*, std::vector<basics::AttributeName>> access;
  if (!node->isAttributeAccessForVariable(access, false) ||
      access.first != reference) {
    return false;
  }
  return basics::AttributeName::isIdentical(access.second, field, false);
}

bool referencesVariable(aql::AstNode const* node,
                        aql::Variable const* reference) {
  aql::VarSet vars;
  aql::Ast::getReferencedVariables(node, vars);
  return vars.contains(reference);
}

/// prefix field position matched by an equality, or -1
int matchPrefixEquality(Index const& index, aql::AstNode const* op,
                        aql::Variable const* reference,
                        aql::AstNode const*& value) {
  auto const& fields = index.fields();
  for (std::size_t side = 0; side < 2; ++side) {
    aql::AstNode const* access = op->getMemberUnchecked(side);
    aql::AstNode const* other = op->getMemberUnchecked(1 - side);
    if (referencesVariable(other, reference) ||
        (other->isConstant() && (other->isArray() || other->isObject()))) {
      continue;
    }
    for (std::size_t i = 0; i + 1 < fields.size(); ++i) {
      if (isFieldAccess(access, reference, fields[i])) {
        value = other;
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

}  // namespace

std::optional<ConditionParts> matchCondition(Index const& index,
                                             aql::AstNode const* andNode,
                                             aql::Variable const* reference) {
  auto const& fields = index.fields();
  if (fields.empty() || andNode == nullptr ||
      andNode->type != aql::NODE_TYPE_OPERATOR_NARY_AND) {
    return std::nullopt;
  }
  auto const& arrayField = fields.back();
  std::size_t const numPrefix = fields.size() - 1;

  ConditionParts parts;
  parts.prefixValues.resize(numPrefix, nullptr);

  for (std::size_t m = 0; m < andNode->numMembers(); ++m) {
    aql::AstNode const* op = andNode->getMemberUnchecked(m);
    switch (op->type) {
      case aql::NODE_TYPE_OPERATOR_BINARY_EQ: {
        aql::AstNode const* value = nullptr;
        int pos = matchPrefixEquality(index, op, reference, value);
        if (pos >= 0 && parts.prefixValues[pos] == nullptr) {
          parts.prefixValues[pos] = value;
          parts.members.push_back(m);
        }
        break;
      }
      case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN: {
        // `@labels ALL IN doc.attr`
        TRI_ASSERT(op->numMembers() == 3);
        aql::AstNode const* lhs = op->getMemberUnchecked(0);
        if (!aql::Quantifier::isAll(op->getMemberUnchecked(2)) ||
            !isFieldAccess(op->getMemberUnchecked(1), reference, arrayField) ||
            !lhs->isConstant() || !lhs->isArray() || lhs->numMembers() == 0) {
          // an empty array is ALL IN anything, including documents that are
          // not indexed, so it cannot be answered by the index
          break;
        }
        bool allScalar = true;
        for (std::size_t i = 0; i < lhs->numMembers(); ++i) {
          auto const* element = lhs->getMemberUnchecked(i);
          if (element->isArray() || element->isObject()) {
            // only scalar array elements are indexed
            allScalar = false;
            break;
          }
        }
        if (!allScalar) {
          break;
        }
        parts.termValues.push_back(lhs);
        parts.numTerms += lhs->numMembers();
        parts.members.push_back(m);
        break;
      }
      case aql::NODE_TYPE_OPERATOR_BINARY_IN: {
        // `"x" IN doc.attr`
        aql::AstNode const* lhs = op->getMemberUnchecked(0);
        if (!isFieldAccess(op->getMemberUnchecked(1), reference, arrayField) ||
            !lhs->isConstant() || lhs->isArray() || lhs->isObject()) {
          break;
        }
        parts.termValues.push_back(lhs);
        parts.numTerms += 1;
        parts.members.push_back(m);
        break;
      }
      default:
        break;
    }
  }

  if (parts.termValues.empty() ||
      std::any_of(parts.prefixValues.begin(), parts.prefixValues.end(),
                  [](auto const* v) { return v == nullptr; })) {
    return std::nullopt;
  }
  return parts;
}

Index::FilterCosts supportsFilterCondition(Index const& index,
                                           aql::AstNode const* node,
                                           aql::Variable const* reference,
                                           std::size_t itemsInIndex) {
  auto parts = matchCondition(index, node, reference);
  if (!parts.has_value()) {
    return Index::FilterCosts::defaultCosts(itemsInIndex);
  }

  // without statistics, assume each equality prefix and each searched
  // element narrows the result considerably
  double items = static_cast<double>(itemsInIndex);
  for (std::size_t i = 0; i < parts->prefixValues.size(); ++i) {
    items /= 50.0;
  }
  for (std::size_t i = 0; i < std::min<std::size_t>(parts->numTerms, 3); ++i) {
    items /= 10.0;
  }

  Index::FilterCosts costs;
  costs.supportsCondition = true;
  costs.coveredAttributes = index.fields().size();
  costs.estimatedItems = std::max<std::size_t>(1, static_cast<std::size_t>(items));
  // one seek per searched element; bitmaps answer the intersection without
  // touching documents, which makes the lookup cheaper than an equality
  // lookup that has to post-filter every edge
  costs.estimatedCosts =
      0.8 * std::max(1.0, std::log2(static_cast<double>(
                              std::max<std::size_t>(itemsInIndex, 2)))) +
      0.05 * static_cast<double>(costs.estimatedItems);
  return costs;
}

aql::AstNode* specializeCondition(Index const& index, aql::AstNode* node,
                                  aql::Variable const* reference) {
  auto parts = matchCondition(index, node, reference);
  TRI_ASSERT(parts.has_value());
  if (!parts.has_value()) {
    return node;
  }

  std::vector<aql::AstNode*> keep;
  keep.reserve(parts->members.size());
  for (std::size_t m : parts->members) {
    keep.push_back(node->getMemberUnchecked(m));
  }

  // must edit in place, no access to AST
  TEMPORARILY_UNLOCK_NODE(node);
  node->clearMembers();
  for (auto* member : keep) {
    node->addMember(member);
  }
  return node;
}

}  // namespace arangodb::bitmap_index
