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
#include <string>

namespace arangodb::bitmap_index {

std::size_t TermExpr::numTerms() const {
  if (type == Type::Term) {
    return 1;
  }
  std::size_t n = 0;
  for (auto const& child : children) {
    n += child.numTerms();
  }
  return n;
}

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

bool isConstantScalar(aql::AstNode const* node) {
  return node->isConstant() && !node->isArray() && !node->isObject();
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

TermExpr makeTerm(aql::AstNode const* value) {
  return TermExpr{.type = TermExpr::Type::Term, .value = value};
}

TermExpr makeConstant(bool value) {
  return TermExpr{.type = value ? TermExpr::Type::True : TermExpr::Type::False};
}

/// combine children, collapsing a single child without exclusions. Without
/// children an And is true and an Or is false.
TermExpr combine(TermExpr::Type type, std::vector<TermExpr> children,
                 std::vector<TermExpr> excluded = {}) {
  if (children.empty() && excluded.empty()) {
    return makeConstant(type == TermExpr::Type::And);
  }
  if (children.size() == 1 && excluded.empty()) {
    return std::move(children.front());
  }
  return TermExpr{.type = type,
                  .children = std::move(children),
                  .excluded = std::move(excluded)};
}

/// a condition on the array attribute: documents matching `expr`, or, if
/// `negated`, all documents except those
struct Predicate {
  TermExpr expr;
  bool negated = false;
};

class PredicateMatcher {
 public:
  PredicateMatcher(Index const& index, aql::Variable const* reference,
                   bool runtime)
      : _field(index.fields().back()),
        _reference(reference),
        _runtime(runtime) {}

  std::optional<Predicate> match(aql::AstNode const* node) const {
    switch (node->type) {
      case aql::NODE_TYPE_OPERATOR_BINARY_IN:
      case aql::NODE_TYPE_OPERATOR_BINARY_NIN: {
        // `x IN doc.attr`, `x NOT IN doc.attr`
        aql::AstNode const* lhs = node->getMemberUnchecked(0);
        if (!onField(node) || referencesVariable(lhs, _reference)) {
          return std::nullopt;
        }
        bool const negated = node->type == aql::NODE_TYPE_OPERATOR_BINARY_NIN;
        if (!lhs->isConstant() || isConstantScalar(lhs)) {
          // a value only known at runtime is matched again then
          return Predicate{makeTerm(lhs), negated};
        }
        if (!_runtime) {
          return std::nullopt;
        }
        return Predicate{TermExpr{.type = TermExpr::Type::Unknown}, negated};
      }
      case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
      case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
        return matchArrayComparison(node);
      case aql::NODE_TYPE_OPERATOR_UNARY_NOT: {
        auto inner = match(node->getMemberUnchecked(0));
        if (inner.has_value()) {
          inner->negated = !inner->negated;
        }
        return inner;
      }
      case aql::NODE_TYPE_OPERATOR_BINARY_AND:
      case aql::NODE_TYPE_OPERATOR_NARY_AND:
        return matchAnd(node);
      case aql::NODE_TYPE_OPERATOR_BINARY_OR:
      case aql::NODE_TYPE_OPERATOR_NARY_OR:
        return matchOr(node);
      default:
        return std::nullopt;
    }
  }

 private:
  bool onField(aql::AstNode const* op) const {
    return isFieldAccess(op->getMemberUnchecked(1), _reference, _field);
  }

  std::optional<Predicate> matchArrayComparison(aql::AstNode const* op) const {
    TRI_ASSERT(op->numMembers() == 3);
    aql::AstNode const* lhs = op->getMemberUnchecked(0);
    aql::AstNode const* quantifier = op->getMemberUnchecked(2);
    if (!onField(op) || aql::Quantifier::isAtLeast(quantifier) ||
        referencesVariable(lhs, _reference)) {
      return std::nullopt;
    }
    std::vector<TermExpr> terms;
    if (!lhs->isConstant()) {
      // the searched array is only known at runtime and matched again then
      if (_runtime) {
        return std::nullopt;
      }
      terms.push_back(makeTerm(lhs));
    } else if (!lhs->isArray()) {
      // a left side that is no array makes the comparison false
      if (!_runtime) {
        return std::nullopt;
      }
      return Predicate{makeConstant(false), false};
    } else {
      if (lhs->numMembers() == 0 && !_runtime) {
        // a constant empty array makes the quantifier true or false
        // regardless of the document, the index is of no use then
        return std::nullopt;
      }
      for (std::size_t i = 0; i < lhs->numMembers(); ++i) {
        auto const* element = lhs->getMemberUnchecked(i);
        if (element->isArray() || element->isObject()) {
          // only scalar array elements are indexed
          if (!_runtime) {
            return std::nullopt;
          }
          terms.push_back(TermExpr{.type = TermExpr::Type::Unknown});
        } else {
          terms.push_back(makeTerm(element));
        }
      }
    }

    bool const notIn = op->type == aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN;
    if (aql::Quantifier::isAll(quantifier)) {
      // ALL IN: all present; ALL NOT IN: none present
      return notIn ? Predicate{combine(TermExpr::Type::Or, std::move(terms)),
                               true}
                   : Predicate{combine(TermExpr::Type::And, std::move(terms)),
                               false};
    }
    if (aql::Quantifier::isAny(quantifier)) {
      // ANY IN: one present; ANY NOT IN: not all present
      return notIn ? Predicate{combine(TermExpr::Type::And, std::move(terms)),
                               true}
                   : Predicate{combine(TermExpr::Type::Or, std::move(terms)),
                               false};
    }
    TRI_ASSERT(aql::Quantifier::isNone(quantifier));
    // NONE IN: none present; NONE NOT IN: all present
    return notIn ? Predicate{combine(TermExpr::Type::And, std::move(terms)),
                             false}
                 : Predicate{combine(TermExpr::Type::Or, std::move(terms)),
                             true};
  }

  std::optional<Predicate> matchAnd(aql::AstNode const* node) const {
    std::vector<TermExpr> required;
    std::vector<TermExpr> excluded;
    for (std::size_t i = 0; i < node->numMembers(); ++i) {
      auto child = match(node->getMemberUnchecked(i));
      if (!child.has_value()) {
        return std::nullopt;
      }
      (child->negated ? excluded : required).push_back(std::move(child->expr));
    }
    if (required.empty()) {
      // NOT a AND NOT b == NOT (a OR b)
      return Predicate{combine(TermExpr::Type::Or, std::move(excluded)), true};
    }
    return Predicate{combine(TermExpr::Type::And, std::move(required),
                             std::move(excluded)),
                     false};
  }

  std::optional<Predicate> matchOr(aql::AstNode const* node) const {
    std::vector<TermExpr> children;
    std::optional<bool> negated;
    for (std::size_t i = 0; i < node->numMembers(); ++i) {
      auto child = match(node->getMemberUnchecked(i));
      if (!child.has_value() ||
          (negated.has_value() && *negated != child->negated)) {
        // a mix of required and excluded elements would need the set of all
        // documents
        return std::nullopt;
      }
      negated = child->negated;
      children.push_back(std::move(child->expr));
    }
    if (!negated.has_value()) {
      return std::nullopt;
    }
    if (*negated) {
      // NOT a OR NOT b == NOT (a AND b)
      return Predicate{combine(TermExpr::Type::And, std::move(children)),
                       true};
    }
    return Predicate{combine(TermExpr::Type::Or, std::move(children)), false};
  }

  std::vector<basics::AttributeName> const& _field;
  aql::Variable const* _reference;
  bool const _runtime;
};

/// whether a condition on the array attribute searches values only known at
/// runtime
bool hasRuntimeValues(aql::AstNode const* node) {
  switch (node->type) {
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_NIN:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      return !node->getMemberUnchecked(0)->isConstant();
    default:
      break;
  }
  for (std::size_t i = 0; i < node->numMembers(); ++i) {
    if (hasRuntimeValues(node->getMemberUnchecked(i))) {
      return true;
    }
  }
  return false;
}

double estimateItems(TermExpr const& expr, double items) {
  switch (expr.type) {
    case TermExpr::Type::Term:
    case TermExpr::Type::Unknown:
      return items / 10.0;
    case TermExpr::Type::True:
      return items;
    case TermExpr::Type::False:
      return 0.0;
    case TermExpr::Type::And: {
      double result = items;
      for (auto const& child : expr.children) {
        result = std::min(result, estimateItems(child, items));
      }
      // each further required child narrows the result
      return result /
             std::pow(2.0, static_cast<double>(expr.children.size() - 1));
    }
    case TermExpr::Type::Or: {
      double result = 0.0;
      for (auto const& child : expr.children) {
        result += estimateItems(child, items);
      }
      return std::min(result, items);
    }
  }
  return items;
}

}  // namespace

std::optional<ConditionParts> matchCondition(Index const& index,
                                             aql::AstNode const* andNode,
                                             aql::Variable const* reference,
                                             bool runtime) {
  auto const& fields = index.fields();
  if (fields.empty() || andNode == nullptr ||
      andNode->type != aql::NODE_TYPE_OPERATOR_NARY_AND) {
    return std::nullopt;
  }
  std::size_t const numPrefix = fields.size() - 1;
  PredicateMatcher matcher(index, reference, runtime);

  ConditionParts parts;
  parts.prefixValues.resize(numPrefix, nullptr);

  for (std::size_t m = 0; m < andNode->numMembers(); ++m) {
    aql::AstNode const* op = andNode->getMemberUnchecked(m);
    if (op->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      aql::AstNode const* value = nullptr;
      int pos = matchPrefixEquality(index, op, reference, value);
      if (pos >= 0 && parts.prefixValues[pos] == nullptr) {
        parts.prefixValues[pos] = value;
        parts.members.push_back(m);
      }
      continue;
    }
    if (auto predicate = matcher.match(op); predicate.has_value()) {
      (predicate->negated ? parts.expr.excluded : parts.expr.children)
          .push_back(std::move(predicate->expr));
      parts.members.push_back(m);
    }
  }

  // documents without the array attribute are not indexed, so at least one
  // element must be required
  if (parts.expr.children.empty() ||
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

  // without statistics, assume each equality prefix and each required
  // element narrows the result considerably
  double items = estimateItems(parts->expr, static_cast<double>(itemsInIndex));
  for (std::size_t i = 0; i < parts->prefixValues.size(); ++i) {
    items /= 50.0;
  }

  Index::FilterCosts costs;
  costs.supportsCondition = true;
  costs.coveredAttributes = index.fields().size();
  costs.estimatedItems =
      std::max<std::size_t>(1, static_cast<std::size_t>(items));
  // one seek per searched element; bitmaps answer the condition without
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

containers::HashSet<std::size_t> coveredMembers(
    Index const& index, aql::AstNode const* andNode,
    aql::AstNode const* indexCondition, aql::Variable const* reference) {
  containers::HashSet<std::size_t> result;
  if (andNode == nullptr ||
      andNode->type != aql::NODE_TYPE_OPERATOR_NARY_AND) {
    return result;
  }
  auto parts = matchCondition(index, indexCondition, reference);
  if (!parts.has_value()) {
    return result;
  }

  // conditions on the array attribute with constant searched values are
  // answered exactly by the bitmaps
  PredicateMatcher matcher(index, reference, /*runtime*/ false);
  auto isAnswered = [&](aql::AstNode const* member) {
    return !hasRuntimeValues(member) && matcher.match(member).has_value();
  };
  containers::HashSet<std::string> answered;
  for (std::size_t m : parts->members) {
    auto const* member = indexCondition->getMemberUnchecked(m);
    if (isAnswered(member)) {
      answered.emplace(member->toString());
    }
  }
  for (std::size_t i = 0; i < andNode->numMembers(); ++i) {
    auto const* member = andNode->getMemberUnchecked(i);
    if (isAnswered(member) && answered.contains(member->toString())) {
      result.emplace(i);
    }
  }
  return result;
}

containers::HashSet<std::size_t> runtimeMembers(
    Index const& index, aql::AstNode const* andNode,
    aql::Variable const* reference) {
  containers::HashSet<std::size_t> result;
  if (andNode == nullptr ||
      andNode->type != aql::NODE_TYPE_OPERATOR_NARY_AND) {
    return result;
  }
  PredicateMatcher matcher(index, reference, /*runtime*/ false);
  for (std::size_t i = 0; i < andNode->numMembers(); ++i) {
    auto const* member = andNode->getMemberUnchecked(i);
    if (hasRuntimeValues(member) && matcher.match(member).has_value()) {
      result.emplace(i);
    }
  }
  return result;
}

}  // namespace arangodb::bitmap_index
