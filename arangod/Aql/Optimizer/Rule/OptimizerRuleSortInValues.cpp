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

#include "OptimizerRuleSortInValues.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/ScopeGuard.h"
#include "Containers/SmallVector.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief adds a SORT operation for IN right-hand side operands
void arangodb::aql::sortInValuesRule(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;

  for (auto const& n : nodes) {
    // now check who introduced our variable
    auto variable = ExecutionNode::castTo<FilterNode const*>(n)->inVariable();
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced by a CalculationNode. now check the
    // expression
    auto s = ExecutionNode::castTo<CalculationNode*>(setter);
    auto filterExpression = s->expression();
    auto* inNode = filterExpression->nodeForModification();

    TRI_ASSERT(inNode != nullptr);

    // check the filter condition
    if ((inNode->type != NODE_TYPE_OPERATOR_BINARY_IN &&
         inNode->type != NODE_TYPE_OPERATOR_BINARY_NIN) ||
        !inNode->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    ast::RelationalOperatorNode inOp(inNode);
    auto rhs = inOp.getRight();

    if (rhs->type != NODE_TYPE_REFERENCE && rhs->type != NODE_TYPE_ARRAY) {
      continue;
    }

    auto loop = n->getLoop();

    if (loop == nullptr) {
      // FILTER is not used inside a loop. so it will be used at most once
      // not need to sort the IN values then
      continue;
    }

    if (rhs->type == NODE_TYPE_ARRAY) {
      if (rhs->numMembers() < AstNode::kSortNumberThreshold ||
          rhs->isSorted()) {
        // number of values is below threshold or array is already sorted
        continue;
      }

      auto ast = plan->getAst();
      auto args = ast->createNodeArray();
      args->addMember(rhs);
      auto sorted = ast->createNodeFunctionCall("SORTED_UNIQUE", args, true);
      inNode->changeMember(1, sorted);
      modified = true;
      continue;
    }

    variable = static_cast<Variable const*>(rhs->getData());
    setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || (setter->getType() != EN::CALCULATION &&
                              setter->getType() != EN::SUBQUERY)) {
      // variable itself was not introduced by a calculation.
      continue;
    }

    if (loop == setter->getLoop()) {
      // the FILTER and its value calculation are contained in the same loop
      // this means the FILTER will be executed as many times as its value
      // calculation. sorting the IN values will not provide a benefit here
      continue;
    }

    auto ast = plan->getAst();
    AstNode const* originalArg = nullptr;

    if (setter->getType() == EN::CALCULATION) {
      AstNode const* originalNode =
          ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
      TRI_ASSERT(originalNode != nullptr);

      AstNode const* testNode = originalNode;

      if (originalNode->type == NODE_TYPE_FCALL &&
          static_cast<Function const*>(originalNode->getData())
              ->hasFlag(Function::Flags::NoEval)) {
        // bypass NOOPT(...) for testing
        TRI_ASSERT(originalNode->numMembers() == 1);
        ast::FunctionCallNode fcall(originalNode);
        auto args = fcall.getArguments();

        if (args->numMembers() > 0) {
          testNode = args->getMemberUnchecked(0);
        }
      }

      if (testNode->type == NODE_TYPE_VALUE ||
          testNode->type == NODE_TYPE_OBJECT) {
        // not really usable...
        continue;
      }

      if (testNode->type == NODE_TYPE_ARRAY &&
          testNode->numMembers() < AstNode::kSortNumberThreshold) {
        // number of values is below threshold
        continue;
      }

      if (testNode->type == NODE_TYPE_FCALL &&
          (static_cast<Function const*>(testNode->getData())->name ==
               "SORTED_UNIQUE" ||
           static_cast<Function const*>(testNode->getData())->name ==
               "SORTED")) {
        // we don't need to sort results of a function that already returns
        // sorted results

        AstNode* clone = ast->shallowCopyForModify(inNode);
        auto sg =
            arangodb::scopeGuard([&]() noexcept { FINALIZE_SUBTREE(clone); });
        // set sortedness bit for the IN operator
        clone->setBoolValue(true);
        // finally adjust the variable inside the IN calculation
        filterExpression->replaceNode(clone);
        continue;
      }

      if (testNode->isSorted()) {
        // already sorted
        continue;
      }

      originalArg = originalNode;
    } else {
      TRI_ASSERT(setter->getType() == EN::SUBQUERY);
      auto sub = ExecutionNode::castTo<SubqueryNode*>(setter);

      // estimate items in subquery
      CostEstimate estimate = sub->getSubquery()->getCost();

      if (estimate.estimatedNrItems < AstNode::kSortNumberThreshold) {
        continue;
      }

      originalArg = ast->createNodeReference(sub->outVariable());
    }

    TRI_ASSERT(originalArg != nullptr);

    auto args = ast->createNodeArray();
    args->addMember(originalArg);
    auto sorted = ast->createNodeFunctionCall("SORTED_UNIQUE", args, true);

    auto outVar = ast->variables()->createTemporaryVariable();
    auto expression = std::make_unique<Expression>(ast, sorted);
    ExecutionNode* calculationNode = plan->createNode<CalculationNode>(
        plan.get(), plan->nextId(), std::move(expression), outVar);

    // make the new node a parent of the original calculation node
    TRI_ASSERT(setter != nullptr);
    calculationNode->addDependency(setter);
    auto oldParent = setter->getFirstParent();
    TRI_ASSERT(oldParent != nullptr);
    calculationNode->addParent(oldParent);

    oldParent->removeDependencies();
    oldParent->addDependency(calculationNode);
    setter->setParent(calculationNode);

    AstNode* clone = ast->shallowCopyForModify(inNode);
    auto sg = arangodb::scopeGuard([&]() noexcept { FINALIZE_SUBTREE(clone); });
    // set sortedness bit for the IN operator
    clone->setBoolValue(true);
    // finally adjust the variable inside the IN calculation
    clone->changeMember(1, ast->createNodeReference(outVar));
    filterExpression->replaceNode(clone);

    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}