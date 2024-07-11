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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/SortElement.h"
#include "Aql/Variable.h"
#include "Indexes/Index.h"

#include <algorithm>
#include <optional>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// index is eligible only if it is a type of persistent index
/// and if it build on multiple attributes
bool isEligibleIndex(transaction::Methods::IndexHandle const& idx) {
  // we only care about persistent indexes.
  // note that "hash" and "skiplist" indexes are the same as persistent
  // indexes under the hood, just with legacy naming.
  if (idx->type() != Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX &&
      idx->type() != Index::IndexType::TRI_IDX_TYPE_HASH_INDEX &&
      idx->type() != Index::IndexType::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    return false;
  }

  // we only care about compound indexes
  if (idx->fields().size() < 2) {
    return false;
  }

  return true;
}

/// optimization can be applied if the sorting attributes being sorted match
/// the index attributes, and they are all in the same order (ASC/DESC)
bool isEligibleSort(auto itIndex, auto const itIndexEnd, auto const& sortFields,
                    auto const* executionPlan, auto const* indexNode) {
  std::optional<bool> sortAscending;
  auto itSort = sortFields.begin();

  while (itIndex != itIndexEnd && itSort != sortFields.end()) {
    auto const* sortVar = itSort->var;

    if (!itSort->attributePath.empty()) {
      return false;
    }

    // to get the attributes of SortNode CalculationNode is needed
    auto const* executionNode = executionPlan->getVarSetBy(sortVar->id);
    if (executionNode == nullptr) {
      return false;
    }
    if (executionNode->getType() != EN::CALCULATION) {
      return false;
    }
    auto const* calculationNode =
        ExecutionNode::castTo<CalculationNode const*>(executionNode);

    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
        attributeAccessResult;
    auto const* calculationNodeExpression = calculationNode->expression();
    if (calculationNodeExpression != nullptr &&
        calculationNodeExpression->node() != nullptr &&
        !calculationNodeExpression->node()->isAttributeAccessForVariable(
            attributeAccessResult, false)) {
      return false;
    }

    // if variable whose attributes are being accessed is not the same as
    // indexed, we cannot apply the rule
    if (attributeAccessResult.first != indexNode->outVariable()) {
      return false;
    }

    // if the attributes of variable are not the same as indexed attributes,
    // cannot apply the rule
    if (attributeAccessResult.second != *itIndex) {
      return false;
    }

    if (std::any_of(itIndex->begin(), itIndex->end(),
                    [](auto const& it) { return it.shouldExpand; })) {
      return false;
    }
    if (!sortAscending.has_value()) {
      // note first used sort order
      sortAscending = itSort->ascending;
    } else if (*sortAscending != itSort->ascending) {
      // different sort orders used
      return false;
    }

    ++itIndex;
    ++itSort;
  }

  // must exhaust every field in sort node
  if (itSort != sortFields.end()) {
    return false;
  }
  return true;
}

