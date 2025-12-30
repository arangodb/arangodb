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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesConditionRewrite.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief auxilliary struct for finding common nodes in OR conditions
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
        // ooh, cannot optimize this (yet)
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
          // don't return, must consider the other side of the condition
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

/// @brief auxilliary struct for the OR-to-IN conversion
struct OrSimplifier {
  Ast* ast;
  ExecutionPlan* plan;

  OrSimplifier(Ast* ast, ExecutionPlan* plan) : ast(ast), plan(plan) {}

  std::string stringifyNode(AstNode const* node) const {
    try {
      return node->toString();
    } catch (...) {
    }
    return std::string();
  }

  bool qualifies(AstNode const* node, std::string& attributeName) const {
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

  bool detect(AstNode const* node, bool preferRight, std::string& attributeName,
              AstNode const*& attr, AstNode const*& value) const {
    attributeName.clear();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      ast::RelationalOperatorNode eqOp(node);
      auto lhs = eqOp.getLeft();
      auto rhs = eqOp.getRight();
      if (!preferRight && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }

      if (qualifies(rhs, attributeName)) {
        if (lhs->isDeterministic()) {
          attr = rhs;
          value = lhs;
          return true;
        }
      }
      // intentionally falls through
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      ast::RelationalOperatorNode inOp(node);
      auto lhs = inOp.getLeft();
      auto rhs = inOp.getRight();
      if (rhs->isArray() && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }
      // intentionally falls through
    }

    return false;
  }

  AstNode* buildValues(AstNode const* attr, AstNode const* lhs,
                       bool leftIsArray, AstNode const* rhs,
                       bool rightIsArray) const {
    auto values = ast->createNodeArray();
    if (leftIsArray) {
      size_t const n = lhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(lhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(lhs);
    }

    if (rightIsArray) {
      size_t const n = rhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(rhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(rhs);
    }

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr,
                                         values);
  }

  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      auto lhs = orOp.getLeft();
      auto rhs = orOp.getRight();

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // create a modified node
        node = ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      if ((lhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN) &&
          (rhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN)) {
        std::string leftName;
        std::string rightName;
        AstNode const* leftAttr = nullptr;
        AstNode const* rightAttr = nullptr;
        AstNode const* leftValue = nullptr;
        AstNode const* rightValue = nullptr;

        for (size_t i = 0; i < 4; ++i) {
          if (detect(lhsNew, i >= 2, leftName, leftAttr, leftValue) &&
              detect(rhsNew, i % 2 == 0, rightName, rightAttr, rightValue) &&
              leftName == rightName) {
            std::pair<Variable const*,
                      std::vector<arangodb::basics::AttributeName>>
                tmp1;

            if (leftValue->isAttributeAccessForVariable(tmp1)) {
              bool qualifies = false;
              auto setter = plan->getVarSetBy(tmp1.first->id);
              if (setter != nullptr &&
                  setter->getType() == EN::ENUMERATE_COLLECTION) {
                qualifies = true;
              }

              std::pair<Variable const*,
                        std::vector<arangodb::basics::AttributeName>>
                  tmp2;

              if (qualifies && rightValue->isAttributeAccessForVariable(tmp2)) {
                auto setter = plan->getVarSetBy(tmp2.first->id);
                if (setter != nullptr &&
                    setter->getType() == EN::ENUMERATE_COLLECTION) {
                  if (tmp1.first != tmp2.first || tmp1.second != tmp2.second) {
                    continue;
                  }
                }
              }
            }

            return buildValues(leftAttr, leftValue,
                               lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN,
                               rightValue,
                               rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN);
          }
        }
      }

      // return node as is
      return const_cast<AstNode*>(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      ast::LogicalOperatorNode andOp(node);
      auto lhs = andOp.getLeft();
      auto rhs = andOp.getRight();

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // return a modified node
        return ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      // intentionally falls through
    }

    return const_cast<AstNode*>(node);
  }
};

struct RemoveRedundantOr {
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
      // ignore errors and simply return false
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
      return -1;  // high bound
    } else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE ||
                comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
               (type == NODE_TYPE_OPERATOR_BINARY_GE ||
                type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1;  // low bound
    }
    return 0;  // incompatible bounds
  }

  // returns false if the existing value is better and true if the input value
  // is better
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
      // if hasRedundantConditionWalker(lhs) and
      // hasRedundantConditionWalker(rhs), then one of the conditions in the
      // OR statement is of the form x == x intentionally falls through if
    } else if (type == NODE_TYPE_REFERENCE ||
               type == NODE_TYPE_ATTRIBUTE_ACCESS ||
               type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons
      return (node->toString() == commonName);
    }

    return false;
  }
};

