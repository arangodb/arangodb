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

#include "Aql/Match/ProjectionPushdown.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/QueryContext.h"
#include "Aql/Variable.h"
#include "Containers/FlatHashSet.h"

#include <utility>

namespace arangodb::aql::match {
namespace {

[[nodiscard]] bool collectAttributesFromExpression(
    AstNode const* node, Variable const* documentVariable,
    containers::FlatHashSet<AttributeNamePath>& attributes,
    ResourceMonitor& monitor) {
  if (node == nullptr) {
    return true;
  }
  return Ast::getReferencedAttributesRecursive(
      node, documentVariable, /*expectedAttribute*/ "", attributes, monitor);
}

}  // namespace

std::optional<Projections> buildEnumerateProjections(
    Ast& ast, Variable const* documentVariable,
    std::unordered_map<VariableId, Variable const*> const& subst,
    std::optional<Projection> const& projection,
    std::span<std::string_view const> mandatoryAttributes,
    std::vector<PropertyConstraint> const& properties,
    std::optional<ExpressionRef> const& filter) {
  if (documentVariable == nullptr || !projection.has_value()) {
    return std::nullopt;
  }

  auto& monitor = ast.query().resourceMonitor();
  containers::FlatHashSet<AttributeNamePath> attributes;

  for (auto attr : mandatoryAttributes) {
    attributes.emplace(AttributeNamePath(std::string(attr), monitor));
  }

  for (auto const& item : projection->items) {
    if (item.isKeep()) {
      TRI_ASSERT(!item.path.empty());
      attributes.emplace(AttributeNamePath(item.path, monitor));
      continue;
    }
    TRI_ASSERT(item.isAlias());
    // Same rewrite ProjectionBuilder applies so we see accesses on the
    // full-document temp rather than the user pattern variable.
    AstNode* rewritten = Ast::replaceVariables(
        const_cast<AstNode*>(item.expression.node), subst);
    if (!collectAttributesFromExpression(rewritten, documentVariable,
                                         attributes, monitor)) {
      return std::nullopt;
    }
  }

  for (auto const& property : properties) {
    attributes.emplace(AttributeNamePath(property.key, monitor));
  }

  if (filter.has_value()) {
    AstNode* rewritten =
        Ast::replaceVariables(const_cast<AstNode*>(filter->node), subst);
    if (!collectAttributesFromExpression(rewritten, documentVariable,
                                         attributes, monitor)) {
      return std::nullopt;
    }
  }

  if (attributes.empty()) {
    return std::nullopt;
  }
  return Projections{std::move(attributes)};
}

}  // namespace arangodb::aql::match
