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

#include "UseIndexForSort.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/WalkerWorker.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

struct SortToIndexNode final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  ExecutionPlan* _plan;
  SortNode* _sortNode;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  std::vector<std::vector<RegisterId>> _filters;
  bool _modified;

  explicit SortToIndexNode(ExecutionPlan* plan)
      : _plan(plan), _sortNode(nullptr), _modified(false) {
    _filters.emplace_back();
  }

  void getSpecialAttributes(
      AstNode const* node, Variable const* variable,
      std::vector<std::vector<arangodb::basics::AttributeName>>&
          constAttributes,
      ::arangodb::containers::HashSet<
          std::vector<arangodb::basics::AttributeName>>& nonNullAttributes)
      const {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      ast::LogicalOperatorNode andOp(node);
      getSpecialAttributes(andOp.getLeft(), variable, constAttributes,
                           nonNullAttributes);
      getSpecialAttributes(andOp.getRight(), variable, constAttributes,
                           nonNullAttributes);
      return;
    }

    if (!node->isComparisonOperator()) {
      return;
    }

    TRI_ASSERT(node->isComparisonOperator());
    ast::BinaryOperatorNode binOp(node);
    AstNode const* lhs = binOp.getLeft();
    AstNode const* rhs = binOp.getRight();
    AstNode const* check = nullptr;

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        check = rhs;
      } else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_NE) {
      if (lhs->isNullValue() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        check = rhs;
      } else if (rhs->isNullValue() &&
                 lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LT &&
               lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LE &&
               lhs->isConstant() && !lhs->isNullValue() &&
               rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GT &&
               rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      check = lhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GE &&
               rhs->isConstant() && !rhs->isNullValue() &&
               lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      check = lhs;
    }

    if (check == nullptr) {
      return;
    }

    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
        result;

    if (check->isAttributeAccessForVariable(result, false) &&
        result.first == variable) {
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        constAttributes.emplace_back(std::move(result.second));
      } else {
        nonNullAttributes.emplace(std::move(result.second));
      }
    }
  }

  void processCollectionAttributes(
      Variable const* variable,
      std::vector<std::vector<arangodb::basics::AttributeName>>&
          constAttributes,
      ::arangodb::containers::HashSet<
          std::vector<arangodb::basics::AttributeName>>& nonNullAttributes)
      const {
    TRI_ASSERT(!_filters.empty());
    for (auto const& filter : _filters.back()) {
      TRI_ASSERT(filter.isRegularRegister());
      auto it = _variableDefinitions.find(filter.value());
      if (it != _variableDefinitions.end()) {
        getSpecialAttributes((*it).second, variable, constAttributes,
                             nonNullAttributes);
      }
    }
  }

  SortElementVector makeIndexSortElements(SortCondition const& sortCondition,
                                          Variable const* outVariable) {
    TRI_ASSERT(sortCondition.isOnlyAttributeAccess());
    SortElementVector elements;
    elements.reserve(sortCondition.numAttributes());
    for (auto const& field : sortCondition.sortFields()) {
      std::vector<std::string> path;
      path.reserve(field.attributes.size());
      std::transform(field.attributes.begin(), field.attributes.end(),
                     std::back_inserter(path),
                     [](auto const& a) { return a.name; });
      elements.push_back(
          SortElement::createWithPath(outVariable, field.asc, path));
    }
    return elements;
  }

  bool handleEnumerateCollectionNode(
      EnumerateCollectionNode* enumerateCollectionNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (enumerateCollectionNode->isInInnerLoop()) {
      return true;
    }

    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    ::arangodb::containers::HashSet<
        std::vector<arangodb::basics::AttributeName>>
        nonNullAttributes;
    processCollectionAttributes(enumerateCollectionNode->outVariable(),
                                constAttributes, nonNullAttributes);

    SortCondition sortCondition(_plan, _sorts, constAttributes,
                                nonNullAttributes, _variableDefinitions);

    if (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess()) {
      Variable const* outVariable = enumerateCollectionNode->outVariable();
      std::vector<transaction::Methods::IndexHandle> usedIndexes;
      size_t coveredAttributes = 0;

      Collection const* coll = enumerateCollectionNode->collection();
      TRI_ASSERT(coll != nullptr);
      size_t numDocs =
          coll->count(&_plan->getAst()->query().trxForOptimization(),
                      transaction::CountType::kTryCache);

      bool canBeUsed = arangodb::aql::utils::getIndexForSortCondition(
          *coll, &sortCondition, outVariable, numDocs,
          enumerateCollectionNode->hint(), usedIndexes, coveredAttributes);
      if (canBeUsed) {
        auto condition = std::make_unique<Condition>(_plan->getAst());
        condition->normalize(_plan);
        TRI_ASSERT(usedIndexes.size() == 1);
        IndexIteratorOptions opts;
        TRI_ASSERT(!sortCondition.isEmpty());
        opts.ascending = sortCondition.sortFields()[0].asc;
        opts.useCache = false;
        auto indexNode = _plan->createNode<IndexNode>(
            _plan, _plan->nextId(), enumerateCollectionNode->collection(),
            outVariable, usedIndexes, false, std::move(condition), opts);

        enumerateCollectionNode->CollectionAccessingNode::cloneInto(*indexNode);
        enumerateCollectionNode->DocumentProducingNode::cloneInto(_plan,
                                                                  *indexNode);

        _plan->replaceNode(enumerateCollectionNode, indexNode);
        _modified = true;

        if (coveredAttributes == sortCondition.numAttributes()) {
          deleteSortNode(indexNode, sortCondition);
        } else if (usedIndexes.size() == 1 &&
                   usedIndexes[0]->type() ==
                       Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX) {
          _sortNode->setGroupedElements(coveredAttributes);
        }
      }
    }

    return true;
  }

  void deleteSortNode(IndexNode* indexNode, SortCondition& sortCondition) {
    auto sortNode = _plan->getNodeById(_sortNode->id());
    _plan->unlinkNode(sortNode);
    indexNode->needsGatherNodeSort(
        makeIndexSortElements(sortCondition, indexNode->outVariable()));
    _modified = true;
  }

  bool handleIndexNode(IndexNode* indexNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (indexNode->isInInnerLoop()) {
      return true;
    }

    auto const& indexes = indexNode->getIndexes();
    auto cond = indexNode->condition();
    TRI_ASSERT(cond != nullptr);

    Variable const* outVariable = indexNode->outVariable();
    TRI_ASSERT(outVariable != nullptr);

    auto index = indexes[0];
    bool isSorted = index->isSorted();
    bool isSparse = index->sparse();
    std::vector<std::vector<arangodb::basics::AttributeName>> fields =
        index->fields();

    if (indexes.size() != 1) {
      if (!cond->isSorted()) {
        return true;
      }

      if (isSparse) {
        return true;
      }

      for (auto& idx : indexes) {
        if (idx != index) {
          return true;
        }
      }
    }

    TRI_ASSERT(indexes.size() == 1 || cond->isSorted());

    if (indexes.size() == 1 && isSorted) {
      isSparse = false;
    }

    SortCondition sortCondition(
        _plan, _sorts, cond->getConstAttributes(outVariable, !isSparse),
        cond->getNonNullAttributes(outVariable), _variableDefinitions);

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    bool indexFullyCoversSortCondition = false;
    if (index->type() == Index::IndexType::TRI_IDX_TYPE_INVERTED_INDEX) {
      indexFullyCoversSortCondition =
          index->supportsSortCondition(&sortCondition, outVariable, 1)
              .supportsCondition;
    } else {
      indexFullyCoversSortCondition =
          isOnlyAttributeAccess && isSorted && !isSparse &&
          sortCondition.isUnidirectional() &&
          sortCondition.isAscending() == indexNode->options().ascending &&
          sortCondition.coveredAttributes(outVariable, fields) >=
              sortCondition.numAttributes();
    }

    if (indexFullyCoversSortCondition) {
      deleteSortNode(indexNode, sortCondition);
    } else if (index->type() ==
               Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX) {
      auto [numberOfCoveredAttributes, sortIsAscending] =
          sortCondition.coveredUnidirectionalAttributesWithDirection(
              outVariable, fields);
      if (isOnlyAttributeAccess && isSorted && !isSparse &&
          numberOfCoveredAttributes > 0) {
        indexNode->setAscending(sortIsAscending);
        _sortNode->setGroupedElements(numberOfCoveredAttributes);
        _modified = true;
      }
    } else {
      if (isOnlyAttributeAccess && indexes.size() == 1) {
        auto root = cond->root();

        if (root != nullptr) {
          auto condNode = root->getMember(0);

          if (condNode->isOnlyEqualityMatch()) {
            size_t const numCovered =
                sortCondition.coveredAttributes(outVariable, fields);

            if (numCovered == sortCondition.numAttributes() &&
                sortCondition.isUnidirectional() &&
                (isSorted || fields.size() >= sortCondition.numAttributes())) {
              deleteSortNode(indexNode, sortCondition);
              indexNode->setAscending(sortCondition.isAscending());
            }
          }
        }
      }
    }

    return true;
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH:
      case EN::ENUMERATE_LIST:
      case EN::ENUMERATE_IRESEARCH_VIEW:
        return true;

      case EN::SUBQUERY: {
        _filters.emplace_back();
        return false;
      }

      case EN::FILTER: {
        auto inVariable =
            ExecutionNode::castTo<FilterNode const*>(en)->inVariable()->id;
        _filters.back().emplace_back(inVariable);
        return false;
      }

      case EN::CALCULATION: {
        _variableDefinitions.try_emplace(
            ExecutionNode::castTo<CalculationNode const*>(en)
                ->outVariable()
                ->id,
            ExecutionNode::castTo<CalculationNode const*>(en)
                ->expression()
                ->node());
        return false;
      }

      case EN::SINGLETON:
      case EN::COLLECT:
      case EN::WINDOW:
      case EN::INSERT:
      case EN::REMOVE:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
      case EN::MATERIALIZE:
      case EN::LIMIT:
      case EN::ENUMERATE_NEAR_VECTORS:
        return true;

      case EN::SORT:
        if (!_sorts.empty() || _sortNode != nullptr) {
          return true;
        }
        _sortNode = ExecutionNode::castTo<SortNode*>(en);
        for (auto& it : _sortNode->elements()) {
          TRI_ASSERT(it.attributePath.empty());
          _sorts.emplace_back(it.var, it.ascending);
        }
        return false;

      case EN::INDEX:
        return handleIndexNode(ExecutionNode::castTo<IndexNode*>(en));
      case EN::ENUMERATE_COLLECTION:
        return handleEnumerateCollectionNode(
            ExecutionNode::castTo<EnumerateCollectionNode*>(en));

      default: {
        TRI_ASSERT(false);
      }
    }
    return true;
  }

  void after(ExecutionNode* en) override final {
    if (en->getType() == EN::SUBQUERY) {
      TRI_ASSERT(!_filters.empty());
      _filters.pop_back();
    }
  }
};

}  // namespace

void useIndexForSortRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                         OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SORT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto sortNode = ExecutionNode::castTo<SortNode*>(n);

    SortToIndexNode finder(plan.get());
    sortNode->walk(finder);

    if (finder._modified) {
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
