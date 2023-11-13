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
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <optional>

#include <Aql/AstNode.h>
#include <Aql/Condition.h>
#include <Aql/Variable.h>

namespace arangodb::aql::expression_matcher {

struct MatchResult {
  AstNode const* closure;
  AstNode const* value;
};

struct MatchError {
  explicit MatchError(std::string message) : stack{{message}} {}
  std::vector<std::string> stack;
};

auto matchNodeType(AstNode const* node, AstNodeType type)
    -> std::optional<AstNode const*>;

auto matchValueNode(AstNode const* node) -> std::optional<AstNode const*>;

auto matchValueNode(AstNode const* node, AstNodeValueType valueType)
    -> std::optional<AstNode const*>;

// Technically the AstNodeValueType is not necessary for this matcher
// Also this seems dangerous since C++ converts everything and their dog
// to an int.
auto matchValueNode(AstNode const* node, AstNodeValueType valueType, bool v)
    -> std::optional<AstNode const*>;

// Technically the AstNodeValueType is not necessary for this matcher
auto matchValueNode(AstNode const* node, AstNodeValueType valueType, int v)
    -> std::optional<AstNode const*>;

auto matchSupportedConditions(Variable const* pathVariable, VarSet variables,
                              std::unique_ptr<Condition>& condition)
    -> std::vector<MatchResult>;

auto matchAttributeAccess(AstNode const* node, Variable const* variable,
                          std::string attribute)
    -> std::variant<MatchError, std::monostate>;

auto matchIterator(AstNode const* node, Variable const* pathVariable)
    -> std::variant<MatchError, std::monostate>;

auto matchExpansionWithReturn(AstNode const* node, Variable const* pathVariable)
    -> std::variant<MatchError, std::monostate>;

auto matchArrayEqualsAll(AstNode const* node, Variable const* pathVariable)
    -> std::optional<MatchResult>;

}  // namespace arangodb::aql::expression_matcher
