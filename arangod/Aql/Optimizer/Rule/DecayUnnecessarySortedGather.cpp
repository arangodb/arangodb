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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Optimizer/Rules.h"
#include "Aql/Collection.h"
#include "Aql/Optimizer/Optimizer.h"

namespace arangodb::aql {
void decayUnnecessarySortedGather(Optimizer* opt,
                                  std::unique_ptr<ExecutionPlan> plan,
                                  OptimizerRule const& rule) {
  using EN = ExecutionNode;
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  bool modified = false;

  for (auto& n : nodes) {
    auto gatherNode = ExecutionNode::castTo<GatherNode*>(n);
    if (gatherNode->elements().empty()) {
      continue;
    }

    auto const* collection = GatherNode::findCollection(*gatherNode);

    // For views (when 'collection == nullptr') we don't need
    // to check number of shards
    // On SmartEdge collections we have 0 shards and we need the elements
    // to be injected here as well. So do not replace it with > 1
    if (collection && collection->numberOfShards() == 1) {
      modified = true;
      gatherNode->elements().clear();
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
