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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Optimizer/Utils/ExtractNonConstPartsOfIndexCondition.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/types.h"
#include "Aql/RegisterPlan.h"
#include "Aql/VarInfoMap.h"
#include "Aql/Function.h"
#include "Indexes/Index.h"
#include "IResearch/IResearchFeature.h"

namespace arangodb::aql::optimizer {

namespace {

/**
 * @brief tests if the expression given by the AstNode
 * accesses the given variable.
 */
bool accessesVariable(AstNode const* node, Variable const* var) {
  if (node->isAttributeAccessForVariable(var, true)) {
    // If this node is the variable access return true
    return true;
  }

  for (size_t i = 0; i < node->numMembers(); i++) {
    // Recursivley test if one of our subtrees accesses the variable
    if (accessesVariable(node->getMemberUnchecked(i), var)) {
      // One of them is enough
      return true;
    }
  }

  return false;
}

bool accessesNonRegisterVariable(AstNode const* node) {
  auto varNode = node->getAttributeAccessForVariable(true);
  if (varNode) {
    auto var = static_cast<Variable const*>(varNode->getData());
    return var && !var->needsRegister();
  }
  for (size_t i = 0; i < node->numMembers(); i++) {
    // Recursivley test if one of our subtrees accesses the variable
    if (accessesNonRegisterVariable(node->getMemberUnchecked(i))) {
      // One of them is enough
      return true;
    }
  }
  return false;
}

AstNode* wrapInUniqueCall(Ast* ast, AstNode* node, bool sorted) {
  if (node->type != NODE_TYPE_ARRAY || node->numMembers() >= 2) {
    // an non-array or an array with more than 1 member
    auto array = ast->createNodeArray();
    array->addMember(node);

    // Here it does not matter which index we choose for the isSorted/isSparse
    // check, we need them all sorted here.

    if (sorted) {
      return ast->createNodeFunctionCall("SORTED_UNIQUE", array, true);
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall("UNIQUE", array, true);
  }

  // presumably an array with no or a single member
  return node;
}

/**
 * @brief Captures the given expression into the NonConstExpressionContainer for
 * later execution. Extracts all required variables and retains their registers,
 * s.t. all necessary pieces are stored in the container.
 */
void captureNonConstExpression(Ast* ast, VarInfoMap const& varInfo,
                               AstNode* expression,
                               std::vector<size_t> selectedMembersFromRoot,
                               NonConstExpressionContainer& result) {
  // all new AstNodes are registered with the Ast in the Query
  auto e = std::make_unique<Expression>(ast, expression);

  TRI_IF_FAILURE("IndexBlock::initialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  result._hasV8Expression |= e->willUseV8();

  VarSet innerVars;
  e->variables(innerVars);

  result._expressions.emplace_back(std::make_unique<NonConstExpression>(
      std::move(e), std::move(selectedMembersFromRoot)));

  for (auto const& v : innerVars) {
    auto it = varInfo.find(v->id);
    TRI_ASSERT(it != varInfo.cend());
    TRI_ASSERT(it->second.registerId.isValid());
    result._varToRegisterMapping.emplace_back(v->id, it->second.registerId);
  }

  TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

void extractNonConstPartsOfJunctionCondition(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* condition, Variable const* indexVariable,
    std::vector<size_t> const& selectedMembersFromRoot,
    NonConstExpressionContainer& result);

void captureFCallArgumentExpressions(
    Ast* ast, VarInfoMap const& varInfo, AstNode const* fCallExpression,
    std::vector<size_t> selectedMembersFromRoot, Variable const* indexVariable,
    NonConstExpressionContainer& result) {
  TRI_ASSERT(fCallExpression->type == NODE_TYPE_FCALL);
  TRI_ASSERT(1 == fCallExpression->numMembers());

  // We select the first member, so store it on our path
  selectedMembersFromRoot.emplace_back(0);
  AstNode* array = fCallExpression->getMemberUnchecked(0);

  for (size_t k = 0; k < array->numMembers(); k++) {
    AstNode* child = array->getMemberUnchecked(k);
    if (!child->isConstant() && !accessesVariable(child, indexVariable) &&
        !accessesNonRegisterVariable(child)) {
      std::vector<size_t> idx = selectedMembersFromRoot;
      idx.emplace_back(k);
      captureNonConstExpression(ast, varInfo, child, std::move(idx), result);
    }
  }
}

void captureArrayFilterArgumentExpressions(
    Ast* ast, VarInfoMap const& varInfo, AstNode const* filter,
    std::vector<size_t> const& selectedMembersFromRoot, bool evaluateFCalls,
    Variable const* indexVariable, NonConstExpressionContainer& result) {
  for (size_t i = 0, size = filter->numMembers(); i != size; ++i) {
    auto member = filter->getMemberUnchecked(i);
    if (!member->isConstant()) {
      auto path = selectedMembersFromRoot;
      path.emplace_back(i);
      if (member->type == NODE_TYPE_RANGE) {
        // intentionally copy path here as we will have many members
        auto path1 = path;
        path1.emplace_back(0);
        // We will capture only Min and Max members as we do not want
        // entire array to be evaluated (like if someone writes
        // query 1..1234567890)
        captureNonConstExpression(ast, varInfo, member->getMemberUnchecked(0),
                                  std::move(path1), result);
        path.emplace_back(1);
        captureNonConstExpression(ast, varInfo, member->getMemberUnchecked(1),
                                  std::move(path), result);
      } else if (member->type == NODE_TYPE_QUANTIFIER) {
        auto quantifierType =
            static_cast<Quantifier::Type>(member->getIntValue(true));
        if (quantifierType == Quantifier::Type::kAtLeast) {
          TRI_ASSERT(member->numMembers() == 1);
          auto atLeastNodeValue = member->getMemberUnchecked(0);
          if (!atLeastNodeValue->isConstant()) {
            path.emplace_back(0);
            captureNonConstExpression(ast, varInfo, atLeastNodeValue,
                                      std::move(path), result);
          }
        }
      } else {
        auto preVisitor = [&path, ast, &varInfo, &result, indexVariable,
                           evaluateFCalls](AstNode const* node) -> bool {
          auto sg = ScopeGuard([&path]() noexcept { ++path.back(); });
          if (node->isConstant()) {
            return false;
          }
          auto var = node->getAttributeAccessForVariable(true);
          if (var) {
            auto acessedVar = static_cast<Variable const*>(var->getData());
            TRI_ASSERT(acessedVar);
            if (acessedVar->needsRegister() && acessedVar != indexVariable) {
              captureNonConstExpression(
                  ast, varInfo, const_cast<AstNode*>(node), path, result);
            }
            // never dive into attribute access
            return false;
          } else if (node->type == NODE_TYPE_FCALL) {
            auto const* fn = static_cast<Function*>(node->getData());
            if (ADB_UNLIKELY(!fn || node->numMembers() != 1)) {
              // malformed node? Better abort here
              TRI_ASSERT(false);
              return false;
            }
            // TODO(Dronplane): currently only inverted index supports
            // array filter. But if other index will start to support
            // it we need here to have index type and check supported
            // functions accordingly
            if (!evaluateFCalls || iresearch::isFilter(*fn)) {
              captureFCallArgumentExpressions(ast, varInfo, node, path,
                                              indexVariable, result);
            } else {
              captureNonConstExpression(
                  ast, varInfo, const_cast<AstNode*>(node), path, result);
            }
            return false;
          } else if (node->type == NODE_TYPE_REFERENCE) {
            auto acessedVar = static_cast<Variable const*>(node->getData());
            TRI_ASSERT(acessedVar);
            if (acessedVar->needsRegister() && acessedVar != indexVariable) {
              captureNonConstExpression(
                  ast, varInfo, const_cast<AstNode*>(node), path, result);
            }
            return false;
          }
          path.push_back(0);
          // dive into hierarchy. postVisitor will do the cleanup
          sg.cancel();
          return true;
        };

        auto postVisitor = [&path](AstNode const*) -> void { ++path.back(); };

        auto visitor = [&path](AstNode* node) -> AstNode* {
          path.pop_back();
          return node;
        };
        Ast::traverseAndModify(member, preVisitor, visitor, postVisitor);
      }
    }
  }
}

void extractNonConstPartsOfLeafNode(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode* leaf, Variable const* indexVariable,
    std::vector<size_t> const& selectedMembersFromRoot,
    NonConstExpressionContainer& result) {
  if (leaf->isConstant()) {
    return;
  }

  switch (leaf->type) {
    case NODE_TYPE_FCALL:
      // FCALL at this level is most likely a geo index
      captureFCallArgumentExpressions(
          ast, varInfo, leaf, selectedMembersFromRoot, indexVariable, result);
      return;
    case NODE_TYPE_EXPANSION:
      if (leaf->numMembers() > 2 &&
          leaf->isAttributeAccessForVariable(indexVariable, false)) {
        // we need to gather all expressions from nested filter
        auto filter = leaf->getMemberUnchecked(2);
        TRI_ASSERT(filter->type == NODE_TYPE_ARRAY_FILTER);
        if (ADB_LIKELY(filter->type == NODE_TYPE_ARRAY_FILTER)) {
          auto path = selectedMembersFromRoot;
          path.emplace_back(2);
          captureArrayFilterArgumentExpressions(ast, varInfo, filter,
                                                std::move(path), evaluateFCalls,
                                                indexVariable, result);
        }
      }
      // we don't care about other expansion types
      return;
    case NODE_TYPE_OPERATOR_UNARY_NOT: {
      TRI_ASSERT(leaf->numMembers() == 1);
      auto negatedNode = leaf->getMemberUnchecked(0);
      TRI_ASSERT(negatedNode);
      if (ADB_UNLIKELY(negatedNode->isConstant())) {
        return;
      }
      auto path = selectedMembersFromRoot;
      path.emplace_back(0);
      extractNonConstPartsOfLeafNode(ast, varInfo, evaluateFCalls, index,
                                     negatedNode, indexVariable,
                                     std::move(path), result);
      return;
    }
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE: {
      TRI_ASSERT(leaf->numMembers() == 3);
      auto valueNode = leaf->getMemberUnchecked(0);
      TRI_ASSERT(valueNode);
      if (!valueNode->isConstant()) {
        auto path = selectedMembersFromRoot;
        path.emplace_back(0);
        captureNonConstExpression(ast, varInfo, valueNode, std::move(path),
                                  result);
      }
      return;
    }
    case NODE_TYPE_OPERATOR_NARY_OR:
    case NODE_TYPE_OPERATOR_NARY_AND:
      extractNonConstPartsOfJunctionCondition(ast, varInfo, evaluateFCalls,
                                              index, leaf, indexVariable,
                                              selectedMembersFromRoot, result);
      return;
    default:
      if (leaf->numMembers() != 2) {
        // The Index cannot solve non-binary operators.
        TRI_ASSERT(false);
        return;
      }
      break;
  }

  // We only support binary conditions
  TRI_ASSERT(leaf->numMembers() == 2);
  AstNode* lhs = leaf->getMember(0);
  AstNode* rhs = leaf->getMember(1);
  if (lhs->isAttributeAccessForVariable(indexVariable, false)) {
    // Index is responsible for the left side, check if right side
    // has to be evaluated
    if (!rhs->isConstant()) {
      if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        rhs = wrapInUniqueCall(
            ast, rhs,
            index != nullptr && (index->sparse() || index->isSorted()));
      }
      auto path = selectedMembersFromRoot;
      path.emplace_back(1);
      captureNonConstExpression(ast, varInfo, rhs, std::move(path), result);
    }
  } else {
    auto path = selectedMembersFromRoot;
    path.emplace_back(0);
    // Index is responsible for the right side, check if left side
    // has to be evaluated
    auto isInvertedIndexFunc = [&] {
      if (index == nullptr || index->type() != IndexType::Inverted) {
        return false;
      }
      TRI_ASSERT(lhs != nullptr);
      auto fn = static_cast<Function*>(lhs->getData());
      TRI_ASSERT(fn != nullptr);
      return iresearch::isFilter(*fn);
    };

    if (lhs->type == NODE_TYPE_FCALL &&
        (!evaluateFCalls || isInvertedIndexFunc())) {
      // most likely a geo index condition
      captureFCallArgumentExpressions(ast, varInfo, lhs, std::move(path),
                                      indexVariable, result);
    } else if (!lhs->isConstant()) {
      captureNonConstExpression(ast, varInfo, lhs, std::move(path), result);
    }
  }
}

void extractNonConstPartsOfAndPart(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* andNode, Variable const* indexVariable,
    std::vector<size_t> const& selectedMembersFromRoot,
    NonConstExpressionContainer& result) {
  // in case of a geo spatial index a might take the form
  // of a GEO_* function. We might need to evaluate fcall arguments
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  for (size_t j = 0; j < andNode->numMembers(); ++j) {
    auto leaf = andNode->getMemberUnchecked(j);
    if (!leaf->isConstant()) {
      auto path = selectedMembersFromRoot;
      path.emplace_back(j);
      extractNonConstPartsOfLeafNode(ast, varInfo, evaluateFCalls, index, leaf,
                                     indexVariable, path, result);
    }
  }
}

void extractNonConstPartsOfJunctionCondition(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* condition, Variable const* indexVariable,
    std::vector<size_t> const& selectedMembersFromRoot,
    NonConstExpressionContainer& result) {
  // conditions can be of the form (a [<|<=|>|=>] b) && ...
  TRI_ASSERT(condition != nullptr);
  TRI_ASSERT(condition->type == NODE_TYPE_OPERATOR_NARY_AND ||
             condition->type == NODE_TYPE_OPERATOR_NARY_OR);

  if (condition->type == NODE_TYPE_OPERATOR_NARY_OR) {
    for (size_t i = 0; i < condition->numMembers(); ++i) {
      auto andNode = condition->getMemberUnchecked(i);
      TRI_ASSERT(andNode);
      if (andNode->isConstant()) {
        continue;
      }
      auto path = selectedMembersFromRoot;
      path.emplace_back(i);
      extractNonConstPartsOfAndPart(ast, varInfo, evaluateFCalls, index,
                                    andNode, indexVariable, std::move(path),
                                    result);
    }
  } else {
    extractNonConstPartsOfAndPart(ast, varInfo, evaluateFCalls, index,
                                  condition, indexVariable,
                                  selectedMembersFromRoot, result);
  }
}

}  // namespace

NonConstExpressionContainer extractNonConstPartsOfIndexCondition(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* condition, Variable const* indexVariable) {
  // conditions can be of the form (a [<|<=|>|=>] b) && ...
  TRI_ASSERT(condition != nullptr);
  TRI_ASSERT(condition->type == NODE_TYPE_OPERATOR_NARY_AND ||
             condition->type == NODE_TYPE_OPERATOR_NARY_OR);
  TRI_ASSERT(indexVariable != nullptr);

  NonConstExpressionContainer result;
  extractNonConstPartsOfJunctionCondition(ast, varInfo, evaluateFCalls, index,
                                          condition, indexVariable, {}, result);
  return result;
}

}  // namespace arangodb::aql::optimizer
