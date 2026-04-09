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

#include "RemoveTraversalPathVariable.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/OptimizeTraversalPathVariable.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void removeTraversalPathVariable(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  bool modified = false;

  for (auto const& n : tNodes) {
    TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);
    auto outVariable = traversal->pathOutVariable();
    if (outVariable != nullptr) {
      std::vector<Variable const*> pruneVars;
      traversal->getPruneVariables(pruneVars);
      modified |=
          optimizeTraversalPathVariable(outVariable, traversal, pruneVars);
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
