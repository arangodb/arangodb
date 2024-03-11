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

#include "LazyConditions.h"

#include "Aql/Ast.h"
#include "Aql/Scopes.h"
#include "Aql/VariableGenerator.h"
#include "Basics/debugging.h"

#include <string>

namespace arangodb::aql {

LazyConditions::LazyConditions(Ast& ast) : _ast(ast), _forceInlineRequests(0) {}

/// @brief push a condition onto the stack
void LazyConditions::push(AstNode* condition, bool negated) {
  _stack.emplace_back(LazyCondition{
      .condition = condition, .negated = negated, .variableNode = nullptr});
}

/// @brief pop a condition from the stack
LazyCondition LazyConditions::pop() {
  TRI_ASSERT(!_stack.empty());
  // intentional copy
  LazyCondition result = _stack.back();
  _stack.pop_back();
  return result;
}

LazyConditions::Conditions const& LazyConditions::peek() const {
  return _stack;
}

LazyConditions::Conditions& LazyConditions::peek() { return _stack; }

bool LazyConditions::forceInline() const noexcept {
  return _forceInlineRequests > 0;
}

void LazyConditions::pushForceInline() noexcept { ++_forceInlineRequests; }

void LazyConditions::popForceInline() noexcept {
  TRI_ASSERT(_forceInlineRequests > 0);
  --_forceInlineRequests;
}

void LazyConditions::flushAssignments() {
  if (forceInline()) {
    return;
  }

  for (auto& it : _stack) {
    TRI_ASSERT(!forceInline());

    if (it.variableNode == nullptr) {
      std::string variableName = _ast.variables()->nextName();

      AstNode* condition = it.condition;
      if (condition->type == NODE_TYPE_REFERENCE) {
        // coalesce a reference to a reference into a simpple reference
        it.variableNode = condition;
      } else {
        AstNode* letNode = _ast.createNodeLet(
            variableName.data(), variableName.size(), condition, false);
        _ast.addOperation(letNode);

        TRI_ASSERT(letNode->type == NODE_TYPE_LET);
        Variable const* variable =
            static_cast<Variable const*>(letNode->getMember(0)->getData());

        it.variableNode = _ast.createNodeReference(variable);
        TRI_ASSERT(it.variableNode->type == NODE_TYPE_REFERENCE);

        it.condition = it.variableNode;
      }
    }
  }
}

void LazyConditions::flushFilters() const {
  if (forceInline()) {
    return;
  }

  AstNode* filterCondition = nullptr;
  for (auto const& it : _stack) {
    TRI_ASSERT(!forceInline());
    TRI_ASSERT(it.variableNode != nullptr);

    TRI_ASSERT(it.variableNode->type == NODE_TYPE_REFERENCE);

    AstNode* condition = it.variableNode;
    if (it.negated) {
      condition =
          _ast.createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, condition);
    }

    if (filterCondition == nullptr) {
      filterCondition = condition;
    } else {
      filterCondition = _ast.createNodeBinaryOperator(
          NODE_TYPE_OPERATOR_BINARY_AND, filterCondition, condition);
    }
  }

  if (filterCondition != nullptr) {
    auto node = _ast.createNodeFilter(filterCondition);
    _ast.addOperation(node);
  }
}

}  // namespace arangodb::aql
