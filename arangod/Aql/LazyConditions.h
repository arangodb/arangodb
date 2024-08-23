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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <vector>

namespace arangodb::aql {
class Ast;
struct AstNode;

/// @brief the parser
struct LazyCondition {
  // the original condition expression (always non-null)
  AstNode* condition;
  bool negated;
  // the variable node that is assigned the condition expression's result. this
  // is null initially, but can be populated on demand with a
  // NODE_TYPE_REFERENCE
  AstNode* variableNode = nullptr;
};

class LazyConditions {
 public:
  using Conditions = std::vector<LazyCondition>;

  LazyConditions(LazyConditions const&) = delete;
  LazyConditions& operator=(LazyConditions const&) = delete;

  explicit LazyConditions(Ast& ast);

  /// @brief push a condition onto the stack
  void push(AstNode* condition, bool negated);

  /// @brief pop a condition from the stack
  LazyCondition pop();

  Conditions const& peek() const;
  Conditions& peek();

  bool forceInline() const noexcept;
  void pushForceInline() noexcept;
  void popForceInline() noexcept;

  void flushAssignments();
  void flushFilters() const;

 private:
  /// @brief the underlying AST
  Ast& _ast;

  /// @brief the stack of lazy conditions
  Conditions _stack;

  /// @brief whether or not conditional operators must be inlined (value > 0).
  size_t _forceInlineRequests;
};

}  // namespace arangodb::aql
