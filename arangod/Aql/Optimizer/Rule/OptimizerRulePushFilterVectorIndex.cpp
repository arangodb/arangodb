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
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Assertions/Assert.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {
bool removeFilterAndCalculationNode(auto* maybeFilterNode, auto& plan,
                                    auto& filterExpression) {
  auto* maybeCalculationNode = maybeFilterNode->getFirstDependency();
  // We always expect a calculationNode in this scenario
  if (maybeCalculationNode == nullptr ||
      maybeCalculationNode->getType() != EN::CALCULATION) {
    return false;
  }
  auto const* calculationNode =
      ExecutionNode::castTo<CalculationNode const*>(maybeCalculationNode);
  TRI_ASSERT(calculationNode->expression() != nullptr);
  filterExpression = calculationNode->expression()->clone(plan->getAst());

  plan->unlinkNode(maybeFilterNode);
  plan->unlinkNode(maybeCalculationNode);

  return true;
}

}  // namespace

void arangodb::aql::pushFilterIntoEnumerateNear(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified{false};

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, true);
  for (ExecutionNode* node : nodes) {
    auto* enumerateNearVectorNode =
        ExecutionNode::castTo<EnumerateNearVectorNode*>(node);

    auto* currentNode = enumerateNearVectorNode->getFirstParent();
    const auto skipOverCalculationNodes = [&currentNode] {
      while (currentNode != nullptr &&
             currentNode->getType() == EN::CALCULATION) {
        currentNode = currentNode->getFirstParent();
      }
    };
    skipOverCalculationNodes();

    // We expect filter node
    if (currentNode == nullptr || currentNode->getType() != EN::FILTER) {
      continue;
    }
    ExecutionNode* filterNode = currentNode;
    currentNode = currentNode->getFirstParent();

    // If there is a FilterNode it comes with CalculationNode, we remove it
    // and handle it in EnumerateNearVectorNode
    // TODO Check if IndexNode always does this
    std::unique_ptr<Expression> filterExpression{nullptr};
    bool isCoveredByStoredValues{false};
    if (bool isRemoved =
            removeFilterAndCalculationNode(filterNode, plan, filterExpression);
        !isRemoved) {
      continue;
    }

    // check which variables are used by the node's post-filter
    std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
    VarSet inVars;
    filterExpression->variables(inVars);
    filterVarsToRegs.reserve(inVars.size());

    auto const* oldDocumentVariable =
        enumerateNearVectorNode->documentOutVariable();
    // Here we take all variables in the expression
    for (auto const& var : inVars) {
      TRI_ASSERT(var != nullptr);
      if (var->id == oldDocumentVariable->id) {
        continue;
      }
      // TODO(jbajic) Why I cannot get this from the execution plan and not
      // from the node?
      auto const regId =
          filterNode->getRegisterPlan()->variableToRegisterId(var);

      filterVarsToRegs.emplace_back(var->id, regId);
    }

    auto const* vecIdx = reinterpret_cast<RocksDBVectorIndex*>(
        enumerateNearVectorNode->index().get());
    if (vecIdx->hasStoredValues() && !filterVarsToRegs.empty()) {
      // check if all attribtues in filterExpression are covered
      // by storedValues

      // Get stored values from the vector index
      auto const& storedValues = vecIdx->storedValues();

      // Extract all attribute names from filter expression
      containers::FlatHashSet<aql::AttributeNamePath> filterAttributes;
      // auto* ast = engine.getQuery().ast();
      auto* ast = plan->getAst();

      for (auto const& varPair : filterVarsToRegs) {
        auto const* variable = ast->variables()->getVariable(varPair.first);
        if (variable != nullptr) {
          // Extract attribute names for this variable from the filter
          // expression
          if (!Ast::getReferencedAttributesRecursive(
                  filterExpression->node(), variable,
                  /*expectedAttribute*/ "", filterAttributes,
                  plan->getAst()->query().resourceMonitor())) {
            // If we can't extract attributes, we can't use stored values
            break;
          }
        }
      }

      // Check if all filter attributes are covered by stored values
      isCoveredByStoredValues = true;
      for (auto const& filterAttr : filterAttributes) {
        bool found = false;
        for (auto const& storedValue : storedValues) {
          // Convert stored value to AttributeNamePath for comparison
          aql::AttributeNamePath storedPath(
              plan->getAst()->query().resourceMonitor());
          for (auto const& attrName : storedValue) {
            storedPath.add(attrName.name);
          }

          if (filterAttr == storedPath) {
            found = true;
            break;
          }
        }
        if (!found) {
          isCoveredByStoredValues = false;
          break;
        }
      }
      enumerateNearVectorNode->setFilterExpression(filterExpression.get());
      enumerateNearVectorNode->setIsCoveredByStoredValues(
          isCoveredByStoredValues);

      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
