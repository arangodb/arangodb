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

#include "OptimizerRuleUseIndexes.h"

#include "Aql/Ast.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/ScopeGuard.h"
#include "Containers/SmallVector.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

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
