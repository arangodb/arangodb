////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRuleReplaceAnyEqWithIn.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Quantifier.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/ScopeGuard.h"
#include "Containers/SmallVector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

/// @brief common utilities for checking and stringifying AST nodes
struct SimplifierHelper {
  /// @brief convert AST node to its string representation
  static std::string stringifyNode(AstNode const* node) {
    if (node == nullptr) {
      return std::string();
    }
    try {
      return node->toString();
    } catch (...) {
      return std::string();
    }
  }

  /// @brief check if a node qualifies as an attribute/reference expression
  static bool qualifies(AstNode const* node, std::string& attributeName) {
    if (node == nullptr) {
      return false;
    }

    if (node->isConstant()) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
        node->type == NODE_TYPE_INDEXED_ACCESS ||
        node->type == NODE_TYPE_REFERENCE) {
      attributeName = stringifyNode(node);
      return true;
    }

    return false;
  }
};

/// @brief transforms ANY == array comparisons into IN expressions
///
/// Example transformation:
///   ['Alice', 'Bob', 'Carol'] ANY == p.name
/// into:
///   p.name IN ['Alice', 'Bob', 'Carol']
///
/// enables the optimizer to use index lookups and other optimizations
/// that are not yet available for ANY == expressions.
struct AnySimplifier {
  Ast& ast;
  ExecutionPlan& plan;

  explicit AnySimplifier(Ast& ast, ExecutionPlan& plan)
      : ast(ast), plan(plan) {}

  /// @brief safely convert an AST node to its string representation
  std::string stringifyNode(AstNode const* node) const {
    return SimplifierHelper::stringifyNode(node);
  }

  /// @brief check if a node qualifies as an attribute/reference expression
  bool qualifies(AstNode const* node, std::string& attributeName) const {
    return SimplifierHelper::qualifies(node, attributeName);
  }

  /// @brief extract attribute and array from an ANY == expression
  std::optional<std::pair<AstNode*, AstNode*>> extractAttributeAndArray(
      AstNode const* node) const {
    TRI_ASSERT(node->numMembers() == 3);

    // Member 2 is always the quantifier - must be ANY for this transformation
    auto quantifierNode = node->getMember(2);
    if (quantifierNode == nullptr ||
        quantifierNode->type != NODE_TYPE_QUANTIFIER) {
      return std::nullopt;
    }

    ast::QuantifierNode quantifier(quantifierNode);
    if (!quantifier.isAny()) {
      return std::nullopt;
    }

    // Members 0 and 1 are the left and right expressions
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);

    if (lhs == nullptr || rhs == nullptr) {
      return std::nullopt;
    }

    AstNode* attr = nullptr;
    AstNode* array = nullptr;
    std::string tmpName;

    // Check if lhs is the array and rhs is the attribute
    if (lhs->isArray() && lhs->isDeterministic() && qualifies(rhs, tmpName)) {
      array = lhs;
      attr = rhs;
    }
    // Check if rhs is the array and lhs is the attribute
    else if (rhs->isArray() && rhs->isDeterministic() &&
             qualifies(lhs, tmpName)) {
      array = rhs;
      attr = lhs;
    } else {
      return std::nullopt;
    }

    return std::make_pair(attr, array);
  }

  /// @brief rewrite an ANY == expression to an IN expression
  AstNode* simplifyAnyEq(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
      return const_cast<AstNode*>(node);
    }

    auto result = extractAttributeAndArray(node);
    if (!result.has_value()) {
      return const_cast<AstNode*>(node);
    }

    auto [attr, array] = result.value();

    // Build: attr IN array
    return ast.createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr,
                                        array);
  }

  /// @brief recursively simplify the expression tree (post-order traversal)
  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    // First, recurse into children (post-order traversal)
    size_t const numMembers = node->numMembers();
    bool childrenModified = false;
    containers::SmallVector<AstNode*, 8> newChildren;
    newChildren.reserve(numMembers);

    for (size_t i = 0; i < numMembers; ++i) {
      AstNode* child = node->getMember(i);
      AstNode* newChild = simplify(child);
      newChildren.push_back(newChild);
      if (newChild != child) {
        childrenModified = true;
      }
    }

    // If children were modified, create a new node with the modified children
    AstNode* result = const_cast<AstNode*>(node);
    if (childrenModified) {
      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        result = ast.createNodeBinaryOperator(node->type, newChildren[0],
                                              newChildren[1]);
      } else if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
        result = ast.createNodeNaryOperator(node->type);
        for (auto* child : newChildren) {
          result->addMember(child);
        }
      } else if (numMembers == 2 &&
                 (node->type == NODE_TYPE_OPERATOR_BINARY_OR ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_NE ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
                  node->type == NODE_TYPE_OPERATOR_BINARY_GE)) {
        result = ast.createNodeBinaryOperator(node->type, newChildren[0],
                                              newChildren[1]);
      } else if (node->type == NODE_TYPE_OPERATOR_NARY_OR) {
        result = ast.createNodeNaryOperator(node->type);
        for (auto* child : newChildren) {
          result->addMember(child);
        }
      } else {
        // For other node types, clone and replace children
        result = ast.shallowCopyForModify(node);
        auto sg =
            arangodb::scopeGuard([&]() noexcept { FINALIZE_SUBTREE(result); });
        for (size_t i = 0; i < numMembers; ++i) {
          result->changeMember(i, newChildren[i]);
        }
      }
    }

    // Now try to rewrite this node itself if it is ANY ==
    if (result->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
      return simplifyAnyEq(result);
    }

    return result;
  }
};

}  // namespace

void arangodb::aql::replaceAnyEqWithInRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
    auto outVar = cn->outVariable();

    auto root = cn->expression()->node();

    AnySimplifier simplifier(*plan->getAst(), *plan.get());
    auto newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      auto expr = std::make_unique<Expression>(plan->getAst(), newRoot);

      TRI_IF_FAILURE("OptimizerRules::replaceAnyEqWithInRuleOom") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);

      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
