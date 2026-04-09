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

#include "OptimizeTraversals.h"

#include "Aql/AttributeNamePath.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/OptimizeTraversalPathVariable.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"
#include "Graph/TraverserOptions.h"

namespace arangodb::aql {
using EN = ExecutionNode;

namespace {

bool applyGraphProjections(TraversalNode* traversal) {
  auto* options =
      static_cast<arangodb::traverser::TraverserOptions*>(traversal->options());
  containers::FlatHashSet<AttributeNamePath> attributes;
  bool modified = false;
  size_t maxProjections = options->getMaxProjections();
  auto pathOutVariable = traversal->pathOutVariable();

  // find projections for vertex output variable
  bool useVertexProjections = true;

  // if the path does not include vertices, we can restrict the vertex
  // gathering to only the required attributes
  if (traversal->vertexOutVariable() != nullptr) {
    useVertexProjections = utils::findProjections(
        traversal, traversal->vertexOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && options->producePathsVertices() &&
      pathOutVariable != nullptr) {
    useVertexProjections = utils::findProjections(
        traversal, pathOutVariable, StaticStrings::GraphQueryVertices,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && !attributes.empty() &&
      attributes.size() <= maxProjections) {
    traversal->setVertexProjections(Projections(std::move(attributes)));
    modified = true;
  }

  // find projections for edge output variable
  attributes.clear();
  bool useEdgeProjections = true;

  if (traversal->edgeOutVariable() != nullptr) {
    useEdgeProjections = utils::findProjections(
        traversal, traversal->edgeOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections && options->producePathsEdges() &&
      pathOutVariable != nullptr) {
    useEdgeProjections = utils::findProjections(
        traversal, pathOutVariable, StaticStrings::GraphQueryEdges,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections) {
    // if we found any projections, make sure that they include _from
    // and _to, as the traversal code will refer to these attributes later.
    if (ServerState::instance()->isCoordinator() && !traversal->isSmart() &&
        !traversal->isLocalGraphNode() && !traversal->isUsedAsSatellite()) {
      // On cluster community variant we will also need the ID value on the
      // coordinator to uniquely identify edges
      AttributeNamePath idElement = {
          StaticStrings::IdString,
          traversal->plan()->getAst()->query().resourceMonitor()};
      attributes.emplace(std::move(idElement));
      // Also the community variant needs to transport weight, as the
      // coordinator will do the searching.
      if (traversal->options()->mode ==
          traverser::TraverserOptions::Order::WEIGHTED) {
        AttributeNamePath weightElement = {
            traversal->options()->weightAttribute,
            traversal->plan()->getAst()->query().resourceMonitor()};
        attributes.emplace(std::move(weightElement));
      }
    }

    AttributeNamePath fromElement = {
        StaticStrings::FromString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(fromElement));

    AttributeNamePath toElement = {
        StaticStrings::ToString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(toElement));

    if (attributes.size() <= maxProjections) {
      traversal->setEdgeProjections(Projections(std::move(attributes)));
      modified = true;
    }
  }

  return modified;
}

}  // namespace

/// @brief optimizes away unused traversal output variables and
/// merges filter nodes into graph traversal nodes
void optimizeTraversalsRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                            OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_set<AttributeNamePath> attributes;
  bool modified = false;

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  // While on it, pick up possible projections on the vertex and edge documents
  for (auto const& n : tNodes) {
    auto* traversal = ExecutionNode::castTo<TraversalNode*>(n);
    auto* options = static_cast<arangodb::traverser::TraverserOptions*>(
        traversal->options());

    std::vector<Variable const*> pruneVars;
    traversal->getPruneVariables(pruneVars);

    // optimize path output variable
    auto pathOutVariable = traversal->pathOutVariable();
    modified |=
        optimizeTraversalPathVariable(pathOutVariable, traversal, pruneVars);

    // note that we can NOT optimize away the vertex output variable
    // yet, as many traversal internals depend on the number of vertices
    // found/built
    //
    // however, we can turn off looking up vertices and producing them in the
    // result set. we can do this if the traversal's vertex out variable is
    // never used later and also the traversal's path out variable is not used
    // later (note that the path out variable can contain the "vertices" sub
    // attribute)
    auto outVariable = traversal->vertexOutVariable();
    if (outVariable != nullptr) {
      if (!n->isVarUsedLater(outVariable) &&
          std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
              pruneVars.end()) {
        outVariable = traversal->pathOutVariable();
        if (outVariable == nullptr ||
            ((!n->isVarUsedLater(outVariable) ||
              !options->producePathsVertices()) &&
             std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
                 pruneVars.end())) {
          // both traversal vertex and path outVariables not used later
          options->setProduceVertices(false);
          modified = true;
        }
      }
    }

    outVariable = traversal->edgeOutVariable();
    if (outVariable != nullptr) {
      if (!n->isVarUsedLater(outVariable)) {
        // traversal edge outVariable not used later
        options->setProduceEdges(false);
        if (std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
            pruneVars.end()) {
          traversal->setEdgeOutput(nullptr);
        }
        modified = true;
      }
    }

    // handle projections (must be done after path variable optimization)
    bool appliedProjections = applyGraphProjections(traversal);
    if (appliedProjections) {
      modified = true;
    }

    // check if we can make use of the optimized neighbors enumerator
    if (!options->isDisjoint()) {
      // Use NeighborsEnumerator optimization only in case we have do not
      // have a (Hybrid)Disjoint SmartGraph
      if (!ServerState::instance()->isCoordinator()) {
        if (traversal->vertexOutVariable() != nullptr &&
            traversal->edgeOutVariable() == nullptr &&
            traversal->pathOutVariable() == nullptr &&
            options->isUseBreadthFirst() &&
            options->uniqueVertices == traverser::TraverserOptions::GLOBAL &&
            !options->usesPrune() && !options->hasDepthLookupInfo()) {
          // this is possible in case *only* vertices are produced (no edges, no
          // path), the traversal is breadth-first, the vertex uniqueness level
          // is set to "global", there is no pruning and there are no
          // depth-specific filters
          options->useNeighbors = true;
          modified = true;
        }
      }
    }
  }

  if (!tNodes.empty()) {
    // These are all the end nodes where we start
    containers::SmallVector<ExecutionNode*, 8> nodes;
    plan->findEndNodes(nodes, true);

    for (auto const& n : nodes) {
      TraversalConditionFinder finder(plan.get(), &modified);
      n->walk(finder);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
