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

#include "ParallelizeGather.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/WalkerWorker.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

struct ParallelizableFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  bool _isParallelizable = true;
  bool _hasParallelTraversal = false;

  ~ParallelizableFinder() = default;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if ((node->getType() == EN::SCATTER || node->getType() == EN::DISTRIBUTE) &&
        _hasParallelTraversal) {
      _isParallelizable = false;
      return true;
    }

    if (node->getType() == EN::TRAVERSAL ||
        node->getType() == EN::SHORTEST_PATH ||
        node->getType() == EN::ENUMERATE_PATHS) {
      auto* gn = ExecutionNode::castTo<GraphNode*>(node);
      _hasParallelTraversal |= gn->options()->parallelism() > 1;
      if (!gn->isLocalGraphNode()) {
        _isParallelizable = false;
        return true;
      }
    }

    return false;
  }
};

bool isParallelizable(GatherNode* node) {
  if (node->parallelism() == GatherNode::Parallelism::Serial) {
    return false;
  }

  ParallelizableFinder finder;
  for (ExecutionNode* e : node->getDependencies()) {
    e->walk(finder);
    if (!finder._isParallelizable) {
      return false;
    }
  }
  return true;
}

}  // namespace

void parallelizeGatherRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                           OptimizerRule const& rule) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  containers::SmallVector<ExecutionNode*, 8> graphNodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  for (auto node : nodes) {
    GatherNode* gn = ExecutionNode::castTo<GatherNode*>(node);

    if (!gn->isInSubquery() && isParallelizable(gn)) {
      graphNodes.clear();
      plan->findNodesOfType(
          graphNodes, {EN::TRAVERSAL, EN::SHORTEST_PATH, EN::ENUMERATE_PATHS},
          true);
      bool const allSatellite =
          std::all_of(graphNodes.begin(), graphNodes.end(), [](auto n) {
            GraphNode* graphNode = ExecutionNode::castTo<GraphNode*>(n);
            return graphNode->isLocalGraphNode();
          });

      if (allSatellite) {
        gn->setParallelism(GatherNode::Parallelism::Parallel);
        modified = true;
      }
    } else {
      gn->setParallelism(GatherNode::Parallelism::Serial);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
