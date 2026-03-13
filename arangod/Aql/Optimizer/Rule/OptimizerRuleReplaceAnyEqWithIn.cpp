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
/// @author Julia Puget
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
#include "Containers/SmallVector.h"

#include <memory>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

/// @brief rewrites ANY == array comparisons into IN expressions
///
/// Example transformation:
///   ['Alice', 'Bob', 'Carol'] ANY == p.name
/// into:
///   p.name IN ['Alice', 'Bob', 'Carol']
///
/// This enables index lookups and other optimizations that are not available
/// for ANY == expressions.
struct AnySimplifier {
  Ast* ast;

  explicit AnySimplifier(Ast* ast) : ast(ast) {}

  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
        node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      auto* lhs = node->getMember(0);
      auto* rhs = node->getMember(1);
      auto* lhsNew = simplify(lhs);
      auto* rhsNew = simplify(rhs);
      if (lhs != lhsNew || rhs != rhsNew) {
        return ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }
      return const_cast<AstNode*>(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_NARY_AND ||
        node->type == NODE_TYPE_OPERATOR_NARY_OR) {
      bool modified = false;
      containers::SmallVector<AstNode*, 8> newChildren;
      newChildren.reserve(node->numMembers());
      for (size_t i = 0; i < node->numMembers(); ++i) {
        auto* child = node->getMember(i);
        auto* newChild = simplify(child);
        newChildren.push_back(newChild);
        if (newChild != child) {
          modified = true;
        }
      }
      if (modified) {
        auto* result = ast->createNodeNaryOperator(node->type);
        for (auto* child : newChildren) {
          result->addMember(child);
        }
        return result;
      }
      return const_cast<AstNode*>(node);
    }

    return simplifyAnyEq(node);
  }

 private:
  /// @brief check if a node qualifies as a rewritable attribute/reference
  static bool qualifies(AstNode const* node) {
    return node != nullptr && !node->isConstant() &&
           (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
            node->type == NODE_TYPE_INDEXED_ACCESS ||
            node->type == NODE_TYPE_REFERENCE);
  }

  /// @brief rewrite an ANY == expression to IN, or return the node unchanged
  AstNode* simplifyAnyEq(AstNode const* node) const {
    if (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
      return const_cast<AstNode*>(node);
    }

    TRI_ASSERT(node->numMembers() == 3);

    // Member 2 is always the quantifier — must be ANY for this transformation
    auto* quantifierNode = node->getMember(2);
    if (quantifierNode == nullptr ||
        quantifierNode->type != NODE_TYPE_QUANTIFIER) {
      return const_cast<AstNode*>(node);
    }

    ast::QuantifierNode quantifier(quantifierNode);
    if (!quantifier.isAny()) {
      return const_cast<AstNode*>(node);
    }

    auto* lhs = node->getMember(0);
    auto* rhs = node->getMember(1);

    if (lhs == nullptr || rhs == nullptr) {
      return const_cast<AstNode*>(node);
    }

    AstNode* attr = nullptr;
    AstNode* array = nullptr;

    if (lhs->isArray() && lhs->isDeterministic() && qualifies(rhs)) {
      array = lhs;
      attr = rhs;
    } else if (rhs->isArray() && rhs->isDeterministic() && qualifies(lhs)) {
      array = rhs;
      attr = lhs;
    } else {
      return const_cast<AstNode*>(node);
    }

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr,
                                         array);
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
    auto* fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto* setter = plan->getVarSetBy(fn->inVariable()->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto* cn = ExecutionNode::castTo<CalculationNode*>(setter);
    auto* outVar = cn->outVariable();
    auto* root = cn->expression()->node();

    AnySimplifier simplifier(plan->getAst());
    auto* newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      auto expr = std::make_unique<Expression>(plan->getAst(), newRoot);
      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
