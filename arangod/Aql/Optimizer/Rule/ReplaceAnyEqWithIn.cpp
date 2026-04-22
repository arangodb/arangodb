////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "ReplaceAnyEqWithIn.h"

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

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

/// @brief rewrites ANY == comparisons into IN expressions
///
/// Example:
///   ['Alice', 'Bob'] ANY == x.name  →  x.name IN ['Alice', 'Bob']
///
/// This enables index lookups that are unavailable for ANY == expressions.
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

    return simplifyAnyEq(node);
  }

 private:
  AstNode* simplifyAnyEq(AstNode const* node) const {
    if (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
      return const_cast<AstNode*>(node);
    }

    TRI_ASSERT(node->numMembers() == 3);

    auto* quantifierNode = node->getMember(2);
    if (quantifierNode == nullptr ||
        quantifierNode->type != NODE_TYPE_QUANTIFIER) {
      return const_cast<AstNode*>(node);
    }

    ast::QuantifierNode quantifier(quantifierNode);
    if (!quantifier.isAny()) {
      return const_cast<AstNode*>(node);
    }

    // grammar guarantees member(0) is array
    // `lhs ANY == rhs`  →  `rhs IN lhs`
    auto* lhs = node->getMember(0);
    auto* rhs = node->getMember(1);

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, rhs,
                                         lhs);
  }
};

}  // namespace

void replaceAnyEqWithInRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
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
    auto* root = cn->expression()->node();

    AnySimplifier simplifier(plan->getAst());
    auto* newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      auto* outVar = cn->outVariable();
      auto expr = std::make_unique<Expression>(plan->getAst(), newRoot);
      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
