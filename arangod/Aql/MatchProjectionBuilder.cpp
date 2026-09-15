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

#include "MatchProjectionBuilder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"

#include <absl/strings/str_cat.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace arangodb::aql {

MatchProjectionBuilder::MatchProjectionBuilder(ExecutionPlan& plan, Ast* ast)
    : _plan(plan), _ast(ast) {}

ExecutionNode* MatchProjectionBuilder::createDocumentPatternProjection(
    Variable const* destinationVariable, Variable const* fullDocumentVar,
    std::optional<MatchProjection> const& projection,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  return createPatternProjection(
      destinationVariable, fullDocumentVar, projection,
      kMandatoryDocumentMatchProjectionAttributes, subst);
}

ExecutionNode* MatchProjectionBuilder::createEdgeDocumentPatternProjection(
    Variable const* destinationVariable, Variable const* fullDocumentVar,
    std::optional<MatchProjection> const& projection,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  return createPatternProjection(
      destinationVariable, fullDocumentVar, projection,
      kMandatoryEdgeDocumentMatchProjectionAttributes, subst);
}

ExecutionNode* MatchProjectionBuilder::createPatternProjection(
    Variable const* destinationVariable, Variable const* fullDocumentVar,
    std::optional<MatchProjection> const& projectionOpt,
    std::span<std::string_view const> mandatoryAttributes,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  if (!projectionOpt.has_value()) {
    auto* root = _ast->createNodeReference(fullDocumentVar);
    return _plan.createNode<CalculationNode>(
        &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
        destinationVariable);
  }

  // Projection semantics (paths, aliases, reserved attributes) are already
  // normalized; this method only builds the AST / CalculationNode.
  auto const& projection = *projectionOpt;
  auto* root = _ast->createNodeObject();
  auto* ref = _ast->createNodeReference(fullDocumentVar);

  auto registerKey = [&](std::string_view key) -> std::string_view {
    // Copy into Ast resource pool so the resulting AstNode outlives the
    // temporary NormalizedMatchStatement that owns MatchProjection strings.
    char const* p = _ast->resources().registerString(key);
    return {p, key.size()};
  };

  auto findOrCreateNestedObject = [&](AstNode* object,
                                      std::string_view key) -> AstNode* {
    for (size_t i = 0; i < object->numMembers(); ++i) {
      AstNode* elt = object->getMemberUnchecked(i);
      if (elt->type == NODE_TYPE_OBJECT_ELEMENT &&
          elt->getStringView() == key &&
          elt->getMember(0)->type == NODE_TYPE_OBJECT) {
        return elt->getMember(0);
      }
    }
    auto* nested = _ast->createNodeObject();
    object->addMember(_ast->createNodeObjectElement(registerKey(key), nested));
    return nested;
  };

  auto insertNestedPath = [&](AstNode* object,
                              std::vector<std::string> const& path,
                              AstNode* valueExpr) {
    TRI_ASSERT(!path.empty());
    AstNode* cursor = object;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
      cursor = findOrCreateNestedObject(cursor, path[i]);
    }
    cursor->addMember(
        _ast->createNodeObjectElement(registerKey(path.back()), valueExpr));
  };

  auto addProjectedAttribute = [&](std::vector<std::string> const& path) {
    TRI_ASSERT(!path.empty());
    auto* attrAccess = _ast->createNodeAttributeAccess(ref, path);
    insertNestedPath(root, path, attrAccess);
  };

  auto const isReservedAttribute = [&](std::string_view name) noexcept {
    return std::find(mandatoryAttributes.begin(), mandatoryAttributes.end(),
                     name) != mandatoryAttributes.end();
  };

  for (auto attr : mandatoryAttributes) {
    addProjectedAttribute({std::string(attr)});
  }

  std::vector<std::vector<std::string>> keepPaths;
  struct AliasItem {
    std::string_view name;
    AstNode* expr;
  };
  std::vector<AliasItem> aliases;

  for (auto const& item : projection.items) {
    if (item.isAlias()) {
      aliases.push_back(
          AliasItem{item.name, const_cast<AstNode*>(item.expression.node)});
      continue;
    }
    TRI_ASSERT(item.isKeep());
    TRI_ASSERT(!item.path.empty());
    // Reserved attributes are already mandatory. Ignore user projection paths
    // rooted at _id, _from, or _to to avoid overwriting these scalar
    // attributes.
    if (isReservedAttribute(item.topLevelKey())) {
      continue;
    }
    keepPaths.push_back(item.path);
  }

  // Drop paths that are duplicates or have a shorter kept prefix
  {
    std::sort(keepPaths.begin(), keepPaths.end());
    keepPaths.erase(std::unique(keepPaths.begin(), keepPaths.end()),
                    keepPaths.end());
    std::vector<std::vector<std::string>> filtered;
    filtered.reserve(keepPaths.size());
    for (auto const& path : keepPaths) {
      bool covered = false;
      for (auto const& kept : filtered) {
        if (kept.size() <= path.size() &&
            std::equal(kept.begin(), kept.end(), path.begin())) {
          covered = true;
          break;
        }
      }
      if (!covered) {
        filtered.push_back(path);
      }
    }
    keepPaths = std::move(filtered);
  }

  std::unordered_set<std::string_view> usedTopLevelKeys;
  for (auto attr : mandatoryAttributes) {
    usedTopLevelKeys.emplace(attr);
  }
  for (auto const& path : keepPaths) {
    TRI_ASSERT(!path.empty());
    usedTopLevelKeys.emplace(path[0]);
  }

  for (auto const& path : keepPaths) {
    addProjectedAttribute(path);
  }

  for (auto const& alias : aliases) {
    if (isReservedAttribute(alias.name)) {
      continue;
    }
    if (!usedTopLevelKeys.emplace(alias.name).second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          absl::StrCat("duplicate projection attribute name '", alias.name,
                       "'"));
    }

    // alias = expression: evaluate in normal query scope (explicit
    // variable references required, e.g. v.profile.first_name).
    AstNode* expr = Ast::replaceVariables(alias.expr, subst);
    root->addMember(
        _ast->createNodeObjectElement(registerKey(alias.name), expr));
  }

  return _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      destinationVariable);
}

}  // namespace arangodb::aql
