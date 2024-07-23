////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

/// @brief replace enumeration of ENTRIES with object iteration
void aql::replaceEntriesWithObjectIteration(Optimizer* opt,
                                            std::unique_ptr<ExecutionPlan> plan,
                                            OptimizerRule const& rule) {
  bool modified = false;

  SmallVector<ExecutionNode*, 8> enumerationListNodes;
  SmallVector<ExecutionNode*, 8> calculationNodes;

  plan->findNodesOfType(enumerationListNodes, EN::ENUMERATE_LIST,
                        /* enterSubqueries */ true);
  plan->findNodesOfType(calculationNodes, EN::CALCULATION,
                        /* enterSubqueries */ true);

  // We store here EnumerateListNode which takes as input CalculationNode. This
  // Calculation node is "ENTRIES" function.
  SmallVector<
      std::tuple<EnumerateListNode*, const CalculationNode*, const Expression*>,
      8>
      enumAndCalc;

  for (auto elNodeRaw : enumerationListNodes) {
    auto eln = ExecutionNode::castTo<EnumerateListNode*>(elNodeRaw);
    auto enumInVariable = eln->inVariable();

    for (auto calcNodeRaw : calculationNodes) {
      auto cn = ExecutionNode::castTo<CalculationNode*>(calcNodeRaw);

      if (enumInVariable->isEqualTo(*cn->outVariable())) {
        auto exp = cn->expression();
        if (exp->node()->type == NODE_TYPE_FCALL) {
          auto func = static_cast<Function const*>(exp->node()->getData());
          if (func->name == "ENTRIES") {
            // potentially subject to optimize
            enumAndCalc.push_back(std::make_tuple(eln, cn, exp));
            break;
          }
        }
      }
    }
  }

  auto getCalcNodesWithVar = [&calculationNodes](const Variable* reqVar)
      -> std::tuple<bool, SmallVector<CalculationNode*, 2>> {
    std::array<int, 2> indexes{0, 0};
    SmallVector<CalculationNode*, 2> keyValueNodes;
    keyValueNodes.resize(2);

    for (auto n : calculationNodes) {
      auto cn = ExecutionNode::castTo<CalculationNode*>(n);
      auto exp = cn->expression()->node();
      if (exp->type == NODE_TYPE_INDEXED_ACCESS) {
        // <something>[<some index>]
        // myVariable[<some index>]
        auto indexedValue = exp->getMemberUnchecked(0);
        if (indexedValue->type == NODE_TYPE_REFERENCE) {
          Variable const* v =
              static_cast<Variable const*>(indexedValue->getData());
          if (reqVar->isEqualTo(*v)) {
            auto indexValue = exp->getMemberUnchecked(1);
            if (indexedValue->isConstant()) {
              int val = indexValue->getIntValue();
              if (!indexes[val]) {
                return std::make_tuple(false, keyValueNodes);
              }
              indexes[val] = 1;
              keyValueNodes[val] = cn;
            }
          }
        }
      }
    }

    return std::make_tuple(indexes[0] && indexes[1], std::move(keyValueNodes));
  };

  for (auto p : enumAndCalc) {
    EnumerateListNode* eln;
    const CalculationNode* cn;
    const Expression* exp;
    std::tie(eln, cn, exp) = p;

    auto outVar = cn->outVariable();
    bool optRequired;
    SmallVector<CalculationNode*, 2> keyValueNodes;

    std::tie(optRequired, keyValueNodes) = getCalcNodesWithVar(outVar);
    if (optRequired) {
      // Optimization Step
      eln->setEnumerateObject(keyValueNodes[0]->outVariable(),
                              keyValueNodes[1]->outVariable());
      plan->unlinkNode(keyValueNodes[0]);
      plan->unlinkNode(keyValueNodes[1]);

      // Replace current ExecutionNode with new one;

      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
