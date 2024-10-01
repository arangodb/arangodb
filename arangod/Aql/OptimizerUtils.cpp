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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerUtils.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "IResearch/IResearchFeature.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
/// the usedIndexes vector may also be re-sorted
bool sortOrs(aql::Ast* ast, aql::AstNode* root, aql::Variable const* variable,
             std::vector<std::shared_ptr<Index>>& usedIndexes) {
  if (root == nullptr) {
    return true;
  }

  size_t const n = root->numMembers();

  if (n < 2) {
    return true;
  }

  if (n != usedIndexes.size()) {
    // sorting will break if the number of ORs is unequal to the number of
    // indexes but we shouldn't have got here then
    TRI_ASSERT(false);
    return false;
  }

  typedef std::pair<aql::AstNode*, std::shared_ptr<Index>> ConditionData;
  containers::SmallVector<ConditionData*, 8> conditionData;

  auto sg = scopeGuard([&conditionData]() noexcept -> void {
    for (auto& it : conditionData) {
      delete it;
    }
  });

  std::vector<aql::ConditionPart> parts;
  parts.reserve(n);

  std::pair<aql::Variable const*, std::vector<basics::AttributeName>> result;

  for (size_t i = 0; i < n; ++i) {
    // sort the conditions of each AND
    AstNode* sub = root->getMemberUnchecked(i);

    TRI_ASSERT(sub != nullptr &&
               sub->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    // cppcheck-suppress nullPointerRedundantCheck
    size_t const nAnd = sub->numMembers();

    if (nAnd != 1) {
      // we can't handle this one
      return false;
    }

    auto operand = sub->getMemberUnchecked(0);

    if (!operand->isComparisonOperator()) {
      return false;
    }

    if (operand->type == aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type == aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      result.first = nullptr;
      result.second.clear();

      if (rhs->isConstant() && lhs->isAttributeAccessForVariable(result) &&
          result.first == variable &&
          (operand->type != aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN ||
           rhs->isArray())) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(result.first, result.second, operand,
                           aql::AttributeSideType::ATTRIBUTE_LEFT, p);
      }
    }

    if (rhs->type == aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS ||
        rhs->type == aql::AstNodeType::NODE_TYPE_EXPANSION) {
      result.first = nullptr;
      result.second.clear();

      if (lhs->isConstant() && rhs->isAttributeAccessForVariable(result) &&
          result.first == variable) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(result.first, result.second, operand,
                           aql::AttributeSideType::ATTRIBUTE_RIGHT, p);
      }
    }
  }

  if (parts.size() != root->numMembers()) {
    return false;
  }

  // check if all parts use the same variable and attribute
  for (size_t i = 1; i < n; ++i) {
    auto const& lhs = parts[i - 1];
    auto const& rhs = parts[i];

    if (lhs.variable != rhs.variable ||
        lhs.attributeName != rhs.attributeName) {
      // oops, the different OR parts are on different variables or attributes
      return false;
    }
  }

  size_t previousIn = SIZE_MAX;

  for (size_t i = 0; i < n; ++i) {
    auto& p = parts[i];

    if (p.operatorType == aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        p.valueNode->isArray()) {
      TRI_ASSERT(p.valueNode->isConstant());

      if (previousIn != SIZE_MAX) {
        // merge IN with IN
        TRI_ASSERT(previousIn < i);
        auto emptyArray = ast->createNodeArray();
        auto mergedIn = ast->createNodeUnionizedArray(
            parts[previousIn].valueNode, p.valueNode);

        aql::AstNode* clone = ast->clone(root->getMember(previousIn));
        root->changeMember(previousIn, clone);
        static_cast<ConditionData*>(parts[previousIn].data)->first = clone;

        clone = ast->clone(root->getMember(i));
        root->changeMember(i, clone);
        static_cast<ConditionData*>(parts[i].data)->first = clone;

        // can now edit nodes in place...
        parts[previousIn].valueNode = mergedIn;
        {
          auto n1 = root->getMember(previousIn)->getMember(0);
          TRI_ASSERT(n1->type ==
                     aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
          TEMPORARILY_UNLOCK_NODE(n1);
          n1->changeMember(1, mergedIn);
        }

        p.valueNode = emptyArray;
        {
          auto n2 = root->getMember(i)->getMember(0);
          TRI_ASSERT(n2->type ==
                     aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN);
          TEMPORARILY_UNLOCK_NODE(n2);
          n2->changeMember(1, emptyArray);
        }

      } else {
        // note first IN
        previousIn = i;
      }
    }
  }

  // now sort all conditions by variable name, attribute name, attribute value
  std::sort(
      parts.begin(), parts.end(),
      [](aql::ConditionPart const& lhs, aql::ConditionPart const& rhs) -> bool {
        // compare variable names first
        auto res = lhs.variable->name.compare(rhs.variable->name);

        if (res != 0) {
          return res < 0;
        }

        // compare attribute names next
        res = lhs.attributeName.compare(rhs.attributeName);

        if (res != 0) {
          return res < 0;
        }

        // compare attribute values next
        auto ll = lhs.lowerBound();
        auto lr = rhs.lowerBound();

        if (ll == nullptr && lr != nullptr) {
          // left lower bound is not set but right
          return true;
        } else if (ll != nullptr && lr == nullptr) {
          // left lower bound is set but not right
          return false;
        }

        if (ll != nullptr && lr != nullptr) {
          // both lower bounds are set
          res = compareAstNodes(ll, lr, true);

          if (res != 0) {
            return res < 0;
          }
        }

        if (lhs.isLowerInclusive() && !rhs.isLowerInclusive()) {
          return true;
        }
        if (rhs.isLowerInclusive() && !lhs.isLowerInclusive()) {
          return false;
        }

        // all things equal
        return false;
      });

  TRI_ASSERT(parts.size() == conditionData.size());

  // clean up
  root->clearMembers();

  usedIndexes.clear();
  containers::FlatHashSet<std::string> seenIndexConditions;

  // and rebuild
  for (size_t i = 0; i < n; ++i) {
    if (parts[i].operatorType ==
            aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        parts[i].valueNode->isArray() &&
        parts[i].valueNode->numMembers() == 0) {
      // can optimize away empty IN array
      continue;
    }

    auto conditionData = static_cast<ConditionData*>(parts[i].data);
    bool isUnique = true;

    if (!usedIndexes.empty()) {
      // try to find duplicate condition parts, and only return each
      // unique condition part once
      try {
        auto conditionString =
            absl::StrCat(conditionData->first->toString(), " - ",
                         conditionData->second->id().id());
        isUnique =
            seenIndexConditions.emplace(std::move(conditionString)).second;
        // we already saw the same combination of index & condition
        // don't add it again
      } catch (...) {
        // condition stringification may fail. in this case, we simply carry own
        // without simplifying the condition
      }
    }

    if (isUnique) {
      root->addMember(conditionData->first);
      usedIndexes.emplace_back(conditionData->second);
    }
  }

  return true;
}