/// if the following conditions are met this rule will apply:
/// - there is an persistent index used
/// - IndexNode must not have post filter
/// - IndexNode must not be in the inner loop
/// - IndexNode must have a single condition containing `COMPARE IN` operator
/// - attribute in the `COMPARE IN` operator must be in the same one used in
/// index
/// - first parent of IndexNode must be SortNode (we can skip calculation node)
/// - order of attributes in SortNode must match the order of attributes of
/// index
/// - first parent of SortNode must be a LimitNode
void arangodb::aql::pushLimitIntoIndexRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> indexes;
  plan->findNodesOfType(indexes, EN::INDEX, /* enterSubqueries */ true);
  for (auto* index : indexes) {
    TRI_ASSERT(index->getType() == EN::INDEX);
    auto* indexNode = ExecutionNode::castTo<IndexNode*>(index);

    // check that there is no post filtering
    if (indexNode->hasFilter()) {
      continue;
    }

    // cannot apply rule if in loop
    if (indexNode->isInInnerLoop()) {
      continue;
    }

    // condition of IndexNode can only be a single `compare in` for the
    // rule to be applicable
    // the tree has a specific format => DNF therefore the assumption
    // of having only one member and expecting COMPARE IN node
    // is valid
    auto const* compareInNode = indexNode->condition() == nullptr
                                    ? nullptr
                                    : indexNode->condition()->root();
    while (compareInNode != nullptr) {
      if (compareInNode->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        break;
      }
      if (compareInNode->numMembers() != 1) {
        compareInNode = nullptr;
        break;
      }

      compareInNode = compareInNode->getMember(0);
    }
    if (compareInNode == nullptr) {
      continue;
    }

    auto const& indexes = indexNode->getIndexes();
    if (indexes.size() != 1) {
      // IndexNode uses more than a single index. not safe to apply the
      // optimization here.
      continue;
    }
    auto const& usedIndex = indexes.front();
    if (!isEligibleIndex(usedIndex)) {
      continue;
    }

    // remember the output variable that is produced by the IndexNode
    Variable const* outVariable = indexNode->outVariable();

    // check if `compare in` variable is same as IndexNode output,
    // and that it compares against the attribute defined in index
    if (auto const* lhs = compareInNode->getMember(0);
        lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
          attributeAccessResult;
      if (!lhs->isAttributeAccessForVariable(attributeAccessResult, false)) {
        continue;
      }
      if (attributeAccessResult.first != outVariable) {
        continue;
      }
      if (attributeAccessResult.second != *usedIndex->fields().begin()) {
        continue;
      }
    } else {
      continue;
    }

    // check if index node parents contains a pair of sort and limit.
    // skipping over any amount of CalculationNodes is valid here.
    auto const* sortNode = indexNode->getFirstParent();
    while (sortNode != nullptr && sortNode->getType() == EN::CALCULATION) {
      sortNode = sortNode->getFirstParent();
    }

    if (sortNode == nullptr || sortNode->getType() != EN::SORT) {
      continue;
    }

    auto const& sortFields =
        ExecutionNode::castTo<SortNode const*>(sortNode)->elements();
    if (!isEligibleSort(usedIndex->fields().begin(), usedIndex->fields().end(),
                        sortFields, plan.get(), indexNode) &&
        !isEligibleSort(usedIndex->fields().begin() + 1,
                        usedIndex->fields().end(), sortFields, plan.get(),
                        indexNode)) {
      continue;
    }

    auto* maybeLimitNode = sortNode->getFirstParent();
    while (maybeLimitNode != nullptr &&
           (maybeLimitNode->getType() == EN::REMOTE ||
            maybeLimitNode->getType() == EN::MATERIALIZE ||
            maybeLimitNode->getType() == EN::CALCULATION ||
            maybeLimitNode->getType() == EN::GATHER)) {
      if (maybeLimitNode->getType() == EN::GATHER) {
        auto const* gatherNode =
            ExecutionNode::castTo<GatherNode const*>(maybeLimitNode);

        if (gatherNode->isSortingGather()) {
          maybeLimitNode = nullptr;
          break;
        }
      } else if (maybeLimitNode->getType() == EN::CALCULATION) {
        auto* calculationNode =
            ExecutionNode::castTo<CalculationNode*>(maybeLimitNode);

        if (!calculationNode->isDeterministic()) {
          maybeLimitNode = nullptr;
          break;
        }
      }
      maybeLimitNode = maybeLimitNode->getFirstParent();
    }
    if (maybeLimitNode == nullptr || maybeLimitNode->getType() != EN::LIMIT) {
      continue;
    }

    auto* limitNode = ExecutionNode::castTo<LimitNode*>(maybeLimitNode);
    indexNode->setLimit(limitNode->offset() + limitNode->limit());

    indexNode->setAscending(sortFields.front().ascending);

    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
