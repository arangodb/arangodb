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

#include "Aql/Match/CollectionAccessBuilder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexHint.h"
#include "Aql/Match/FilterBuilder.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "VocBase/AccessMode.h"

#include <memory>
#include <utility>

namespace arangodb::aql::match {

CollectionAccessBuilder::CollectionAccessBuilder(ExecutionPlan& plan, Ast* ast,
                                                 FilterBuilder& filters)
    : _plan(plan), _ast(ast), _filters(filters) {}

std::string CollectionAccessBuilder::requireCollectionName(
    DataSource const& ds) {
  if (ds.kind() != DataSource::Kind::kCollection) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "MATCH planning requires resolved collection names; unresolved "
        "collection bind parameters are not supported at plan time");
  }
  return std::string(ds.name());
}

AstNode* CollectionAccessBuilder::buildEdgeCollectionList(
    NormalizedEdge const& edge) {
  auto* edgeCollectionList = _ast->createNodeArray();
  if (!edge.collectionAstNodes.empty()) {
    for (AstNode const* collectionNode : edge.collectionAstNodes) {
      edgeCollectionList->addMember(collectionNode);
    }
    return edgeCollectionList;
  }

  for (auto const& ds : edge.collections) {
    auto name = requireCollectionName(ds);
    edgeCollectionList->addMember(_ast->createNodeCollection(
        _ast->query().resolver(), name, AccessMode::Type::READ));
  }
  return edgeCollectionList;
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
CollectionAccessBuilder::enumerateCollection(
    DataSource const& dataSource, Variable const* outputVariable,
    std::vector<PropertyConstraint> const& properties,
    std::optional<ExpressionRef> const& filter,
    VariableSubstitution const& subst) {
  auto collectionName = requireCollectionName(dataSource);
  auto& collections = _ast->query().collections();
  auto collection = collections.get(collectionName);
  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no collection for EnumerateCollection");
  }
  IndexHint hint(_ast->query(), _ast->createNodeNop(),
                 IndexHint::FromCollectionOperation{});
  auto enumCollection = _plan.createNode<EnumerateCollectionNode>(
      &_plan, _plan.nextId(), collection, outputVariable, false,
      std::move(hint));

  auto [firstNode, lastNode] = _filters.createPropertiesFilter(
      outputVariable, properties, filter, subst);
  firstNode->addDependency(enumCollection);
  return std::make_tuple(enumCollection, lastNode, outputVariable);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
CollectionAccessBuilder::createCollectionAccess(
    NormalizedVertex const& vertex, Variable const* fullDocumentVariable,
    VariableSubstitution const& subst) {
  return enumerateCollection(vertex.collection, fullDocumentVariable,
                             vertex.properties, vertex.filter, subst);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
CollectionAccessBuilder::createPatternEdgeEnumerateAccess(
    NormalizedEdge const& edge, Variable const* outputVariable,
    VariableSubstitution const& subst) {
  ADB_PROD_ASSERT(!edge.collections.empty());
  return enumerateCollection(edge.collections.front(), outputVariable,
                             edge.properties, edge.filter, subst);
}

}  // namespace arangodb::aql::match
