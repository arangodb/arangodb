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

#include "MoveFiltersIntoEnumerate.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {
static constexpr std::initializer_list<ExecutionNode::NodeType> const
    moveFilterIntoEnumerateTypes{EN::ENUMERATE_COLLECTION, EN::INDEX,
                                 EN::ENUMERATE_LIST};
}  // namespace

void moveFiltersIntoEnumerateRule(Optimizer* opt,
                                  std::unique_ptr<ExecutionPlan> plan,
                                  OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, moveFilterIntoEnumerateTypes, true);

  VarSet found;
  VarSet introduced;

  for (auto const& n : nodes) {
    if (n->getType() == EN::INDEX &&
        ExecutionNode::castTo<IndexNode const*>(n)->getIndexes().size() != 1) {
      continue;
    }

    std::vector<Variable const*> outVariable;
    outVariable.resize(1);

    if (n->getType() == EN::INDEX || n->getType() == EN::ENUMERATE_COLLECTION) {
      auto en = dynamic_cast<DocumentProducingNode*>(n);
      if (en == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "unable to cast node to DocumentProducingNode");
      }

      outVariable[0] = en->outVariable();
    } else {
      TRI_ASSERT(n->getType() == EN::ENUMERATE_LIST);
      outVariable = ExecutionNode::castTo<EnumerateListNode const*>(n)
                        ->getVariablesSetHere();
    }

    bool isUsedLater = n->isVarUsedLater(outVariable[0]);
    if (outVariable.size() > 1) {
      isUsedLater |= n->isVarUsedLater(outVariable[1]);
    }
    if (!isUsedLater) {
      continue;
    }

    containers::FlatHashMap<Variable const*, CalculationNode*> calculations;
    introduced.clear();

    ExecutionNode* current = n->getFirstParent();

    while (current != nullptr) {
      if (current->getType() != EN::FILTER &&
          current->getType() != EN::CALCULATION) {
        break;
      }

      if (current->getType() == EN::FILTER) {
        if (calculations.empty()) {
          break;
        }

        auto filterNode = ExecutionNode::castTo<FilterNode*>(current);
        Variable const* inVariable = filterNode->inVariable();

        auto it = calculations.find(inVariable);
        if (it == calculations.end()) {
          break;
        }

        CalculationNode* cn = (*it).second;
        Expression* expr = cn->expression();

        auto setFilter = [&](auto* en, Expression* expr) {
          Expression* existingFilter = en->filter();
          if (existingFilter != nullptr && existingFilter->node() != nullptr) {
            AstNode* merged = plan->getAst()->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_AND, existingFilter->node(),
                expr->node());

            en->setFilter(std::make_unique<Expression>(plan->getAst(), merged));
          } else {
            en->setFilter(expr->clone(plan->getAst()));
          }
        };

        if (n->getType() == EN::INDEX ||
            n->getType() == EN::ENUMERATE_COLLECTION) {
          auto en = dynamic_cast<DocumentProducingNode*>(n);
          TRI_ASSERT(en != nullptr);
          setFilter(en, expr);
        } else {
          TRI_ASSERT(n->getType() == EN::ENUMERATE_LIST);
          setFilter(ExecutionNode::castTo<EnumerateListNode*>(n), expr);
        }

        ExecutionNode* filterParent = current->getFirstParent();
        TRI_ASSERT(filterParent != nullptr);
        plan->unlinkNode(current);

        if (!current->isVarUsedLater(cn->outVariable())) {
          plan->unlinkNode(cn);
        }

        current = filterParent;
        modified = true;
        continue;
      } else if (current->getType() == EN::CALCULATION) {
        TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
        auto calculationNode = ExecutionNode::castTo<CalculationNode*>(current);
        auto expr = calculationNode->expression();
        if (!expr->isDeterministic() ||
            !expr->canRunOnDBServer(vocbase.isOneShard())) {
          break;
        }

        TRI_ASSERT(!expr->willUseV8());
        found.clear();
        Ast::getReferencedVariables(expr->node(), found);

        bool isFound = found.contains(outVariable[0]);
        if (outVariable.size() > 1) {
          isFound |= found.contains(outVariable[1]);
        }
        if (isFound) {
          bool eligible = std::none_of(
              introduced.begin(), introduced.end(),
              [&](Variable const* temp) { return found.contains(temp); });

          if (eligible) {
            calculations.emplace(calculationNode->outVariable(),
                                 calculationNode);
          }
        }

        introduced.emplace(calculationNode->outVariable());
      }

      current = current->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
