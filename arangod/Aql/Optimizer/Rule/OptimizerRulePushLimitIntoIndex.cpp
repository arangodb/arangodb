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
#include "Logger/LogMacros.h"

#include <algorithm>
#include <optional>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

bool isEligibleIndex(transaction::Methods::IndexHandle const& idx) {
  // we only care about persistent indexes.
  // note that "hash" and "skiplist" indexes are the same as persistent
  // indexes under the hood, just with legacy naming.
  if (idx->type() != Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX &&
      idx->type() != Index::IndexType::TRI_IDX_TYPE_HASH_INDEX &&
      idx->type() != Index::IndexType::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    LOG_DEVEL << __LINE__;
    return false;
  }

  // we only care about compound indexes
  if (idx->fields().size() < 2) {
    LOG_DEVEL << __LINE__;
    return false;
  }

  return true;
}

bool isEligibleSort(auto itIndex, auto const itIndexEnd, auto itSort,
                    auto const itSortEnd, const auto* outVariable,
                    auto* executionPlan, auto* indexNode) {
  std::optional<bool> sortAscending;

  while (itIndex != itIndexEnd && itSort != itSortEnd) {
    LOG_DEVEL << "COMPARING: index: " << *itIndex
              << ", sort: " << itSort->attributePath;

    if (itSort->var != nullptr) {
      LOG_DEVEL << "VAR is not nullptr ";
      auto* executionNode = executionPlan->getVarSetBy(itSort->var->id);
      if (executionNode == nullptr) {
        LOG_DEVEL << "Execution plan must have a var";
        return false;
      }

      if (executionNode->getType() != EN::CALCULATION) {
        LOG_DEVEL << "It is not a calc node";
        return false;
      }

      auto calculationNode =
          ExecutionNode::castTo<CalculationNode const*>(executionNode);
      auto* rootNode = calculationNode->expression()->node();
      std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
          varVectorAttributePair;
      if (!rootNode->isAttributeAccessForVariable(varVectorAttributePair,
                                                  false)) {
        LOG_DEVEL << "It is not attribtie for variable";
        return false;
      }
      // Stop optimization
      if (varVectorAttributePair.first != indexNode->outVariable()) {
        LOG_DEVEL << "Output var not equal";
        return false;
      }

      LOG_DEVEL << "ASASASSA";
      LOG_DEVEL << varVectorAttributePair.second;
      LOG_DEVEL << *itIndex;
      if (varVectorAttributePair.second != *itIndex) {
        LOG_DEVEL << "attribute path not eq";
        return false;
      }

    } else {
      if (itIndex->size() != itSort->attributePath.size()) {
        // ["a"] == ["a"]   vs.  ["b"] != ["b", "sub"]
        LOG_DEVEL << __LINE__;
        return false;
      }
      if (std::equal(itIndex->begin(), itIndex->end(),
                     itSort->attributePath.begin(), itSort->attributePath.end(),
                     [](auto const& lhs, auto const& rhs) {
                       return lhs.name == rhs;
                     })) {
        LOG_DEVEL << __LINE__;
        return false;
      }

      if (itSort->var != outVariable) {
        // we are sorting by something unrelated to the index
        LOG_DEVEL << __LINE__;
        return false;
      }
    }

    if (std::any_of(itIndex->begin(), itIndex->end(),
                    [](auto const& it) { return it.shouldExpand; })) {
      LOG_DEVEL << __LINE__;
      return false;
    }

    if (!sortAscending.has_value()) {
      // note first used sort order
      sortAscending = itSort->ascending;
    } else if (*sortAscending != itSort->ascending) {
      // different sort orders used
      LOG_DEVEL << __LINE__;
      return false;
    }

    ++itIndex;
    ++itSort;
  }
  return true;
}

