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

#include "Aql/Match/ProjectionAccessChecker.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryWarnings.h"
#include "Aql/Variable.h"
#include "Basics/voc-errors.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arangodb::aql::match {
namespace {

struct ProjectedAttributes {
  Variable const* variable{nullptr};
  std::vector<std::vector<std::string>> allowedPaths;
};

[[nodiscard]] bool isPrefix(std::vector<std::string> const& prefix,
                            std::vector<std::string> const& path) noexcept {
  if (prefix.size() > path.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), path.begin());
}

[[nodiscard]] bool isPathAllowed(
    std::vector<std::string> const& accessPath,
    std::vector<std::vector<std::string>> const& allowedPaths) noexcept {
  for (auto const& allowed : allowedPaths) {
    // The kept subtree covers this access, or the access is a parent of a
    // kept nested path (e.g. KEEP profile.name allows reading v.profile).
    if (isPrefix(allowed, accessPath) || isPrefix(accessPath, allowed)) {
      return true;
    }
  }
  return false;
}

void addMandatoryPaths(std::vector<std::vector<std::string>>& paths,
                       std::span<std::string_view const> mandatory) {
  for (auto attr : mandatory) {
    paths.push_back({std::string(attr)});
  }
}

void addProjectionPaths(std::vector<std::vector<std::string>>& paths,
                        Projection const& projection,
                        std::span<std::string_view const> mandatory) {
  auto const isMandatory = [&](std::string_view name) noexcept {
    return std::find(mandatory.begin(), mandatory.end(), name) !=
           mandatory.end();
  };

  for (auto const& item : projection.items) {
    if (item.isAlias()) {
      paths.push_back({item.name});
      continue;
    }
    TRI_ASSERT(item.isKeep());
    TRI_ASSERT(!item.path.empty());
    if (isMandatory(item.topLevelKey())) {
      continue;
    }
    paths.push_back(item.path);
  }
}

void registerProjectedVariable(
    std::unordered_map<VariableId, ProjectedAttributes>& projected,
    Variable const* variable, std::optional<Projection> const& projection,
    std::span<std::string_view const> mandatory) {
  if (variable == nullptr || !projection.has_value()) {
    return;
  }
  ProjectedAttributes attrs;
  attrs.variable = variable;
  addMandatoryPaths(attrs.allowedPaths, mandatory);
  addProjectionPaths(attrs.allowedPaths, *projection, mandatory);
  projected.insert_or_assign(variable->id, std::move(attrs));
}

void collectProjectedVariables(
    NormalizedStatement const& statement,
    std::unordered_map<VariableId, ProjectedAttributes>& projected) {
  auto registerVertex = [&](NormalizedVertex const& vertex) {
    registerProjectedVariable(projected, vertex.variable, vertex.projection,
                              kMandatoryDocumentProjectionAttributes);
  };

  for (auto const& pattern : statement.patterns) {
    if (pattern.start.kind == PatternElement::Kind::kVertex &&
        pattern.start.vertex.has_value()) {
      registerVertex(*pattern.start.vertex);
    }
    for (auto const& segment : pattern.segments) {
      registerProjectedVariable(projected, segment.edge.variable,
                                segment.edge.projection,
                                kMandatoryEdgeDocumentProjectionAttributes);
      if (segment.target.kind == PatternElement::Kind::kVertex &&
          segment.target.vertex.has_value()) {
        registerVertex(*segment.target.vertex);
      }
    }
  }
}

[[nodiscard]] bool extractAccessPath(AstNode const* node,
                                     Variable const*& variable,
                                     std::vector<std::string>& path) {
  path.clear();
  AstNode const* cursor = node;
  while (cursor != nullptr && cursor->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    path.emplace_back(cursor->getStringView());
    cursor = cursor->getMemberUnchecked(0);
  }
  if (cursor == nullptr || cursor->type != NODE_TYPE_REFERENCE ||
      path.empty()) {
    return false;
  }
  std::reverse(path.begin(), path.end());
  variable = static_cast<Variable const*>(cursor->getData());
  return variable != nullptr;
}

}  // namespace

void warnExcludedAttributeAccesses(Ast& ast,
                                   NormalizedStatement const& statement) {
  std::unordered_map<VariableId, ProjectedAttributes> projected;
  collectProjectedVariables(statement, projected);
  if (projected.empty() || ast.root() == nullptr) {
    return;
  }

  std::unordered_set<std::string> warned;
  Ast::traverseReadOnly(
      ast.root(),
      [](AstNode const* node) {
        // In-pattern FILTER/properties intentionally see the full document.
        return node->type != NODE_TYPE_MATCH;
      },
      [&](AstNode const* node) {
        if (node->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
          return;
        }
        Variable const* variable = nullptr;
        std::vector<std::string> path;
        if (!extractAccessPath(node, variable, path)) {
          return;
        }
        auto it = projected.find(variable->id);
        if (it == projected.end()) {
          return;
        }
        // Warn on the shortest excluded prefix so v.profile.name with
        // RETURN _key reports "profile" once, not every nested access.
        std::vector<std::string> excludedPrefix;
        for (size_t len = 1; len <= path.size(); ++len) {
          std::vector<std::string> prefix(path.begin(), path.begin() + len);
          if (!isPathAllowed(prefix, it->second.allowedPaths)) {
            excludedPrefix = std::move(prefix);
            break;
          }
        }
        if (excludedPrefix.empty()) {
          return;
        }

        auto attr = absl::StrJoin(excludedPrefix, ".");
        auto key = absl::StrCat(variable->id, ":", attr);
        if (!warned.emplace(key).second) {
          return;
        }
        ast.query().warnings().registerWarning(
            TRI_ERROR_QUERY_MATCH_EXCLUDED_ATTRIBUTE,
            absl::StrCat("attribute '", attr,
                         "' was excluded by MATCH projection on variable '",
                         variable->name, "'"));
      });
}

}  // namespace arangodb::aql::match
