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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "PropagateProjectionsIntoEnumerateNear.h"

#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/Optimizer.h"
#include "Assertions/Assert.h"
#include "Cluster/ServerState.h"

namespace arangodb::aql {

using EN = ExecutionNode;

#define LOG_RULE_ENABLED false
#define LOG_RULE_IF(cond) LOG_DEVEL_IF((LOG_RULE_ENABLED) && (cond))
#define LOG_RULE LOG_RULE_IF(true)

// Runs after optimizeProjectionsRule. For every EnumerateNearVectorNode
// with a MaterializeRocksDBNode parent, transfer the projections the
// projections rule assigned to the materializer over to the
// EnumerateNearVectorNode and drop the materializer. Cluster mode is left
// alone because scatterInClusterRule uses the materializer to host the
// SCATTER/GATHER pair.
void propagateProjectionsIntoEnumerateNear(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  LOG_RULE << "propagateProjectionsIntoEnumerateNear: entered, cluster="
           << ServerState::instance()->isRunningInCluster();
  if (ServerState::instance()->isRunningInCluster()) {
    opt->addPlan(std::move(plan), rule, /*modified*/ false);
    return;
  }

  bool modified{false};
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, true);
  for (ExecutionNode* node : nodes) {
    auto* enumerateNearVectorNode =
        ExecutionNode::castTo<EnumerateNearVectorNode*>(node);

    // The previous filter-pushdown pass may have already removed the
    // materializer; in that case this node is in kDocument and there is
    // nothing more to do.
    if (enumerateNearVectorNode->strategy() !=
        EnumerateNearVectorNode::Strategy::kPassThroughId) {
      continue;
    }

    // Find the MaterializeRocksDBNode whose input doc-id variable IS this
    // EnumerateNearVectorNode's outVariable.
    auto const* outVariable = enumerateNearVectorNode->outVariable();
    containers::SmallVector<ExecutionNode*, 8> matCandidates;
    plan->findNodesOfType(matCandidates, EN::MATERIALIZE, /*enterSubqueries*/
                          true);
    materialize::MaterializeRocksDBNode* matNode = nullptr;
    for (auto* cand : matCandidates) {
      auto* mat = dynamic_cast<materialize::MaterializeRocksDBNode*>(cand);
      if (mat != nullptr && &mat->docIdVariable() == outVariable) {
        matNode = mat;
        break;
      }
    }
    if (matNode == nullptr) {
      continue;
    }

    // materializeIntoSeparateVariable may have rebound the materializer's
    // outVariable to a fresh variable, with all downstream attribute
    // references rewritten to read it. Adopt that variable as our own
    // outVariable so dropping the materializer leaves no orphan reads.
    // This is only safe when the EnumerateNearVectorNode does not have a
    // pushed-down filter expression -- that filter binds the loaded doc
    // to the original `oldDocVariable`, and rebinding would break the
    // binding contract with RocksDBVectorIndex::readBatch. Filters are
    // already handled by pushFilterIntoEnumerateNear, so this rule only
    // sees the no-filter path; the assertion makes that explicit.
    TRI_ASSERT(!enumerateNearVectorNode->hasFilter());
    if (&matNode->docIdVariable() != &matNode->outVariable()) {
      enumerateNearVectorNode->rebindOutVariable(&matNode->outVariable());
    }

    // Transfer the projections (with their already-assigned output
    // variables) onto the EnumerateNearVectorNode. The executor will fill
    // those registers from the document or storedValues.
    enumerateNearVectorNode->setProjections(std::move(matNode->projections()));

    enumerateNearVectorNode->recomputeStrategy();

    plan->unlinkNode(matNode);
    // TODO(cluster): useVectorIndexRule excluded EnumerateNearVectorNode
    // from scatter/gather. Once the cluster path can also drop the
    // materializer that exclusion needs to be reverted here.
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
