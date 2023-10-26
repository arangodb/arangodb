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
#include "EnumeratePathsRule.h"

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/TraversalNode.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

enum class PathAccessState {
  // Search for access to any path variable
  kSearchAccessPath,
  // Check if we access edges or vertices on it
  kAccessEdgesOrVertices,
  // Check if we have an indexed access [x] (we find the x here)
  kSpecificDepthAccessDepth,
  // CHeck if we have an indexed access [x] (we find [ ] here)
  kSpecificDepthAccessIndexedAccess,
};
}

namespace arangodb::aql {

void optimizeEnumeratePathsRule(Optimizer* opt,
                                std::unique_ptr<ExecutionPlan> plan,
                                OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> enumeratePathsNodes;
  plan->findNodesOfType(enumeratePathsNodes, ExecutionNode::ENUMERATE_PATHS,
                        true);

  if (enumeratePathsNodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  auto appliedAChange = false;

  //  for (auto const& enumeratePathsNode : enumeratePathsNodes) {
  //    auto pathOutputVariable = enumeratePathsNode->pathOutVariable();
  // TRI_ASSERT(pathOutputVariable != nullptr);

  // Find (simple) filter conditions on pathOutVariable of the form
  // - pathOutVariable.vertices.[* RETURN ...]
  // - pathOutVariable.edges.[* RETURN ...]
  //}

  opt->addPlan(std::move(plan), rule, appliedAChange);
}

}  // namespace arangodb::aql
