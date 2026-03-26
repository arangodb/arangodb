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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstHelper.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Rule/OptimizerRuleVectorIndexHelpers.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Indexes/Index.h"

namespace arangodb::aql {

using EN = ExecutionNode;

#define LOG_RULE_ENABLED false
#define LOG_RULE_IF(cond) LOG_DEVEL_IF((LOG_RULE_ENABLED) && (cond))
#define LOG_RULE LOG_RULE_IF(true)

std::unique_ptr<Expression> tryRemoveFilterNode(
    auto* maybeFilterNode, auto& plan,
    auto const* enumerateNearVectorOutputDocument) {
  auto const* filterNode =
      ExecutionNode::castTo<FilterNode const*>(maybeFilterNode);
  auto const* filterInVar = filterNode->inVariable();

  // Find the calculation node that populates the filter variable
  auto const* maybeCalculationNode = plan->getVarSetBy(filterInVar->id);
  if (maybeCalculationNode == nullptr ||
      maybeCalculationNode->getType() != EN::CALCULATION) {
    return nullptr;
  }

  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(maybeCalculationNode);

  // Check that all variables used in filterExpression can be handled in
  // EnumerateNearVector node
  VarSet calculationVars;
  auto filterExpression = calculationNode->expression()->clone(plan->getAst());
  filterExpression->variables(calculationVars);

  for (auto const* calcVar : calculationVars) {
    if (calcVar->type() != Variable::Type::Regular) {
      continue;
    }
    if (calcVar != enumerateNearVectorOutputDocument) {
      return nullptr;
    }
  }

  // CalculationNode will be removed by the subsequent rule if it is not
  // referenced by any other node
  plan->unlinkNode(maybeFilterNode);

  return filterExpression;
}

bool areAllAttributesCovered(
    std::unique_ptr<ExecutionPlan> const& plan,
    std::unique_ptr<Expression> const& filterExpression,
    EnumerateNearVectorNode const* enumerateNearVectorNode,
    std::vector<std::vector<basics::AttributeName>> const& storedValues) {
  containers::FlatHashSet<aql::AttributeNamePath> filterAttributes;
  Ast::getReferencedAttributesRecursive(
      filterExpression->node(), enumerateNearVectorNode->documentOutVariable(),
      "", filterAttributes, plan->getAst()->query().resourceMonitor());

  for (auto const& attr : filterAttributes) {
    auto const matchesAttribute =
        [&attr](std::vector<basics::AttributeName> const& elem) {
          if (attr.size() != elem.size()) {
            return false;
          }
          for (size_t i = 0; i < attr.size(); ++i) {
            if (elem[i].shouldExpand) {
              return false;
            }
            if (attr[i] != elem[i].name) {
              return false;
            }
          }
          return true;
        };
    if (auto const it =
            std::ranges::find_if(storedValues, std::move(matchesAttribute));
        it == storedValues.end()) {
      return false;
    }
  }

  return true;
}

// Find a compatible vector index whose storedValues cover the filter
// expression. We consider all vector indexes on the collection, not just the
// currently assigned one -- a different index with the right storedValues can
// avoid a full document materialization.
std::shared_ptr<Index> findBestVectorIndex(
    std::unique_ptr<ExecutionPlan> const& plan,
    std::unique_ptr<Expression> const& filterExpression,
    EnumerateNearVectorNode const* enumerateNearVectorNode) {
  auto const& currentIndex = enumerateNearVectorNode->index();
  bool ascending = enumerateNearVectorNode->isAscending();
  auto const& allIndexes = enumerateNearVectorNode->collection()->indexes();

  for (auto const& candidate : allIndexes) {
    if (!isCompatibleVectorIndex(candidate, currentIndex, ascending)) {
      continue;
    }
    auto const& storedValues = candidate->storedValues();
    if (storedValues.empty()) {
      LOG_RULE << "Index " << candidate->name()
               << " has no storedValues, skipping";
      continue;
    }
    if (!areAllAttributesCovered(plan, filterExpression,
                                 enumerateNearVectorNode, storedValues)) {
      LOG_RULE << "Index " << candidate->name()
               << " storedValues don't cover filter";
      continue;
    }
    return candidate;
  }

  return nullptr;
}

// This rule check if EnumerateNearVectorNode has a FilterNode and if so tries
// to remove it and apply early pruning in EnumerateNearVectorNode.
// Also we check if we can use storedFields optimization with the given index.
//
// If we have found both EnumerateNearVectorNode and Filter node we must
// be able to push filter expression into EnumerateNearVectorNode if we cannot
// then we throw since we do not support post filtering with vector index
void pushFilterIntoEnumerateNear(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  bool modified{false};

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, true);
  for (ExecutionNode* node : nodes) {
    auto* enumerateNearVectorNode =
        ExecutionNode::castTo<EnumerateNearVectorNode*>(node);

    // The use-vector-index rule inserts MaterializeNode after the
    // EnumerateNearVectorNode
    auto* currentNode = enumerateNearVectorNode->getFirstParent();
    while (currentNode != nullptr &&
           (currentNode->getType() == EN::CALCULATION ||
            currentNode->getType() == EN::MATERIALIZE)) {
      currentNode = currentNode->getFirstParent();
    }

    // We expect filter node since this rules is enabled only when
    // vector index was used and we see a filter node
    if (currentNode == nullptr || currentNode->getType() != EN::FILTER) {
      LOG_RULE << ADB_HERE << ": No filter node";
      continue;
    }
    ExecutionNode* filterNode = currentNode;
    currentNode = currentNode->getFirstParent();

    // If there is a FilterNode it comes with CalculationNode, we remove it
    // and handle it in EnumerateNearVectorNode
    std::unique_ptr<Expression> filterExpression = tryRemoveFilterNode(
        filterNode, plan, enumerateNearVectorNode->documentOutVariable());
    if (!filterExpression) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
          "filter node could not be moved into EnumerateNearVector node!");
    }
    // We are handling filtering in EnumerateNearVectorNode
    enumerateNearVectorNode->setFilterExpression(filterExpression.get());
    modified = true;

    if (auto bestIndex = findBestVectorIndex(plan, filterExpression,
                                             enumerateNearVectorNode);
        bestIndex != nullptr) {
      if (bestIndex != enumerateNearVectorNode->index()) {
        enumerateNearVectorNode->setIndex(std::move(bestIndex));
      }
      enumerateNearVectorNode->setIsCoveredByStoredValues(true);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
