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
///
/// @author Wilfried Goesgens
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ReplaceNode.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Basics/debugging.h"

namespace arangodb::aql {

void replaceNode(ExecutionPlan* plan, ExecutionNode* oldNode,
                 ExecutionNode* newNode) {
  if (oldNode == plan->root()) {
    // intentional copy, the dependencies are changed in the loop
    std::vector<ExecutionNode*> deps = oldNode->getDependencies();
    for (auto* x : deps) {
      TRI_ASSERT(x != nullptr);
      newNode->addDependency(x);
      oldNode->removeDependency(x);
    }
    plan->root(newNode, true);
  } else {
    // replaceNode does not seem to work well with subqueries
    // if the subqueries root is replaced.
    // It looks like Subquery node will still point to
    // the old node.

    TRI_ASSERT(oldNode != plan->root());
    plan->replaceNode(oldNode, newNode);
    TRI_ASSERT(!oldNode->hasDependency());
    TRI_ASSERT(!oldNode->hasParent());
  }
}

}  // namespace arangodb::aql