std::pair<bool, bool> findIndexHandleForAndNode(
    transaction::Methods& trx,
    std::vector<std::shared_ptr<Index>> const& indexes, aql::AstNode* node,
    aql::Variable const* reference, aql::SortCondition const& sortCondition,
    size_t itemsInCollection, aql::IndexHint const& hint,
    std::vector<transaction::Methods::IndexHandle>& usedIndexes,
    aql::AstNode*& specializedCondition, bool& isSparse, bool failOnForcedHint,
    ReadOwnWrites readOwnWrites) {
  if (hint.isDisabled()) {
    // usage of index disabled via index hint: disableIndex: true
    return std::make_pair(false, false);
  }

  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort = false;

  auto considerIndex =
      [&trx, &bestIndex, &bestCost, &bestSupportsFilter, &bestSupportsSort,
       &indexes, node, reference, itemsInCollection, readOwnWrites,
       &sortCondition](std::shared_ptr<Index> const& idx) -> void {
    TRI_ASSERT(!idx->inProgress());

    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;
    size_t coveredAttributes = 0;

    bool supportsFilter = false;
    bool supportsSort = false;

    if (readOwnWrites == ReadOwnWrites::yes &&
        idx->type() == arangodb::Index::TRI_IDX_TYPE_INVERTED_INDEX) {
      // inverted index does not support ReadOwnWrites
      return;
    }

    // check if the index supports the filter condition
    Index::FilterCosts costs = idx->supportsFilterCondition(
        trx, indexes, node, reference, itemsInIndex);

    if (costs.supportsCondition) {
      // index supports the filter condition
      filterCost = costs.estimatedCosts;
      // this reduces the number of items left
      itemsInIndex = costs.estimatedItems;
      supportsFilter = true;
    } else {
      // index does not support the filter condition
      filterCost = itemsInIndex * 1.5;
    }

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    if (sortCondition.isUnidirectional()) {
      // only go in here if we actually have a sort condition and it can in
      // general be supported by an index. for this, a sort condition must not
      // be empty, must consist only of attribute access, and all attributes
      // must be sorted in the same direction
      Index::SortCosts sc =
          idx->supportsSortCondition(&sortCondition, reference, itemsInIndex);
      if (sc.supportsCondition) {
        supportsSort = true;
      }
      sortCost = sc.estimatedCosts;
      coveredAttributes = sc.coveredAttributes;
    }

    if (!supportsSort && isOnlyAttributeAccess && node->isOnlyEqualityMatch()) {
      // index cannot be used for sorting, but the filter condition consists
      // only of equality lookups (==)
      // now check if the index fields are the same as the sort condition fields
      // e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1, c.value2
      if (coveredAttributes == sortCondition.numAttributes() &&
          (idx->isSorted() ||
           idx->fields().size() == sortCondition.numAttributes())) {
        // no sorting needed
        sortCost = 0.0;
      }
    }

    if (!supportsFilter && !supportsSort) {
      return;
    }

    if (!sortCondition.isEmpty()) {
      // only take into account the costs for sorting if there is actually
      // something to sort
      if (!supportsSort) {
        sortCost = Index::SortCosts::defaultCosts(itemsInIndex).estimatedCosts;
      }
    } else {
      sortCost = 0.0;
    }

    double totalCost = filterCost + sortCost;

    // the more attributes an index contains, the more useful it will be for
    // projections.
    double projectionsFactor = 1.0 - ((idx->fields().size() - 1) * 0.02);
    totalCost *= projectionsFactor;

    LOG_TOPIC("7278d", TRACE, Logger::FIXME)
        << "looked at candidate index: " << idx->name()
        << ", isSorted: " << idx->isSorted() << ", isSparse: " << idx->sparse()
        << ", fields: " << idx->fields()
        << ", num fields: " << idx->fields().size()
        << ", hasSelectivityEstimate: " << idx->hasSelectivityEstimate()
        << ", selectivityEstimate: "
        << (idx->hasSelectivityEstimate()
                ? std::to_string(idx->selectivityEstimate())
                : "n/a")
        << ", supportsFilter: " << supportsFilter
        << ", supportsSort: " << supportsSort
        << ", projectionsFactor: " << projectionsFactor
        << ", isOnlyAttributeAccess: " << isOnlyAttributeAccess
        << ", isUnidirectional: " << sortCondition.isUnidirectional()
        << ", isOnlyEqualityMatch: " << node->isOnlyEqualityMatch()
        << ", itemsInIndex/estimatedItems: " << itemsInIndex
        << ", filterCost: " << filterCost << ", sortCost: " << sortCost
        << ", totalCost: " << totalCost;

    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
      bestSupportsFilter = supportsFilter;
      bestSupportsSort = supportsSort;
    }
  };

  if (hint.isSimple()) {
    std::vector<std::string> const& hintedIndices = hint.candidateIndexes();
    for (std::string const& hinted : hintedIndices) {
      std::shared_ptr<Index> matched;
      for (std::shared_ptr<Index> const& idx : indexes) {
        if (idx->inProgress()) {
          continue;
        }
        if (idx->name() == hinted) {
          matched = idx;
          break;
        }
      }

      if (matched != nullptr) {
        considerIndex(matched);
        if (bestIndex != nullptr) {
          break;
        }
      }
    }

    if (hint.isForced() && bestIndex == nullptr && failOnForcedHint) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
          absl::StrCat("could not use index hint to serve query; ",
                       hint.toString()));
    }
  }

  if (bestIndex == nullptr) {
    for (auto const& idx : indexes) {
      if (idx->inProgress()) {
        continue;
      }
      if (!Index::onlyHintForced(idx->type())) {
        considerIndex(idx);
      }
    }
  }

  if (bestIndex == nullptr) {
    // intentionally commented out here. can be enabled during development
    // LOG_TOPIC("3aac4", TRACE, Logger::FIXME) << "- no index used";
    return std::make_pair(false, false);
  }

  LOG_TOPIC("1d732", TRACE, Logger::FIXME)
      << "selected index: " << bestIndex.get()
      << ", isSorted: " << bestIndex->isSorted()
      << ", isSparse: " << bestIndex->sparse()
      << ", fields: " << bestIndex->fields().size();

  // intentionally commented out here. can be enabled during development
  // LOG_TOPIC("4b655", TRACE, Logger::FIXME) << "- picked: " <<
  // bestIndex.get();

  specializedCondition = bestIndex->specializeCondition(trx, node, reference);

  usedIndexes.emplace_back(bestIndex);
  isSparse = bestIndex->sparse();

  return std::make_pair(bestSupportsFilter, bestSupportsSort);
}

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
  auto e = std::make_unique<aql::Expression>(ast, expression);

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
            auto const* fn = static_cast<aql::Function*>(node->getData());
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

