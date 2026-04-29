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
#include "Aql/Projections.h"
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

    // Coverage analysis mutates the projections
    auto candidateProjections = matNode->projections();
    bool const indexCoversProjections =
        !candidateProjections.empty() && index->covers(candidateProjections);

    bool const filterLoadsDocument =
        vectorNode->filterMode() == vector::FilterMode::kDocument;

    if (indexCoversProjections) {
      candidateProjections.setCoveringContext(vectorNode->collection()->id(),
                                              index);
      vectorNode->setProjections(std::move(candidateProjections));
      vectorNode->setProjectionMode(vector::ProjectionMode::kCovered);
    } else if (filterLoadsDocument) {
      // iterator already loads the doc for filter eval -- capture it for
      // the executor and project by name from it.
      vectorNode->setProjections(std::move(matNode->projections()));
      vectorNode->setProjectionMode(vector::ProjectionMode::kDocument);
    } else {
      // Neither optimization win applies; the materializer must stay.
      continue;
    }

    plan->unlinkNode(matNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
