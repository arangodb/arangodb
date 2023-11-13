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
////////////////////////////////////////////////////////////////////////////////
#include "ExpressionMatcher.h"

#include <optional>
#include <variant>

#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/EnumeratePathsNode.h"
#include "Aql/Ast.h"
#include "Aql/Quantifier.h"

#include "Basics/ErrorT.h"
#include "Basics/overload.h"

using EN = arangodb::aql::ExecutionNode;

namespace arangodb::aql::expression_matcher {
auto matchValue(AstNode const* node) -> std::optional<AstNode const*> {
  if (node->type == NODE_TYPE_VALUE) {
    return node;
  }
  return std::nullopt;
}

auto matchNodeType(AstNode const* node, AstNodeType type)
    -> std::optional<AstNode const*> {
  if (node->type == type) {
    return node;
  }
  return std::nullopt;
}

auto matchValueNode(AstNode const* node) -> std::optional<AstNode const*> {
  return matchNodeType(node, NODE_TYPE_VALUE);
}

auto matchValueNode(AstNode const* node, AstNodeValueType valueType)
    -> std::optional<AstNode const*> {
  auto m = matchNodeType(node, NODE_TYPE_VALUE);

  if (!m.has_value()) {
    return std::nullopt;
  }

  if (!node->isValueType(valueType)) {
    return std::nullopt;
  }

  return node;
}

auto matchValueNode(AstNode const* node, AstNodeValueType valueType, bool v)
    -> std::optional<AstNode const*> {
  auto m = matchValueNode(node, valueType);

  if (!m.has_value()) {
    return std::nullopt;
  }

  if (node->getBoolValue() != v) {
    return std::nullopt;
  }

  return node;
}

auto matchValueNode(AstNode const* node, AstNodeValueType valueType, int v)
    -> std::optional<AstNode const*> {
  auto m = matchValueNode(node, valueType);

  if (!m.has_value()) {
    return std::nullopt;
  }

  if (node->getIntValue() != v) {
    return std::nullopt;
  }

  return node;
}

#if 0

auto matchSupportedConditions(Variable const* pathVariable, VarSet variables,
                              std::unique_ptr<Condition>& condition)
    -> std::vector<expression_matcher::MatchResult> {
  // condition is put in DNF
  condition->normalize();
  // condition is of the form
  // (cond11 AND cond12 AND ... AND cond 1n)
  //  OR (cond21 AND cond22 AND ... AND cond2n)
  //  OR ...
  //  OR (condm1 AND condm2 AND ... AND cond mn)
  //
  // where none of the condij contain an AND or an OR

  auto orNode = condition->root();
  TRI_ASSERT(orNode->type == NODE_TYPE_OPERATOR_NARY_OR);

  if (orNode->numMembers() != 1) {
    // Multiple OR statements.
    // => No optimization
    return {};
  }

  auto andNode = orNode->getMemberUnchecked(0);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  auto result = std::vector<expression_matcher::MatchResult>{};
  for (auto i = andNode->numMembers(); i > 0; --i) {
    auto cond = andNode->getMemberUnchecked(i - 1);

    auto varsReferencedByCondition = VarSet{};
    Ast::getReferencedVariables(cond, varsReferencedByCondition);

    auto match = expression_matcher::matchArrayEqualsAll(cond, pathVariable);

    if (match) {
      result.emplace_back(*match);
    }
  }

  return result;
}

auto matchAttributeAccess(AstNode const* node, Variable const* variable,
                          std::string attribute)
    -> std::variant<MatchError, std::monostate> {
  if (node->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
    return MatchError{
        .errorstack = {
            fmt::format("attribute access {} is not NODE_TYPE_ATTRIBUTE_ACCESS",
                        node->getTypeString())}};
  }
  if (not node->stringEquals(attribute)) {
    return MatchError{
        .errorstack = {fmt::format("found attribute {}, expected {}",
                                   node->getStringValue(), attribute)}};
  }
  auto variableRef = node->getMemberUnchecked(0);
  auto referencedVariable = static_cast<Variable*>(variableRef->getData());

  if (referencedVariable->id != variable->id) {
    return MatchError{
        .errorstack = {fmt::format("found variable {}, expected {}",
                                   referencedVariable->id, variable->id)}};
  }

  return std::monostate{};
}

auto matchIterator(AstNode const* node, Variable const* pathVariable)
    -> std::variant<MatchError, std::monostate> {
  if (node->type != NODE_TYPE_ITERATOR) {
    return MatchError{.errorstack = {fmt::format("{} is not NODE_TYPE_ITERATOR",
                                                 node->getTypeString())}};
  }

  //  auto variable = node->getMemberUnchecked(0);
  auto iteratee = node->getMemberUnchecked(1);
  auto matchIteratee = matchAttributeAccess(iteratee, pathVariable, "vertices");
  return std::visit(
      overload{
          [](MatchError e) -> std::variant<MatchError, std::monostate> {
            auto stack = e.errorstack;
            stack.push_back("matchIterator, iteratee");

            return MatchError{.errorstack = stack};
          },
          [](std::monostate r) -> std::variant<MatchError, std::monostate> {
            return std::monostate{};
          }},
      matchIteratee);
}

auto matchExpansionWithReturn(AstNode const* node, Variable const* pathVariable)
    -> std::variant<MatchError, std::monostate> {
  if (node->type != NODE_TYPE_EXPANSION) {
    return MatchError{
        .errorstack = {fmt::format("{} is not NODE_TYPE_EXPANSION",
                                   node->getTypeString())}};
  }

  auto filter = node->getMemberUnchecked(2);
  if (filter->type != NODE_TYPE_NOP) {
    return MatchError{.errorstack = {"expect filter to be NODE_TYPE_NOP"}};
  }

  auto limit = node->getMemberUnchecked(3);
  if (limit->type != NODE_TYPE_NOP) {
    return MatchError{.errorstack = {"expect limit to be NODE_TYPE_NOP"}};
  }

  auto iterator = node->getMemberUnchecked(0);
  auto iteratorMatch = matchIterator(iterator, pathVariable);

  //    auto reference = node->getMemberUnchecked(1);
  auto map = node->getMemberUnchecked(4);

  if (filter->type == NODE_TYPE_NOP and limit->type == NODE_TYPE_NOP) {
    if (iteratorMatch.has_value()) {
      if (map->type != NODE_TYPE_NOP) {
        return map;
      }
    }
  }
}

auto matchArrayEqualsAll(AstNode const* node, Variable const* pathVariable)
    -> std::optional<MatchResult> {
  if (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
    LOG_TRACE_OPTIMIZER("abcde")
        << "does not match NODE_TYPE_OPERATOR_ARRAY_EQ";
    return std::nullopt;
  }

  auto quantifier = node->getMemberUnchecked(2);
  if (not Quantifier::isAll(quantifier)) {
    LOG_TRACE_OPTIMIZER("abcde") << "quantifier is not ALL";
    return std::nullopt;
  }

  auto lhs = node->getMemberUnchecked(0);
  auto lhsMatch = matchExpansionWithReturn(lhs, pathVariable);
  if (not lhsMatch) {
    LOG_TRACE_OPTIMIZER("abcde") << "lhs does not match expansion with return";
    return std::nullopt;
  }

  auto rhs = node->getMemberUnchecked(1);
  auto rhsMatch = matchValue(rhs);
  if (not rhsMatch) {
    LOG_TRACE_OPTIMIZER("abcde") << "rhs does not match value";
    return std::nullopt;
  }

  return MatchResult{.closure = *lhsMatch, .value = *rhsMatch};
}
#endif
}  // namespace arangodb::aql::expression_matcher
