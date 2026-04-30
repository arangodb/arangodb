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

#include "MaterializeForEnumerateNear.h"

#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VectorIndex/VectorSearchConfiguration.h"

namespace arangodb::aql {

using EN = ExecutionNode;

namespace {

// Discover which attributes of the doc variable produced by the vector node
// are read downstream and turn them into a Projections value. Returns an
// empty Projections if no projection rewrite is feasible (e.g. the doc itself
// is consumed).
Projections collectProjections(EnumerateNearVectorNode& vectorNode) {
  auto* start = vectorNode.getFirstParent();
  if (start == nullptr) {
    return Projections{};
  }

  containers::FlatHashSet<AttributeNamePath> attributes;
  bool const projectable = utils::findProjections(
      start, vectorNode.outVariable(), /*expectedAttribute*/ "",
      /*excludeStartNodeFilterCondition*/ false, attributes);
  if (!projectable || attributes.empty() ||
      attributes.size() > vectorNode.maxProjections()) {
    return Projections{};
  }
  return Projections(std::move(attributes));
}

}  // namespace

// Decide for each EnumerateNearVectorNode how its document output reaches
// downstream consumers. There are three options:
//
//   1. The vector index storedValues cover the projections. The vector node
//      produces projection values directly (kCovered).
//   2. The pushed-down filter already loaded the document. The vector node
//      can serve projections (or the whole doc) from the in-hand document
//      (kDocument).
//   3. Neither applies. We insert a MaterializeRocksDBNode after the vector
//      node to translate the doc-id into the full document, and exclude the
//      vector node from scatter/gather so the materializer anchors the
//      cluster snippet.
void materializeForEnumerateNear(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  bool modified{false};
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, /*enterSub*/ true);

  for (ExecutionNode* node : nodes) {
    auto* vectorNode = ExecutionNode::castTo<EnumerateNearVectorNode*>(node);

    auto projections = collectProjections(*vectorNode);
    bool const indexCoversProjections =
        !projections.empty() && vectorNode->index()->covers(projections);
    bool const filterLoadsDocument =
        vectorNode->filterMode() == vector::FilterMode::kDocument;

    if (indexCoversProjections) {
      projections.setCoveringContext(vectorNode->collection()->id(),
                                     vectorNode->index());
    }

    if (indexCoversProjections || filterLoadsDocument) {
      if (!projections.empty()) {
        utils::rewriteProjectionAttributeAccesses(
            *plan, vectorNode, vectorNode->outVariable(), projections,
            /*index*/ 0);
        vectorNode->setProjections(std::move(projections));
      }
      vectorNode->setProjectionMode(indexCoversProjections
                                        ? vector::ProjectionMode::kCovered
                                        : vector::ProjectionMode::kDocument);
      modified = true;
      continue;
    }

    // Vector node alone can't supply the doc -- insert a materializer.
    auto* materializer = plan->createNode<materialize::MaterializeRocksDBNode>(
        plan.get(), plan->nextId(), vectorNode->collection(),
        *vectorNode->outVariable(), *vectorNode->outVariable(),
        *vectorNode->outVariable());
    plan->insertAfter(vectorNode, materializer);
    plan->excludeFromScatterGather(vectorNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
