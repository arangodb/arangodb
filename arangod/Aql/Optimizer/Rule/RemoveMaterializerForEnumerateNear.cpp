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

// Discover which attributes of the materialized doc are read downstream
// and turn them into a Projections value. Returns an empty Projections if
// no projection rewrite is feasible (e.g. the doc itself is consumed).
Projections collectProjections(materialize::MaterializeRocksDBNode& matNode) {
  // UseVectorIndex creates the materializer with the same variable for
  // docId, outVariable, and oldOutVariable, so findProjections would
  // bail on matNode itself (its getVariablesUsedHere returns the doc-id
  // which is also our search variable). Skip past matNode and search
  // its parent and further downstream consumers.
  auto* start = matNode.getFirstParent();
  if (start == nullptr) {
    return Projections{};
  }

  containers::FlatHashSet<AttributeNamePath> attributes;
  bool const projectable = utils::findProjections(
      start, &matNode.outVariable(), /*expectedAttribute*/ "",
      /*excludeStartNodeFilterCondition*/ false, attributes);
  if (!projectable || attributes.empty() ||
      attributes.size() > matNode.maxProjections()) {
    return Projections{};
  }
  return Projections(std::move(attributes));
}

}  // namespace

void removeMaterializerForEnumerateNear(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  bool modified{false};
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_NEAR_VECTORS, /*enterSub*/ true);

  for (ExecutionNode* node : nodes) {
    auto* vectorNode = ExecutionNode::castTo<EnumerateNearVectorNode*>(node);
    auto* matNode = findMaterializerFor(*plan, *vectorNode);
    if (matNode == nullptr) {
      continue;
    }

    // Pull the projection list straight off the downstream calc nodes.
    // optimizeProjectionsRule does the same thing for MaterializeNode but
    // runs after us, so we replicate the work to make our coverage check
    // meaningful.
    auto projections = collectProjections(*matNode);
    bool indexCoversProjections =
        !projections.empty() && vectorNode->index()->covers(projections);

    bool const filterLoadsDocument =
        vectorNode->filterMode() == vector::FilterMode::kDocument;

    if (indexCoversProjections) {
      projections.setCoveringContext(vectorNode->collection()->id(),
                                     vectorNode->index());
    } else if (!filterLoadsDocument) {
      // Neither optimisation win applies; the materializer must stay.
      continue;
    }

    if (!projections.empty()) {
      utils::rewriteProjectionAttributeAccesses(
          *plan, matNode, &matNode->outVariable(), projections, /*index*/ 0);
      vectorNode->setProjections(std::move(projections));
    }
    vectorNode->setProjectionMode(indexCoversProjections
                                      ? vector::ProjectionMode::kCovered
                                      : vector::ProjectionMode::kDocument);

    plan->unlinkNode(matNode);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
