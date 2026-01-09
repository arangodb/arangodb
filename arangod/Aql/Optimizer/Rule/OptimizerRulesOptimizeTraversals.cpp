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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesOptimizeTraversals.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Containers/SmallVector.h"

#include <memory>

namespace {

bool applyGraphProjections(arangodb::aql::TraversalNode* traversal) {
  auto* options =
      static_cast<arangodb::traverser::TraverserOptions*>(traversal->options());
  arangodb::containers::FlatHashSet<arangodb::aql::AttributeNamePath>
      attributes;
  bool modified = false;
  size_t maxProjections = options->getMaxProjections();
  auto pathOutVariable = traversal->pathOutVariable();

  // find projections for vertex output variable
  bool useVertexProjections = true;

  // if the path does not include vertices, we can restrict the vertex
  // gathering to only the required attributes
  if (traversal->vertexOutVariable() != nullptr) {
    useVertexProjections = arangodb::aql::utils::findProjections(
        traversal, traversal->vertexOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && options->producePathsVertices() &&
      pathOutVariable != nullptr) {
    useVertexProjections = arangodb::aql::utils::findProjections(
        traversal, pathOutVariable, arangodb::StaticStrings::GraphQueryVertices,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && !attributes.empty() &&
      attributes.size() <= maxProjections) {
    traversal->setVertexProjections(
        arangodb::aql::Projections(std::move(attributes)));
    modified = true;
  }

  // find projections for edge output variable
  attributes.clear();
  bool useEdgeProjections = true;

  if (traversal->edgeOutVariable() != nullptr) {
    useEdgeProjections = arangodb::aql::utils::findProjections(
        traversal, traversal->edgeOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections && options->producePathsEdges() &&
      pathOutVariable != nullptr) {
    useEdgeProjections = arangodb::aql::utils::findProjections(
        traversal, pathOutVariable, arangodb::StaticStrings::GraphQueryEdges,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections) {
    // if we found any projections, make sure that they include _from
    // and _to, as the traversal code will refer to these attributes later.
    if (arangodb::ServerState::instance()->isCoordinator() &&
        !traversal->isSmart() && !traversal->isLocalGraphNode() &&
        !traversal->isUsedAsSatellite()) {
      // On cluster community variant we will also need the ID value on the
      // coordinator to uniquely identify edges
      arangodb::aql::AttributeNamePath idElement = {
          arangodb::StaticStrings::IdString,
          traversal->plan()->getAst()->query().resourceMonitor()};
      attributes.emplace(std::move(idElement));
      // Also the community variant needs to transport weight, as the
      // coordinator will do the searching.
      if (traversal->options()->mode ==
          arangodb::traverser::TraverserOptions::Order::WEIGHTED) {
        arangodb::aql::AttributeNamePath weightElement = {
            traversal->options()->weightAttribute,
            traversal->plan()->getAst()->query().resourceMonitor()};
        attributes.emplace(std::move(weightElement));
      }
    }

    arangodb::aql::AttributeNamePath fromElement = {
        arangodb::StaticStrings::FromString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(fromElement));

    arangodb::aql::AttributeNamePath toElement = {
        arangodb::StaticStrings::ToString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(toElement));

    if (attributes.size() <= maxProjections) {
      traversal->setEdgeProjections(
          arangodb::aql::Projections(std::move(attributes)));
      modified = true;
    }
  }

  return modified;
}

}  // namespace

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace arangodb {
namespace aql {

// checks if the path variable (variable) can be optimized away, or restricted
// to some attributes (vertices, edges, weights)
bool optimizeTraversalPathVariable(
    Variable const* variable, TraversalNode* traversal,
    std::vector<Variable const*> const& pruneVars) {
  if (variable == nullptr) {
    return false;
  }

  auto* options =
      static_cast<arangodb::traverser::TraverserOptions*>(traversal->options());

  if (!traversal->isVarUsedLater(variable)) {
    // traversal path outVariable not used later
    if (std::find(pruneVars.begin(), pruneVars.end(), variable) ==
        pruneVars.end()) {
      options->setProducePaths(/*vertices*/ false, /*edges*/ false,
                               /*weights*/ false);
      traversal->setPathOutput(nullptr);
      return true; /*modified*/
    }

    // we still need to build the path because PRUNE relies on it
    // TODO: this can potentially be optimized in the future.
    options->setProducePaths(/*vertices*/ true, /*edges*/ true,
                             /*weights*/ true);
    return false; /*modified*/
  }

  // path is used later, but lets check which of its sub-attributes
  // "vertices" or "edges" are in use (or the complete path)
  containers::FlatHashSet<AttributeNamePath> attributes;
  VarSet vars;

  ExecutionNode* current = traversal->getFirstParent();
  while (current != nullptr) {
    switch (current->getType()) {
      case EN::CALCULATION: {
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(variable) != vars.end()) {
          // path variable used here
          Expression* exp =
              ExecutionNode::castTo<CalculationNode*>(current)->expression();
          AstNode const* node = exp->node();
          if (!Ast::getReferencedAttributesRecursive(
                  node, variable, /*expectedAttribute*/ "", attributes,
                  current->plan()->getAst()->query().resourceMonitor())) {
            // full path variable is used, or accessed in a way that we don't
            // understand, e.g. "p" or "p[0]" or "p[*]..."
            return false;
          }
        }
        break;
      }
      default: {
        // if the path is used by any other node type, we don't know what to
        // do and will not optimize parts of it away
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(variable) != vars.end()) {
          return false;
        }
        break;
      }
    }
    current = current->getFirstParent();
  }

  // check which attributes from the path are actually used
  bool producePathsVertices = false;
  bool producePathsEdges = false;
  bool producePathsWeights = false;

  for (auto const& it : attributes) {
    TRI_ASSERT(!it.empty());
    if (!producePathsVertices &&
        it[0] == std::string_view{StaticStrings::GraphQueryVertices}) {
      producePathsVertices = true;
    } else if (!producePathsEdges &&
               it[0] == std::string_view{StaticStrings::GraphQueryEdges}) {
      producePathsEdges = true;
    } else if (!producePathsWeights &&
               options->mode == traverser::TraverserOptions::Order::WEIGHTED &&
               it[0] == std::string_view{StaticStrings::GraphQueryWeights}) {
      producePathsWeights = true;
    }
  }

  if (!producePathsVertices && !producePathsEdges && !producePathsWeights &&
      !attributes.empty()) {
    // none of the existing path attributes is actually accessed - but a
    // different (non-existing) attribute is accessed, e.g. `p.whatever`. in
    // order to not optimize away our path variable, and then being unable
    // to access the non-existing attribute, we simply activate the
    // production of vertices. this prevents us from running into errors
    // trying to access an attribute of an optimzed-away variable later
    producePathsVertices = true;
  }

  if (!producePathsVertices || !producePathsEdges || !producePathsWeights) {
    // pass the info to the traversal
    options->setProducePaths(producePathsVertices, producePathsEdges,
                             producePathsWeights);
    return true; /*modified*/
  }

  return false; /*modified*/
}

}  // namespace aql
}  // namespace arangodb

/// @brief optimizes away unused traversal output variables and
/// merges filter nodes into graph traversal nodes
void arangodb::aql::optimizeTraversalsRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_set<arangodb::aql::AttributeNamePath> attributes;
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
            options->uniqueVertices ==
                arangodb::traverser::TraverserOptions::GLOBAL &&
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

// remove filter nodes already covered by a traversal
void arangodb::aql::removeFiltersCoveredByTraversal(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> fNodes;
  plan->findNodesOfType(fNodes, EN::FILTER, true);
  if (fNodes.empty()) {
    // no filters present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& node : fNodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::TRAVERSAL) {
        auto traversalNode =
            ExecutionNode::castTo<TraversalNode const*>(current);

        // found a traversal node, now check if the expression
        // is covered by the traversal
        auto traversalCondition = traversalNode->condition();

        if (traversalCondition != nullptr && !traversalCondition->isEmpty()) {
          VarSet varsUsedByCondition;
          Ast::getReferencedVariables(condition.root(), varsUsedByCondition);

          auto remover = [&](Variable const* outVariable,
                             bool isPathCondition) -> bool {
            if (outVariable == nullptr) {
              return false;
            }
            if (varsUsedByCondition.find(outVariable) ==
                varsUsedByCondition.end()) {
              return false;
            }

            auto newNode = condition.removeTraversalCondition(
                plan.get(), outVariable, traversalCondition->root(),
                isPathCondition);
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it
              // is still used by other nodes in the plan
              return true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              auto* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              return true;
            }

            return false;
          };

          std::array<std::pair<Variable const*, bool>, 3> vars = {
              std::make_pair(traversalNode->pathOutVariable(),
                             /*isPathCondition*/ true),
              std::make_pair(traversalNode->vertexOutVariable(),
                             /*isPathCondition*/ false),
              std::make_pair(traversalNode->edgeOutVariable(),
                             /*isPathCondition*/ false)};

          for (auto [v, isPathCondition] : vars) {
            if (remover(v, isPathCondition)) {
              modified = true;
              handled = true;
              break;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }
      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief removes redundant path variables, after applying
/// `removeFiltersCoveredByTraversal`. Should significantly reduce overhead
void arangodb::aql::removeTraversalPathVariable(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  bool modified = false;
  // first make a pass over all traversal nodes and remove unused
  // variables from them

  for (auto const& n : tNodes) {
    TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);
    auto outVariable = traversal->pathOutVariable();
    if (outVariable != nullptr) {
      std::vector<Variable const*> pruneVars;
      traversal->getPruneVariables(pruneVars);
      modified |=
          optimizeTraversalPathVariable(outVariable, traversal, pruneVars);
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}
