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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RemoveRedundantOr.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

struct CommonNodeFinder {
  std::vector<AstNode const*> possibleNodes;

  bool find(AstNode const* node, AstNodeType condition,
            AstNode const*& commonNode, std::string& commonName) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      return (find(orOp.getLeft(), condition, commonNode, commonName) &&
              find(orOp.getRight(), condition, commonNode, commonName));
    }

    if (node->type == NODE_TYPE_VALUE) {
      possibleNodes.clear();
      return true;
    }

    if (node->type == condition ||
        (condition != NODE_TYPE_OPERATOR_BINARY_EQ &&
         (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_IN))) {
      ast::BinaryOperatorNode binOp(node);
      auto lhs = binOp.getLeft();
      auto rhs = binOp.getRight();

      bool const isIn =
          (node->type == NODE_TYPE_OPERATOR_BINARY_IN && rhs->isArray());

      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN &&
          rhs->type == NODE_TYPE_EXPANSION) {
        possibleNodes.clear();
        return false;
      }

      if (!isIn && lhs->isConstant()) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->isConstant()) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->type == NODE_TYPE_FCALL || rhs->type == NODE_TYPE_FCALL_USER ||
          rhs->type == NODE_TYPE_REFERENCE) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn &&
          (lhs->type == NODE_TYPE_FCALL || lhs->type == NODE_TYPE_FCALL_USER ||
           lhs->type == NODE_TYPE_REFERENCE)) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                    lhs->type == NODE_TYPE_INDEXED_ACCESS)) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (lhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
        } else {
          possibleNodes.emplace_back(lhs);
        }
      }
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (rhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          return false;
        } else {
          possibleNodes.emplace_back(rhs);
          return true;
        }
      }
    }
    possibleNodes.clear();
    return false;
  }
};

struct RemoveRedundantOrHelper {
  AstNode const* bestValue = nullptr;
  AstNodeType comparison;
  bool inclusive;
  bool isComparisonSet = false;
  CommonNodeFinder finder;
  AstNode const* commonNode = nullptr;
  std::string commonName;

  bool hasRedundantCondition(AstNode const* node) {
    try {
      if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_LT, commonNode,
                      commonName)) {
        return hasRedundantConditionWalker(node);
      }
    } catch (...) {
    }
    return false;
  }

  AstNode* createReplacementNode(Ast* ast) {
    TRI_ASSERT(commonNode != nullptr);
    TRI_ASSERT(bestValue != nullptr);
    TRI_ASSERT(isComparisonSet == true);
    return ast->createNodeBinaryOperator(comparison, commonNode->clone(ast),
                                         bestValue);
  }

 private:
  bool isInclusiveBound(AstNodeType type) {
    return (type == NODE_TYPE_OPERATOR_BINARY_GE ||
            type == NODE_TYPE_OPERATOR_BINARY_LE);
  }

  int isCompatibleBound(AstNodeType type, AstNode const*) {
    if ((comparison == NODE_TYPE_OPERATOR_BINARY_LE ||
         comparison == NODE_TYPE_OPERATOR_BINARY_LT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_LE ||
         type == NODE_TYPE_OPERATOR_BINARY_LT)) {
      return -1;
    } else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE ||
                comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
               (type == NODE_TYPE_OPERATOR_BINARY_GE ||
                type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1;
    }
    return 0;
  }

  bool compareBounds(AstNodeType type, AstNode const* value, int lowhigh) {
    int cmp = compareAstNodes(bestValue, value, true);

    if (cmp == 0 && (isInclusiveBound(comparison) != isInclusiveBound(type))) {
      return (isInclusiveBound(type) ? true : false);
    }
    return (cmp * lowhigh == 1);
  }

  bool hasRedundantConditionWalker(AstNode const* node) {
    AstNodeType type = node->type;

    if (type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      return (hasRedundantConditionWalker(orOp.getLeft()) &&
              hasRedundantConditionWalker(orOp.getRight()));
    }

    if (type == NODE_TYPE_OPERATOR_BINARY_LE ||
        type == NODE_TYPE_OPERATOR_BINARY_LT ||
        type == NODE_TYPE_OPERATOR_BINARY_GE ||
        type == NODE_TYPE_OPERATOR_BINARY_GT) {
      ast::RelationalOperatorNode relOp(node);
      auto lhs = relOp.getLeft();
      auto rhs = relOp.getRight();

      if (hasRedundantConditionWalker(rhs) &&
          !hasRedundantConditionWalker(lhs) && lhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = Ast::reverseOperator(type);
          bestValue = lhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(Ast::reverseOperator(type), lhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, lhs, lowhigh)) {
          comparison = Ast::reverseOperator(type);
          bestValue = lhs;
        }
        return true;
      }
      if (hasRedundantConditionWalker(lhs) &&
          !hasRedundantConditionWalker(rhs) && rhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = type;
          bestValue = rhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(type, rhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, rhs, lowhigh)) {
          comparison = type;
          bestValue = rhs;
        }
        return true;
      }
    } else if (type == NODE_TYPE_REFERENCE ||
               type == NODE_TYPE_ATTRIBUTE_ACCESS ||
               type == NODE_TYPE_INDEXED_ACCESS) {
      return (node->toString() == commonName);
    }

    return false;
  }
};

}  // namespace

void removeRedundantOrRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto cn = ExecutionNode::castTo<CalculationNode*>(dep);
    auto outVar = cn->outVariable();

    if (outVar != fn->inVariable()) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    RemoveRedundantOrHelper remover;
    if (remover.hasRedundantCondition(cn->expression()->node())) {
      auto astNode = remover.createReplacementNode(plan->getAst());

      auto expr = std::make_unique<Expression>(plan->getAst(), astNode);
      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
