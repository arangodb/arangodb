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

#include "OptimizerRuleOptimizePaths.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Containers/SmallVector.h"
#include "Graph/ShortestPathOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief optimizes away unused K_PATHS things
void arangodb::aql::optimizePathsRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_PATHS, true);

  if (nodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  for (auto const& n : nodes) {
    EnumeratePathsNode* ksp = ExecutionNode::castTo<EnumeratePathsNode*>(n);
    if (!ksp->usesPathOutVariable()) {
      continue;
    }

    Variable const* variable = &(ksp->pathOutVariable());

    containers::FlatHashSet<AttributeNamePath> attributes;
    VarSet vars;
    bool canOptimize = true;

    ExecutionNode* current = ksp->getFirstParent();
    arangodb::ResourceMonitor& resMonitor =
        plan->getAst()->query().resourceMonitor();

    while (current != nullptr && canOptimize) {
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
                    node, variable, /*expectedAttributes*/ "", attributes,
                    resMonitor)) {
              // full path variable is used, or accessed in a way that we don't
              // understand, e.g. "p" or "p[0]" or "p[*]..."
              canOptimize = false;
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
            canOptimize = false;
          }
          break;
        }
      }
      current = current->getFirstParent();
    }

    if (canOptimize) {
      bool produceVertices =
          attributes.contains({StaticStrings::GraphQueryVertices, resMonitor});

      if (!produceVertices) {
        auto* options = ksp->options();
        options->setProduceVertices(false);
        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}