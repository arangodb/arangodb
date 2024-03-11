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

  size_t i = 0;
  for (auto& it : _stack) {
    TRI_ASSERT(!forceInline());

    if (it.variableNode == nullptr) {
      std::string variableName = _ast.variables()->nextName();

      AstNode* condition = it.condition;

      if (i > 0) {
        // if we have more than a single condition, we need to pick them all up
        // and build a leftmost prefix for our LET expression. this is necessary
        // so that we only execute the LET expression in case all conditions
        // along the path are truthy.
        AstNode* path = nullptr;
        for (size_t j = 0; j < i; ++j) {
          AstNode* pathCondition = _stack[j].condition;
          if (_stack[j].negated) {
            pathCondition = _ast.createNodeUnaryOperator(
                NODE_TYPE_OPERATOR_UNARY_NOT, pathCondition);
          }
          if (path == nullptr) {
            // first path element
            TRI_ASSERT(j == 0);
            path = pathCondition;
          } else {
            // AND-combine with previous path elements
            path = _ast.createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 path, pathCondition);
          }
        }
        TRI_ASSERT(path != nullptr);
        // make our conditions a leftmost prefix of the actual condition
        condition = _ast.createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                  path, condition);
      }

      if (condition->type == NODE_TYPE_REFERENCE) {
        // coalesce a reference to a reference into a simpple reference
        it.variableNode = condition;
      } else {
        // emit a LET node with the conditional expression.
        // we will refer to this let later from FILTER nodes, the ternary
        // operator or the logical operators
        AstNode* letNode = _ast.createNodeLet(
            variableName.data(), variableName.size(), condition, false);
        _ast.addOperation(letNode);

        TRI_ASSERT(letNode->type == NODE_TYPE_LET);
        Variable const* variable =
            static_cast<Variable const*>(letNode->getMember(0)->getData());

        it.variableNode = _ast.createNodeReference(variable);
        TRI_ASSERT(it.variableNode->type == NODE_TYPE_REFERENCE);

        // replace our condition with a reference to the variable that we have
        // assigned the condition result too. this is necessary to avoid
        // repeated evaluations of the condition. for example, in the expression
        //   RAND() > 0.5 ? (sub1) : (sub2)
        // we don't want to execute the condition  RAND() > 0.5  multiple times,
        // but only once. so we will execute the entire thing as
        //   LET tmp1 = RAND() > 0.5
        //   LET tmp2 = (
        //     FILTER tmp1
        //     sub1
        //   )
        //   LET tmp3 = (
        //     FILTER !tmp1
        //     sub2
        //   )
        //   RETURN tmp1 ? tmp2 : tmp3
        //
        // if we wouldn't do the replacement here, we would end up with
        //   LET tmp1 = RAND() > 0.5
        //   LET tmp2 = (
        //     FILTER RAND() > 0.5
        //     sub1
        //   )
        //   LET tmp3 = (
        //     FILTER !(RAND() > 0.5)
        //     sub2
        //   )
        //   RETURN RAND() > 0.5 ? tmp2 : tmp3
        // which would execute the condition 3 times. this is not only
        // inefficient, but also wrong in this case, as RAND() is
        // non-deterministic and can produce a different result upon every
        // invocation.
        it.condition = it.variableNode;
      }
    }

    ++i;
  }
}

void LazyConditions::flushFilters() const {
  if (forceInline()) {
    return;
  }

  // build an AND-combined FILTER condition with all conditions that we have
  // accumulated
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
      // first filter condition
      filterCondition = condition;
    } else {
      // AND-combine with previous filter conditions
      filterCondition = _ast.createNodeBinaryOperator(
          NODE_TYPE_OPERATOR_BINARY_AND, filterCondition, condition);
    }
  }

  if (filterCondition != nullptr) {
    // emit final FILTER node
    AstNode* node = _ast.createNodeFilter(filterCondition);
    _ast.addOperation(node);
  }
}

}  // namespace arangodb::aql
