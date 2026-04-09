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

#include "OptimizeTraversalPathVariable.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/StaticStrings.h"
#include "Graph/TraverserOptions.h"

namespace arangodb::aql {
using EN = ExecutionNode;

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
}  // namespace arangodb::aql
