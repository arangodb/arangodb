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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesUseIndexes.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/ScopeGuard.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

struct SortToIndexNode final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  ExecutionPlan* _plan;
  SortNode* _sortNode;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  std::vector<std::vector<RegisterId>> _filters;
  bool _modified;

 public:
  explicit SortToIndexNode(ExecutionPlan* plan)
      : _plan(plan), _sortNode(nullptr), _modified(false) {
    _filters.emplace_back();
  }

  /// @brief gets the attributes from the filter conditions that will have a
  /// constant value (e.g. doc.value == 123) or than can be proven to be != null
  void getSpecialAttributes(
      AstNode const* node, Variable const* variable,
      std::vector<std::vector<arangodb::basics::AttributeName>>&
          constAttributes,
      ::arangodb::containers::HashSet<
          std::vector<arangodb::basics::AttributeName>>& nonNullAttributes)
      const {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      // recurse into both sides
      getSpecialAttributes(node->getMemberUnchecked(0), variable,
                           constAttributes, nonNullAttributes);
      getSpecialAttributes(node->getMemberUnchecked(1), variable,
                           constAttributes, nonNullAttributes);
      return;
    }

    if (!node->isComparisonOperator()) {
      return;
    }

    TRI_ASSERT(node->isComparisonOperator());

    AstNode const* lhs = node->getMemberUnchecked(0);
    AstNode const* rhs = node->getMemberUnchecked(1);
    AstNode const* check = nullptr;

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // const value == doc.value
        check = rhs;
      } else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // doc.value == const value
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_NE) {
      if (lhs->isNullValue() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // null != doc.value
        check = rhs;
      } else if (rhs->isNullValue() &&
                 lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // doc.value != null
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LT &&
               lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // const value < doc.value
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LE &&
               lhs->isConstant() && !lhs->isNullValue() &&
               rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // const value <= doc.value
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GT &&
               rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // doc.value > const value
      check = lhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GE &&
               rhs->isConstant() && !rhs->isNullValue() &&
               lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // doc.value >= const value
      check = lhs;
    }

    if (check == nullptr) {
      // condition is useless for us
      return;
    }

    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
        result;

    if (check->isAttributeAccessForVariable(result, false) &&
        result.first == variable) {
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        // found a constant value
        constAttributes.emplace_back(std::move(result.second));
      } else {
        // all other cases handle non-null attributes
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
    // resolve all FILTER variables into their appropriate filter conditions
    TRI_ASSERT(!_filters.empty());
    for (auto const& filter : _filters.back()) {
      TRI_ASSERT(filter.isRegularRegister());
      auto it = _variableDefinitions.find(filter.value());
      if (it != _variableDefinitions.end()) {
        // AND-combine all filter conditions we found, and fill constAttributes
        // and nonNullAttributes as we go along
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
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    // figure out all attributes from the FILTER conditions that have a constant
    // value and/or that cannot be null
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    ::arangodb::containers::HashSet<
        std::vector<arangodb::basics::AttributeName>>
        nonNullAttributes;
    processCollectionAttributes(enumerateCollectionNode->outVariable(),
                                constAttributes, nonNullAttributes);

    SortCondition sortCondition(_plan, _sorts, constAttributes,
                                nonNullAttributes, _variableDefinitions);

    if (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess()) {
      // we have found a sort condition
      // now check if any of the collection's indexes covers it

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
        // If this bit is set, then usedIndexes has length exactly one
        // and contains the best index found.
        auto condition = std::make_unique<Condition>(_plan->getAst());
        condition->normalize(_plan);
        TRI_ASSERT(usedIndexes.size() == 1);
        IndexIteratorOptions opts;
        TRI_ASSERT(!sortCondition.isEmpty());
        opts.ascending = sortCondition.sortFields()[0].asc;
        opts.useCache = false;
        auto indexNode = _plan->createNode<IndexNode>(
            _plan, _plan->nextId(), enumerateCollectionNode->collection(),
            outVariable, usedIndexes,
            false,  // here we could always assume false as there is no lookup
                    // condition here
            std::move(condition), opts);

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

    return true;  // always abort further searching here
  }

  void deleteSortNode(IndexNode* indexNode, SortCondition& sortCondition) {
    // sort condition is fully covered by index... now we can remove the
    auto sortNode = _plan->getNodeById(_sortNode->id());
    _plan->unlinkNode(sortNode);
    // we need to have a sorted result later on, so we will need a sorted
    // GatherNode in the cluster
    indexNode->needsGatherNodeSort(
        makeIndexSortElements(sortCondition, indexNode->outVariable()));
    _modified = true;
  }

  bool handleIndexNode(IndexNode* indexNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (indexNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
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
      // can only use this index node if it uses exactly one index or multiple
      // indexes on exactly the same attributes

      if (!cond->isSorted()) {
        // index conditions do not guarantee sortedness
        return true;
      }

      if (isSparse) {
        return true;
      }

      for (auto& idx : indexes) {
        if (idx != index) {
          // Can only be sorted iff only one index is used.
          return true;
        }
      }

      // all indexes use the same attributes and index conditions guarantee
      // sorted output
    }

    TRI_ASSERT(indexes.size() == 1 || cond->isSorted());

    // if we get here, we either have one index or multiple indexes on the same
    // attributes

    if (indexes.size() == 1 && isSorted) {
      // if we have just a single index and we can use it for the filtering
      // condition, then we can use the index for sorting, too. regardless of if
      // the index is sparse or not. because the index would only return
      // non-null attributes anyway, so we do not need to care about null values
      // when sorting here
      isSparse = false;
    }

    SortCondition sortCondition(
        _plan, _sorts, cond->getConstAttributes(outVariable, !isSparse),
        cond->getNonNullAttributes(outVariable), _variableDefinitions);

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    // FIXME: why not just call index->supportsSortCondition here always?
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
        // special case... the index cannot be used for sorting, but we only
        // compare with equality lookups.
        // now check if the equality lookup attributes are the same as
        // the index attributes
        auto root = cond->root();

        if (root != nullptr) {
          auto condNode = root->getMember(0);

          if (condNode->isOnlyEqualityMatch()) {
            // now check if the index fields are the same as the sort condition
            // fields e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1,
            // c.value2
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

    return true;  // always abort after we found an IndexNode
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
        // found some other FOR loop
        return true;

      case EN::SUBQUERY: {
        _filters.emplace_back();
        return false;  // skip. we don't care.
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
      case EN::LIMIT:  // LIMIT is criterion to stop
      case EN::ENUMERATE_NEAR_VECTORS:
        return true;  // abort.

      case EN::SORT:  // pulling two sorts together is done elsewhere.
        if (!_sorts.empty() || _sortNode != nullptr) {
          return true;  // a different SORT node. abort
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
        // should not reach this point
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

/// @brief useIndex, try to use an index for filtering
void arangodb::aql::useIndexesRule(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const& rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findEndNodes(nodes, true);

  std::unordered_map<ExecutionNodeId, ExecutionNode*> changes;

  auto cleanupChanges = scopeGuard([&changes]() noexcept {
    for (auto& v : changes) {
      delete v.second;
    }
  });

  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    ConditionFinder finder(plan.get(), changes);
    n->walk(finder);
    if (finder.producesEmptyResult()) {
      hasEmptyResult = true;
    }
  }

  if (!changes.empty()) {
    for (auto& it : changes) {
      plan->registerNode(it.second);
      plan->replaceNode(plan->getNodeById(it.first), it.second);

      // prevent double deletion by cleanupChanges()
      it.second = nullptr;
    }
    opt->addPlan(std::move(plan), rule, true);
  } else {
    opt->addPlan(std::move(plan), rule, hasEmptyResult);
  }
}

void arangodb::aql::useIndexForSortRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
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

/// @brief try to remove filters which are covered by indexes
void arangodb::aql::removeFiltersCoveredByIndexRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
  bool modified = false;
  // this rule may modify the plan in place, but the new plan
  // may not yet be optimal. so we may pass it into this same
  // rule again. the default is to continue with the next rule
  // however
  bool rerun = false;

  for (auto const& node : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = ExecutionNode::castTo<IndexNode*>(current);

        // found an index node, now check if the expression is covered by the
        // index
        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && !indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            // single index. this is something that we can handle
            AstNode* newNode{nullptr};
            if (!indexNode->isAllCoveredByOneIndex()) {
              if (n != 1) {
                // either no condition or multiple ORed conditions
                // and index has not covered entire condition.
                break;
              }
              newNode = condition.removeIndexCondition(
                  plan.get(), indexNode->outVariable(), indexCondition->root(),
                  indexesUsed[0].get());
            }
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it is
              // still used by other nodes in the plan
              modified = true;
              handled = true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              CalculationNode* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
              // pass the new plan into this rule again, to optimize even
              // further
              rerun = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  if (rerun) {
    TRI_ASSERT(modified);
    opt->addPlanAndRerun(std::move(plan), rule, modified);
  } else {
    opt->addPlan(std::move(plan), rule, modified);
  }
}