AstNode* wrapInUniqueCall(Ast* ast, AstNode* node, bool sorted) {
  if (node->type != aql::NODE_TYPE_ARRAY || node->numMembers() >= 2) {
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

void extractNonConstPartsOfJunctionCondition(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* condition, Variable const* indexVariable,
    std::vector<size_t> const& selectedMembersFromRoot,
    NonConstExpressionContainer& result);

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
      if (index == nullptr ||
          index->type() != arangodb::Index::TRI_IDX_TYPE_INVERTED_INDEX) {
        return false;
      }
      TRI_ASSERT(lhs != nullptr);
      auto fn = static_cast<aql::Function*>(lhs->getData());
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

namespace arangodb::aql::utils {

// find projection attributes for variable v, starting from node n
// down to the root node of the plan/subquery.
// returns true if it is safe to reduce the full document data from
// "v" to only the projections stored in "attributes". returns false
// otherwise. if false is returned, the contents of "attributes" must
// be ignored by the caller.
// note: this function will *not* wipe "attributes" if there is already
// some data in it.
bool findProjections(ExecutionNode* n, Variable const* v,
                     std::string_view expectedAttribute,
                     bool excludeStartNodeFilterCondition,
                     containers::FlatHashSet<AttributeNamePath>& attributes) {
  using EN = aql::ExecutionNode;

  VarSet vars;

  // Returns true if we managed to extract an attribute path on the given
  // variable. in the true case attributes set is modified by the found
  // AttributeNamePath.
  auto tryAndExtractProjectionsFromExpression =
      [&vars, &attributes, expectedAttribute, v](ExecutionNode const* current,
                                                 AstNode const* node) -> bool {
    vars.clear();
    current->getVariablesUsedHere(vars);

    if (vars.find(v) != vars.end() &&
        !Ast::getReferencedAttributesRecursive(
            node, v, /*expectedAttribute*/ expectedAttribute, attributes,
            current->plan()->getAst()->query().resourceMonitor())) {
      // cannot use projections for this variable
      return false;
    }
    return true;
  };

  auto checkExpression = [&tryAndExtractProjectionsFromExpression](
                             ExecutionNode const* current,
                             Expression const* exp) -> bool {
    return (exp == nullptr ||
            tryAndExtractProjectionsFromExpression(current, exp->node()));
  };

  ExecutionNode* current = n;
  while (current != nullptr) {
    bool doRegularCheck = false;

    if (current->getType() == EN::TRAVERSAL) {
      // check prune condition of traversal
      TraversalNode const* traversalNode =
          ExecutionNode::castTo<TraversalNode const*>(current);

      if (traversalNode->usesInVariable() && traversalNode->inVariable() == v) {
        // start vertex of traversal is our input variable.
        // we need at least the _id attribute from the variable.
        AttributeNamePath anp{
            StaticStrings::IdString,
            traversalNode->plan()->getAst()->query().resourceMonitor()};
        attributes.emplace(std::move(anp));
      }

      // prune condition has to be treated in a special way, because the
      // normal getVariablesUsedHere() call for a TraversalNode does not
      // return the vertex out variable or the edge out variable if they
      // are used by the prune condition.
      Expression const* pruneExpression = traversalNode->pruneExpression();
      if (pruneExpression != nullptr) {
        std::vector<Variable const*> pruneVars;
        traversalNode->getPruneVariables(pruneVars);
        if (std::find(pruneVars.begin(), pruneVars.end(), v) !=
                pruneVars.end() &&
            !Ast::getReferencedAttributesRecursive(
                pruneExpression->node(), v, expectedAttribute, attributes,
                traversalNode->plan()->getAst()->query().resourceMonitor())) {
          // cannot use projections for this variable
          return false;
        }
      }

      if (!checkExpression(traversalNode,
                           traversalNode->postFilterExpression())) {
        // cannot use projections for this variable
        return false;
      }
    } else if (current->getType() == EN::REMOVE) {
      RemoveNode const* removeNode =
          ExecutionNode::castTo<RemoveNode const*>(current);
      if (removeNode->inVariable() == v) {
        // FOR doc IN collection REMOVE doc IN ...
        attributes.emplace(aql::AttributeNamePath(
            StaticStrings::KeyString,
            removeNode->plan()->getAst()->query().resourceMonitor()));
      } else {
        doRegularCheck = true;
      }
    } else if (current->getType() == EN::UPDATE ||
               current->getType() == EN::REPLACE) {
      UpdateReplaceNode const* modificationNode =
          ExecutionNode::castTo<UpdateReplaceNode const*>(current);

      if (modificationNode->inKeyVariable() == v &&
          modificationNode->inDocVariable() != v) {
        // FOR doc IN collection UPDATE/REPLACE doc IN ...
        attributes.emplace(aql::AttributeNamePath(
            StaticStrings::KeyString,
            modificationNode->plan()->getAst()->query().resourceMonitor()));
      } else {
        doRegularCheck = true;
      }
    } else if (current->getType() == EN::CALCULATION) {
      CalculationNode const* calculationNode =
          ExecutionNode::castTo<CalculationNode const*>(current);
      if (!checkExpression(calculationNode, calculationNode->expression())) {
        return false;
      }
    } else if (current->getType() == EN::ENUMERATE_IRESEARCH_VIEW) {
      iresearch::IResearchViewNode const* viewNode =
          ExecutionNode::castTo<iresearch::IResearchViewNode const*>(current);
      // filter condition
      if (!tryAndExtractProjectionsFromExpression(
              viewNode, &viewNode->filterCondition())) {
        return false;
      }
      // scorers
      for (auto const& it : viewNode->scorers()) {
        if (!tryAndExtractProjectionsFromExpression(viewNode, it.node)) {
          return false;
        }
      }
    } else if (current->getType() == EN::GATHER) {
      // compare sort attributes of GatherNode
      auto gn = ExecutionNode::castTo<GatherNode const*>(current);
      for (auto const& it : gn->elements()) {
        if (it.var == v) {
          if (it.attributePath.empty()) {
            // sort of GatherNode refers to the entire document, not to an
            // attribute of the document
            return false;
          }
          // insert attribute name into the set of attributes that we need for
          // our projection
          attributes.emplace(it.attributePath,
                             gn->plan()->getAst()->query().resourceMonitor());
        }
      }
    } else if (current->getType() == EN::ENUMERATE_COLLECTION) {
      EnumerateCollectionNode const* en =
          ExecutionNode::castTo<EnumerateCollectionNode const*>(current);

      if ((!excludeStartNodeFilterCondition || current != n) &&
          en->hasFilter()) {
        if (!Ast::getReferencedAttributesRecursive(
                en->filter()->node(), v,
                /*expectedAttribute*/ expectedAttribute, attributes,
                en->plan()->getAst()->query().resourceMonitor())) {
          return false;
        }
      }
    } else if (current->getType() == EN::INDEX) {
      IndexNode const* indexNode =
          ExecutionNode::castTo<IndexNode const*>(current);
      Condition const* condition = indexNode->condition();

      if (condition != nullptr && condition->root() != nullptr &&
          !tryAndExtractProjectionsFromExpression(indexNode,
                                                  condition->root())) {
        return false;
      }

      if ((!excludeStartNodeFilterCondition || current != n) &&
          indexNode->hasFilter()) {
        if (!Ast::getReferencedAttributesRecursive(
                indexNode->filter()->node(), v,
                /*expectedAttribute*/ expectedAttribute, attributes,
                indexNode->plan()->getAst()->query().resourceMonitor())) {
          return false;
        }
      }
    } else if (current->getType() == EN::SUBQUERY) {
      auto sub = ExecutionNode::castTo<SubqueryNode*>(current);
      ExecutionNode* top = sub->getSubquery();
      while (top->hasDependency()) {
        top = top->getFirstDependency();
      }
      if (!findProjections(top, v, expectedAttribute, false, attributes)) {
        return false;
      }
    } else {
      // all other node types mandate a check
      doRegularCheck = true;
    }

    if (doRegularCheck) {
      vars.clear();
      current->getVariablesUsedHere(vars);

      if (vars.contains(v)) {
        // original variable is still used here
        return false;
      }
    }

    current = current->getFirstParent();
  }

  return true;
}

Projections translateLMIndexVarsToProjections(
    ExecutionPlan* plan, IndexNode::IndexValuesVars const& indexVars,
    transaction::Methods::IndexHandle index) {
  // Translate the late materialize "projections" description
  // into the usual projections description
  auto& coveredFields = index->coveredFields();

  std::vector<AttributeNamePath> projectedAttributes;
  for (auto [var, fieldIndex] : indexVars.second) {
    auto& field = coveredFields[fieldIndex];
    std::vector<std::string> fieldCopy;
    fieldCopy.reserve(field.size());
    std::transform(field.begin(), field.end(), std::back_inserter(fieldCopy),
                   [&](auto const& attr) {
                     TRI_ASSERT(attr.shouldExpand == false);
                     return attr.name;
                   });
    projectedAttributes.emplace_back(std::move(fieldCopy),
                                     plan->getAst()->query().resourceMonitor());
  }

  Projections projections{std::move(projectedAttributes)};

  std::size_t i = 0;
  for (auto [var, fieldIndex] : indexVars.second) {
    auto& proj = projections[i++];
    proj.coveringIndexPosition = fieldIndex;
    proj.coveringIndexCutoff = proj.path.size();
    proj.variable = var;
    proj.levelsToClose = proj.startsAtLevel = 0;
    proj.type = proj.path.type();
  }

  if (index->covers(projections)) {
    projections.setCoveringContext(index->collection().id(), index);
  }
  return projections;
}

/// @brief Gets the best fitting index for one specific condition.
///        Difference to IndexHandles: Condition is only one NARY_AND
///        and the Condition stays unmodified. Also does not care for sorting
///        Returns false if no index could be found.

bool getBestIndexHandleForFilterCondition(
    transaction::Methods& trx, aql::Collection const& collection,
    aql::AstNode* node, aql::Variable const* reference,
    size_t itemsInCollection, aql::IndexHint const& hint,
    std::shared_ptr<Index>& usedIndex, ReadOwnWrites readOwnWrites,
    bool onlyEdgeIndexes) {
  // We can only start after DNF transformation and only a single AND
  TRI_ASSERT(node->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
  if (node->numMembers() == 0) {
    // Well no index can serve no condition.
    return false;
  }

  auto indexes = collection.indexes();
  if (onlyEdgeIndexes) {
    indexes.erase(
        std::remove_if(indexes.begin(), indexes.end(),
                       [](auto&& idx) {
                         return idx->type() !=
                                Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX;
                       }),
        indexes.end());
  }

  aql::SortCondition sortCondition;    // always empty here
  aql::AstNode* specializedCondition;  // unused
  bool isSparse;                       // unused
  std::vector<std::shared_ptr<Index>> usedIndexes;
  if (findIndexHandleForAndNode(trx, indexes, node, reference, sortCondition,
                                itemsInCollection, hint, usedIndexes,
                                specializedCondition, isSparse,
                                true /*failOnForcedHint*/, readOwnWrites)
          .first) {
    TRI_ASSERT(!usedIndexes.empty());
    usedIndex = usedIndexes[0];
    return true;
  }
  return false;
}

/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
    transaction::Methods& trx, aql::Collection const& coll, aql::Ast* ast,
    aql::AstNode* root, aql::Variable const* reference,
    aql::SortCondition const* sortCondition, size_t itemsInCollection,
    aql::IndexHint const& hint,
    std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted,
    bool& isAllCoveredByIndex, ReadOwnWrites readOwnWrites) {
  // We can only start after DNF transformation
  TRI_ASSERT(root->type == aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR);
  auto indexes = coll.indexes();

  // must edit root in place; TODO change so we can replace with copy
  TEMPORARILY_UNLOCK_NODE(root);

  bool canUseForFilter = (root->numMembers() > 0);
  bool canUseForSort = false;
  bool isSparse = false;

  TRI_ASSERT(usedIndexes.empty());

  // we might have an inverted index - it could cover whole condition at once.
  // Give it a try
  if (std::exchange(isAllCoveredByIndex, false)) {
    for (auto& index : indexes) {
      if (index->inProgress()) {
        continue;
      }
      if (readOwnWrites == ReadOwnWrites::yes &&
          index->type() == arangodb::Index::TRI_IDX_TYPE_INVERTED_INDEX) {
        // inverted index does not support ReadOwnWrites
        continue;
      }
      if (index->type() == Index::TRI_IDX_TYPE_INVERTED_INDEX &&
          // apply this index only if hinted
          hint.isSimple() &&
          std::find(hint.candidateIndexes().begin(),
                    hint.candidateIndexes().end(),
                    index->name()) != hint.candidateIndexes().end()) {
        auto costs = index->supportsFilterCondition(
            trx, indexes, root, reference, itemsInCollection);
        if (costs.supportsCondition) {
          // we need to find 'root' in 'ast' and replace it with specialized
          // version but for now we know that index will not alter the node, so
          // just an assert
          index->specializeCondition(trx, root, reference);
          usedIndexes.emplace_back(index);
          isAllCoveredByIndex = true;
          // FIXME: we should somehow consider other indices and calculate here
          // "overall" score Also a question: if sort is covered but filter is
          // not ? What is more optimal?
          auto const sortSupport = index->supportsSortCondition(
              sortCondition, reference, itemsInCollection);
          return std::make_pair(true, sortSupport.supportsCondition);
        }
      }
    }
  }
  size_t const n = root->numMembers();
  for (size_t i = 0; i < n; ++i) {
    // BTS-398: if there are multiple OR-ed conditions, fail only for forced
    // index hints if no index can be found for _any_ condition part.
    auto node = root->getMemberUnchecked(i);
    aql::AstNode* specializedCondition = nullptr;

    bool failOnForcedHint =
        (hint.isForced() && i + 1 == n && usedIndexes.empty());
    auto canUseIndex = findIndexHandleForAndNode(
        trx, indexes, node, reference, *sortCondition, itemsInCollection, hint,
        usedIndexes, specializedCondition, isSparse, failOnForcedHint,
        readOwnWrites);

    if (canUseIndex.second && !canUseIndex.first) {
      // index can be used for sorting only
      // we need to abort further searching and only return one index
      TRI_ASSERT(!usedIndexes.empty());
      if (usedIndexes.size() > 1) {
        auto sortIndex = usedIndexes.back();

        usedIndexes.clear();
        usedIndexes.emplace_back(sortIndex);
      }

      TRI_ASSERT(usedIndexes.size() == 1);

      if (isSparse) {
        // cannot use a sparse index for sorting alone
        usedIndexes.clear();
      }
      return std::make_pair(false, !usedIndexes.empty());
    }

    canUseForFilter &= canUseIndex.first;
    canUseForSort |= canUseIndex.second;

    root->changeMember(i, specializedCondition);
  }

  if (canUseForFilter) {
    isSorted = sortOrs(ast, root, reference, usedIndexes);
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return std::make_pair(canUseForFilter, canUseForSort);
}

/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool getIndexForSortCondition(aql::Collection const& coll,
                              aql::SortCondition const* sortCondition,
                              aql::Variable const* reference,
                              size_t itemsInIndex, aql::IndexHint const& hint,
                              std::vector<std::shared_ptr<Index>>& usedIndexes,
                              size_t& coveredAttributes) {
  if (!hint.isDisabled()) {
    // We do not have a condition. But we have a sort!
    if (!sortCondition->isEmpty() && sortCondition->isOnlyAttributeAccess() &&
        sortCondition->isUnidirectional()) {
      double bestCost = 0.0;
      std::shared_ptr<Index> bestIndex;

      auto considerIndex =
          [reference, sortCondition, itemsInIndex, &bestCost, &bestIndex,
           &coveredAttributes](std::shared_ptr<Index> const& idx) -> void {
        TRI_ASSERT(!idx->inProgress());

        Index::SortCosts costs =
            idx->supportsSortCondition(sortCondition, reference, itemsInIndex);
        if (costs.supportsCondition &&
            (bestIndex == nullptr || costs.estimatedCosts < bestCost)) {
          bestCost = costs.estimatedCosts;
          bestIndex = idx;
          coveredAttributes = costs.coveredAttributes;
        }
      };

      auto indexes = coll.indexes();

      if (hint.isSimple()) {
        std::vector<std::string> const& hintedIndices = hint.candidateIndexes();
        for (std::string const& hinted : hintedIndices) {
          std::shared_ptr<Index> matched;
          for (std::shared_ptr<Index> const& idx : indexes) {
            if (idx->inProgress()) {
              continue;
            }
            if (idx->name() == hinted) {
              matched = idx;
              break;
            }
          }

          if (matched != nullptr) {
            considerIndex(matched);
            if (bestIndex != nullptr) {
              break;
            }
          }
        }

        if (hint.isForced() && bestIndex == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
              "could not use index hint to serve query; " + hint.toString());
        }
      }

      if (bestIndex == nullptr) {
        for (auto const& idx : indexes) {
          if (idx->inProgress()) {
            continue;
          }
          if (!Index::onlyHintForced(idx->type())) {
            considerIndex(idx);
          }
        }
      }

      if (bestIndex != nullptr) {
        usedIndexes.emplace_back(bestIndex);
      }

      return bestIndex != nullptr;
    }
  }  // disableIndex

  // No Index and no sort condition that
  // can be supported by an index.
  // Nothing to do here.
  return false;
}

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

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node) {
  using EN = arangodb::aql::ExecutionNode;
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<
                 arangodb::aql::EnumerateCollectionNode const*>(node)
          ->collection();
    case EN::INDEX:
      return ExecutionNode::castTo<arangodb::aql::IndexNode const*>(node)
          ->collection();
    case EN::TRAVERSAL:
    case EN::ENUMERATE_PATHS:
    case EN::SHORTEST_PATH:
      return ExecutionNode::castTo<arangodb::aql::GraphNode const*>(node)
          ->collection();

    default:
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have a collection");
  }
}

}  // namespace arangodb::aql::utils
