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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "RemoveMaterializerForEnumerateNear.h"

#include "Aql/Collection.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/Optimizer.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "VectorIndex/VectorSearchConfiguration.h"

namespace arangodb::aql {

using EN = ExecutionNode;

namespace {

materialize::MaterializeRocksDBNode* findMaterializerFor(
    ExecutionPlan& plan, EnumerateNearVectorNode const& vectorNode) {
  auto const* outVariable = vectorNode.outVariable();
  containers::SmallVector<ExecutionNode*, 8> candidates;
  plan.findNodesOfType(candidates, EN::MATERIALIZE, /*enterSubqueries*/ true);
  for (auto* cand : candidates) {
    if (auto* mat = dynamic_cast<materialize::MaterializeRocksDBNode*>(cand);
        mat != nullptr && &mat->docIdVariable() == outVariable) {
      return mat;
    }
  }
  return nullptr;
}

}  // namespace

void removeMaterializerForEnumerateNear(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  if (ServerState::instance()->isRunningInCluster()) {
    opt->addPlan(std::move(plan), rule, /*modified*/ false);
    return;
  }

  bool modified{false};
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, /*enterSub*/ true);
  for (ExecutionNode* node : nodes) {
    auto* vectorNode = ExecutionNode::castTo<EnumerateNearVectorNode*>(node);

    auto* matNode = findMaterializerFor(*plan, *vectorNode);
    if (matNode == nullptr) {
      continue;
    }

    auto const& index = vectorNode->index();

    // Try the covered path: copy projections so coverage analysis (which
    // mutates coveringIndexPosition) doesn't touch the materializer's
    // originals if the answer turns out to be "not covered".
    auto candidateProjections = matNode->projections();
    bool const covered = !candidateProjections.empty() &&
                         index->covers(candidateProjections);

    auto const filterMode = vectorNode->filterMode();
    bool const filterLoadsDocument =
        filterMode == vector::FilterMode::kDocument;

    if (!covered && !filterLoadsDocument) {
      // Neither optimization applies -- materializer stays.
      continue;
    }

    // Some materializeIntoSeparateVariable rewrite may have rebound the
    // materializer's outVariable; adopt it so dropping leaves no orphan
    // reads. Skip when a filter is pushed: that filter binds the loaded
    // doc to the original variable, and rebinding would break the
    // binding contract with RocksDBVectorIndex::readBatch.
    if (!vectorNode->hasFilter() &&
        &matNode->docIdVariable() != &matNode->outVariable()) {
      vectorNode->rebindOutVariable(&matNode->outVariable());
    }

    if (covered) {
      candidateProjections.setCoveringContext(
          vectorNode->collection()->id(), index);
      vectorNode->setProjections(std::move(candidateProjections));
      vectorNode->setProjectionMode(vector::ProjectionMode::kCovered);
    } else {
      // filterLoadsDocument: iterator loads the doc anyway; the executor
      // captures it and projects by name.
      vectorNode->setProjections(std::move(matNode->projections()));
      vectorNode->setProjectionMode(vector::ProjectionMode::kDocument);
    }

    plan->unlinkNode(matNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