/// @brief simplify conditions in CalculationNodes
void arangodb::aql::simplifyConditionsRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  auto p = plan.get();
  bool changed = false;

  auto visitor = [&changed, p](AstNode* node) {
    again:
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        auto const* accessed = node->getMemberUnchecked(0);

        if (accessed->type == NODE_TYPE_REFERENCE) {
          Variable const* v = static_cast<Variable const*>(accessed->getData());
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        TRI_ASSERT(accessed != nullptr);

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName(node->getStringView());
          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              // found the attribute!
              ast::ObjectElementNode objElem(member);
              AstNode* next = objElem.getValue();
              if (!next->isDeterministic()) {
                // do not descend into non-deterministic nodes
                return node;
              }
              // descend further
              node = next;
              // now try optimizing the simplified condition
              // time for a goto...!
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              // dynamic attribute name
              isDynamic = true;
            }
          }

          // attribute not found
          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        ast::IndexedAccessNode indexAccess(node);
        auto const* accessed = indexAccess.getObject();

        if (accessed->type == NODE_TYPE_REFERENCE) {
          Variable const* v = static_cast<Variable const*>(accessed->getData());
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        auto indexValue = indexAccess.getIndex();

        if (!indexValue->isConstant() ||
            !(indexValue->isStringValue() || indexValue->isNumericValue())) {
          // cant handle this type of index statically
          return node;
        }

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName;
          std::string indexString;

          if (indexValue->isStringValue()) {
            // string index, e.g. ['123']
            attributeName = indexValue->getStringView();
          } else {
            // numeric index, e.g. [123]
            TRI_ASSERT(indexValue->isNumericValue());
            // convert the numeric index into a string
            indexString = std::to_string(indexValue->getIntValue());
            attributeName = std::string_view(indexString);
          }

          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              // found the attribute!
              ast::ObjectElementNode objElem2(member);
              AstNode* next = objElem2.getValue();
              if (!next->isDeterministic()) {
                // do not descend into non-deterministic nodes
                return node;
              }
              // descend further
              node = next;
              // now try optimizing the simplified condition
              // time for a goto...!
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              // dynamic attribute name
              isDynamic = true;
            }
          }

          // attribute not found
          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        } else if (accessed->type == NODE_TYPE_ARRAY) {
          int64_t position;
          if (indexValue->isStringValue()) {
            // string index, e.g. ['123'] -> convert to a numeric index
            bool valid;
            position = NumberUtils::atoi<int64_t>(
                indexValue->getStringValue(),
                indexValue->getStringValue() + indexValue->getStringLength(),
                valid);
            if (!valid) {
              // invalid index
              changed = true;
              return p->getAst()->createNodeValueNull();
            }
          } else {
            // numeric index, e.g. [123]
            TRI_ASSERT(indexValue->isNumericValue());
            position = indexValue->getIntValue();
          }
          int64_t const n = accessed->numMembers();
          if (position < 0) {
            // a negative position is allowed
            position = n + position;
          }
          if (position >= 0 && position < n) {
            ast::ArrayNode arr(accessed);
            AstNode* next = arr.getElements()[static_cast<size_t>(position)];
            if (!next->isDeterministic()) {
              // do not descend into non-deterministic nodes
              return node;
            }
            // descend further
            node = next;
            // now try optimizing the simplified condition
            // time for a goto...!
            goto again;
          }

          // index out of bounds
          changed = true;
          return p->getAst()->createNodeValueNull();
        }
      }

      return node;
  };

  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic() ||
        nn->outVariable()->type() == Variable::Type::Const) {
      // If this node is non-deterministic or has a constant expression, we must
      // not touch it!
      continue;
    }

    AstNode* root = nn->expression()->nodeForModification();

    if (root != nullptr) {
      // the changed variable is captured by reference by the lambda that
      // traverses the Ast and may modify it. if it performs a modification,
      // it will set changed=true
      changed = false;
      AstNode* simplified = plan->getAst()->traverseAndModify(root, visitor);
      if (simplified != root || changed) {
        nn->expression()->replaceNode(simplified);
        nn->expression()->invalidateAfterReplacements();
        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to
//  the same (single) attribute.
void arangodb::aql::replaceOrWithInRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
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

    auto root = cn->expression()->node();

    OrSimplifier simplifier(plan->getAst(), plan.get());
    auto newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      auto expr = std::make_unique<Expression>(plan->getAst(), newRoot);

      TRI_IF_FAILURE("OptimizerRules::replaceOrWithInRuleOom") {
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

void arangodb::aql::removeRedundantOrRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
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

    RemoveRedundantOr remover;
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