void arangodb::aql::pushLimitIntoIndexRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> indexes;
  plan->findNodesOfType(indexes, EN::INDEX, /* enterSubqueries */ true);

  for (auto* index : indexes) {
    TRI_ASSERT(index->getType() == EN::INDEX);
    auto* indexNode = ExecutionNode::castTo<IndexNode*>(index);

    // Check that there is no post filtering
    if (indexNode->hasFilter()) {
      continue;
    }

    // The condition of IndexNode can only be a single `compare in` for the
    // rule to be applicable
    // The tree has a specific format => DNF therefore the assumption
    // of having only one members and expecting COMPARE IN node
    // is valid
    auto* compareInNode = indexNode->condition() == nullptr
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
      LOG_DEVEL << "Single in operator not found ";
      continue;
    }

    // Cannot apply rule if in loop
    if (indexNode->isInInnerLoop()) {
      continue;
    }

    // remember the output variable that is produced by the IndexNode
    Variable const* outVariable = indexNode->outVariable();

    auto const& indexes = indexNode->getIndexes();
    if (indexes.size() != 1) {
      // IndexNode uses more than a single index. not safe to apply the
      // optimization here.
      LOG_DEVEL << __LINE__;
      continue;
    }
    auto const& usedIndex = indexes.front();
    if (!isEligibleIndex(usedIndex)) {
      LOG_DEVEL << __LINE__;
      continue;
    }

    TRI_ASSERT(compareInNode->numMembers() == 2);
    if (auto* lhs = compareInNode->getMember(0);
        lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::vector<std::string> fullPath;
      auto currentMember = lhs;
      while (currentMember != nullptr &&
             currentMember->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        fullPath.push_back(currentMember->getString());

        currentMember = currentMember->getMember(0);
      }
      if (currentMember->type != NODE_TYPE_REFERENCE) {
        continue;
      }

      auto* variable = static_cast<Variable const*>(currentMember->getData());
      if (outVariable != variable) {
        continue;
      }

      // doc.license.foo IN ["license", "license2"]
      // FULL PATH [foo, license]
      // indexFields:
      // - [license.foo]
      // - [date_created]
      LOG_DEVEL << "FULL PAHT " << fullPath;
      if (!std::equal(fullPath.rbegin(), fullPath.rend(),
                      usedIndex->fields().front().begin(),
                      usedIndex->fields().front().end(),
                      [](const auto& lhs, const auto& rhs) {
                        return lhs == rhs.name;
                      })) {
        LOG_DEVEL << "Comparison between path of full path and index first "
                     "attribute full path fails";
        continue;
      }
    }

    // Check if index node parents contains a pair of sort and limit
    auto* sortNode = indexNode->getFirstParent();
    while (sortNode != nullptr && sortNode->getType() == EN::CALCULATION) {
      sortNode = sortNode->getFirstParent();
    }

    if (sortNode == nullptr || sortNode->getType() != EN::SORT) {
      LOG_DEVEL << __LINE__;
      continue;
    }

    auto const& sortFields =
        ExecutionNode::castTo<SortNode const*>(sortNode)->elements();
    LOG_DEVEL << "SORTFIELDS: " << sortFields.size();
    for (auto const& x : sortFields) {
      LOG_DEVEL << " - " << x.toString();
      LOG_DEVEL << x.attributePath;
      LOG_DEVEL << x.var;
    }
    // SORT doc.a ASC, doc.b DESC, foo.bar.baz ASC
    // [
    //   {var: doc, ascending: true, attributePath: ["a"]},
    //   {var: doc, ascending: false, attributePath: ["b"]},
    //   {var: foo, ascending: true, attributePath: ["bar", "baz"]},
    // ]

    if (!isEligibleSort(usedIndex->fields().begin(), usedIndex->fields().end(),
                        sortFields.begin(), sortFields.end(), outVariable,
                        plan.get(), indexNode) &&
        !isEligibleSort(usedIndex->fields().begin() + 1,
                        usedIndex->fields().end(), sortFields.begin(),
                        sortFields.end(), outVariable, plan.get(), indexNode)) {
      LOG_DEVEL << __LINE__;
      continue;
    }

    ExecutionNode* limitNode = sortNode->getFirstParent();
    if (limitNode == nullptr || limitNode->getType() != EN::LIMIT) {
      LOG_DEVEL << __LINE__;
      continue;
    }

    indexNode->setLimit(ExecutionNode::castTo<LimitNode*>(limitNode)->limit());
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
