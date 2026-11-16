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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Match/PathConstruction.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Basics/debugging.h"

#include <memory>

namespace arangodb::aql::match {

PathConstruction::PathConstruction(ExecutionPlan& plan, Ast* ast)
    : _plan(plan), _ast(ast) {}

void PathConstruction::addPathVertex(std::vector<AstNode const*>& pathVertices,
                                     Variable const* variable) {
  pathVertices.push_back(_ast->createNodeReference(variable));
}

void PathConstruction::addPathEdge(std::vector<AstNode const*>& pathEdges,
                                   Variable const* variable) {
  pathEdges.push_back(_ast->createNodeReference(variable));
}

void PathConstruction::appendTraversalPath(
    std::vector<AstNode const*>& pathVertices,
    std::vector<AstNode const*>& pathEdges,
    Variable const* traversalPathVariable,
    Variable const* startDestinationVariable,
    Variable const* targetDestinationVariable) {
  TRI_ASSERT(traversalPathVariable != nullptr);
  TRI_ASSERT(targetDestinationVariable != nullptr);
  TRI_ASSERT(!pathVertices.empty());

  auto* pathRef = _ast->createNodeReference(traversalPathVariable);
  pathEdges.push_back(_ast->createNodeArraySplice(
      _ast->createNodeAttributeAccess(pathRef, "edges")));

  // Rebuild the traversal segment using MATCH endpoint variables if
  // available. The traversal's first vertex may originate from an external
  // ID, so use FIRST(traversalPath.vertices) when no projected start
  // variable exists to ensure the path contains a document rather than an
  // ID string. Intermediate vertices remain full traversal documents,
  // while the target uses its destination variable.
  pathVertices.pop_back();

  auto* vertices = _ast->createNodeAttributeAccess(
      _ast->createNodeReference(traversalPathVariable), "vertices");
  if (startDestinationVariable != nullptr) {
    pathVertices.push_back(_ast->createNodeReference(startDestinationVariable));
  } else {
    auto* firstArgs = _ast->createNodeArray();
    firstArgs->addMember(vertices);
    pathVertices.push_back(
        _ast->createNodeFunctionCall("FIRST", firstArgs, true));
  }

  auto* sliceArgs = _ast->createNodeArray();
  sliceArgs->addMember(vertices);
  sliceArgs->addMember(_ast->createNodeValueInt(1));
  sliceArgs->addMember(_ast->createNodeValueInt(-1));
  pathVertices.push_back(_ast->createNodeArraySplice(
      _ast->createNodeFunctionCall("SLICE", sliceArgs, true)));
  pathVertices.push_back(_ast->createNodeReference(targetDestinationVariable));
}

AstNode* PathConstruction::constructArray(
    std::vector<AstNode const*> const& vars) {
  auto root = _ast->createNodeArray();
  for (auto v : vars) {
    root->addMember(v);
  }
  return root;
}

CalculationNode* PathConstruction::constructPathObject(
    Variable const* outVariable, std::vector<AstNode const*> const& vertices,
    std::vector<AstNode const*> const& edges) {
  auto root = _ast->createNodeObject();

  root->addMember(
      _ast->createNodeObjectElement("edges", constructArray(edges)));
  root->addMember(
      _ast->createNodeObjectElement("vertices", constructArray(vertices)));

  return _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      outVariable);
}

ExecutionNode* PathConstruction::finalizePattern(
    ExecutionNode* previous, std::vector<ExecutionNode*> const& projections,
    Variable const* pathVariable,
    std::vector<AstNode const*> const& pathVertices,
    std::vector<AstNode const*> const& pathEdges) {
  auto* en = previous;
  for (auto* p : projections) {
    if (p != nullptr) {
      p->addDependency(previous);
      previous = en = p;
    }
  }

  if (pathVariable != nullptr) {
    auto calcNode = constructPathObject(pathVariable, pathVertices, pathEdges);
    calcNode->addDependency(previous);
    previous = en = calcNode;
  }
  return en;
}

}  // namespace arangodb::aql::match
