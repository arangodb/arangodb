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

#include "MaterializeIntoSeparateVariable.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"

namespace arangodb::aql {
using EN = ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

namespace {

void replaceVariableDownwards(
    ExecutionNode* first,
    std::unordered_map<VariableId, Variable const*> const& replacement) {
  for (auto* n = first; n != nullptr; n = n->getFirstParent()) {
    if (n->getType() == ExecutionNode::SUBQUERY) {
      auto* subquery = ExecutionNode::castTo<SubqueryNode*>(n);
      replaceVariableDownwards(subquery->getSubquery()->getSingleton(),
                               replacement);
    } else {
      n->replaceVariables(replacement);
    }
  }
}

}  // namespace

void materializeIntoSeparateVariable(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const& rule) {
  bool modified = false;

  // this rule depends crucially on the optimize-projections rule
  if (!plan->isDisabledRule(
          static_cast<int>(OptimizerRule::optimizeProjectionsRule))) {
    containers::SmallVector<ExecutionNode*, 8> indexes;
    plan->findNodesOfType(indexes, EN::MATERIALIZE, /* enterSubqueries */ true);

    for (auto node : indexes) {
      TRI_ASSERT(node->getType() == EN::MATERIALIZE);
      auto matNode = dynamic_cast<materialize::MaterializeRocksDBNode*>(node);
      if (matNode == nullptr) {
        continue;  // search materialize requires more work
      }

      // create a new output variable for the materialized document.
      // this happens after the join rule. Otherwise, joins are not detected.
      // A separate variable comes in handy when optimizing projections, because
      // it allows to distinguish where the projections belongs to (Index or
      // Mat).
      auto newOutVariable =
          plan->getAst()->variables()->createTemporaryVariable();
      matNode->setDocOutVariable(*newOutVariable);

      // replace every occurrence with this new variable
      replaceVariableDownwards(
          matNode->getFirstParent(),
          {{matNode->oldDocVariable().id, newOutVariable}});
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
