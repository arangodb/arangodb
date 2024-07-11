////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Rule/OptimizerRulesGraph.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

enum class PathAccessState {
  // Search for access to any path variable
  kSearchAccessPath,
  // Check if we access edges or vertices on it
  kAccessEdgesOrVertices,
  // Check if we have an indexed access [x] (we find the x here)
  kSpecificDepthAccessDepth,
  // CHeck if we have an indexed access [x] (we find [ ] here)
  kSpecificDepthAccessIndexedAccess,
};

auto swapOutLastElementAccesses(
    Ast* ast, AstNode* condition,
    std::unordered_map<Variable const*,
                       std::pair<Variable const*, Variable const*>>
        pathVariables) -> std::pair<bool, AstNode*> {
  Variable const* matchingPath = nullptr;
  bool isEdgeAccess = false;
  bool appliedAChange = false;

  PathAccessState currentState{PathAccessState::kSearchAccessPath};

  auto searchAccessPattern = [&](AstNode* node) -> AstNode* {
    switch (currentState) {
      case PathAccessState::kSearchAccessPath: {
        if (node->type == NODE_TYPE_REFERENCE ||
            node->type == NODE_TYPE_VARIABLE) {
          // we are on the bottom of the tree. Check if it is our pathVar
          auto variable = static_cast<Variable*>(node->getData());
          if (pathVariables.contains(variable)) {
            currentState = PathAccessState::kAccessEdgesOrVertices;
            matchingPath = variable;
          }
        }
        // Keep for now
        return node;
      }
      case PathAccessState::kAccessEdgesOrVertices: {
        // we have var.<this-here>
        // Only vertices || edges supported
        if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          if (node->stringEquals(StaticStrings::GraphQueryEdges)) {
            isEdgeAccess = true;
            currentState = PathAccessState::kSpecificDepthAccessDepth;
            return node;
          } else if (node->stringEquals(StaticStrings::GraphQueryVertices)) {
            isEdgeAccess = false;
            currentState = PathAccessState::kSpecificDepthAccessDepth;
            return node;
          }
        }
        // Incorrect type, do not touch, abort this search,
        // setup next one
        currentState = PathAccessState::kSearchAccessPath;
        return node;
      }
      case PathAccessState::kSpecificDepthAccessDepth: {
        // we have var.edges[<this-here>], we can only let -1 pass here for
        // optimization
        if (node->value.type == VALUE_TYPE_INT &&
            node->value.value._int == -1) {
          currentState = PathAccessState::kSpecificDepthAccessIndexedAccess;
          return node;
        }
        // Incorrect type, do not touch, abort this search,
        // setup next one
        currentState = PathAccessState::kSearchAccessPath;
        return node;
      }
      case PathAccessState::kSpecificDepthAccessIndexedAccess: {
        // We are in xxxx[-1] pattern, the next node has to be the indexed
        // access.
        if (node->type == NODE_TYPE_INDEXED_ACCESS) {
          // Reset pattern, we can now restart the search
          currentState = PathAccessState::kSearchAccessPath;

          // Let's switch!!
          appliedAChange = true;
          // We searched for the very same variable above!
          auto const& matchingElements = pathVariables.find(matchingPath);
          return ast->createNodeReference(isEdgeAccess
                                              ? matchingElements->second.second
                                              : matchingElements->second.first);
        }
        currentState = PathAccessState::kSearchAccessPath;
        return node;
      }
    }
    ADB_PROD_ASSERT(false) << "Unreachable code triggered";
    return nullptr;
  };

  auto newCondition = Ast::traverseAndModify(condition, searchAccessPattern);
  if (newCondition != condition) {
    // Swap out everything.
    TRI_ASSERT(appliedAChange) << "We attempt to swap the full condition, we "
                                  "need to count an applied change.";
    return std::make_pair(appliedAChange, newCondition);
  }
  return std::make_pair(appliedAChange, nullptr);
}

}  // namespace

void arangodb::aql::replaceLastAccessOnGraphPathRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  // Only pick Traversal nodes, all others do not provide allowed output format
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, ExecutionNode::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }
  // Unfortunately we do not have a reverse lookup on where a variable is used.
  // So we first select all traversal candidates, and afterwards check where
  // they are used.
  std::unordered_map<Variable const*,
                     std::pair<Variable const*, Variable const*>>
      candidates;
  candidates.reserve(tNodes.size());
  for (auto const& n : tNodes) {
    auto* traversal = ExecutionNode::castTo<TraversalNode*>(n);

    if (traversal->isPathOutVariableUsedLater()) {
      auto pathOutVariable = traversal->pathOutVariable();
      TRI_ASSERT(pathOutVariable != nullptr);
      // Without further optimization an accessible path variable, requires
      // vertex and edge to be accessible.
      TRI_ASSERT(traversal->vertexOutVariable() != nullptr);
      TRI_ASSERT(traversal->edgeOutVariable() != nullptr);
      candidates.emplace(pathOutVariable,
                         std::make_pair(traversal->vertexOutVariable(),
                                        traversal->edgeOutVariable()));
    }
  }

  if (candidates.empty()) {
    // no traversals with path output present nothing to be done
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  // Path Access always has to be in a Calculation Node.
  // So let us check for those
  tNodes.clear();
  plan->findNodesOfType(tNodes, ExecutionNode::CALCULATION, true);

  if (tNodes.empty()) {
    // no calculations, so no one that could access the path
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool appliedAChange = false;
  for (auto& n : tNodes) {
    auto* calculation = ExecutionNode::castTo<CalculationNode*>(n);
    auto [didApplyChange, replacementCondition] = swapOutLastElementAccesses(
        plan->getAst(), calculation->expression()->nodeForModification(),
        candidates);
    // Gets true as soon as one of the swapOut calls returns true
    appliedAChange |= didApplyChange;
    if (replacementCondition != nullptr) {
      TRI_ASSERT(appliedAChange) << "We attempt to swap the full condition, we "
                                    "need to count an applied change.";
      // This is the indicator that we have to replace the full expression here.
      calculation->expression()->replaceNode(replacementCondition);
    }
  }
  opt->addPlan(std::move(plan), rule, appliedAChange);
}
