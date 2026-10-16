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

#pragma once

#include "Aql/Match/PatternTypes.h"
#include "Aql/Projections.h"
#include "Aql/types.h"

#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class Ast;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Build storage/index projections for a MATCH-projected collection
/// scan. Include mandatory attributes, RETURN paths, and attributes referenced
/// by property, WHERE, and alias expressions. Rewrite filter/alias expressions
/// using @p subst to reference @p documentVariable (the enumeration output
/// or full-document temporary variable). An empty optional means no restriction
/// (load the full document).
[[nodiscard]] std::optional<Projections> buildEnumerateProjections(
    Ast& ast, Variable const* documentVariable,
    std::unordered_map<VariableId, Variable const*> const& subst,
    std::optional<Projection> const& projection,
    std::span<std::string_view const> mandatoryAttributes,
    std::vector<PropertyConstraint> const& properties,
    std::optional<ExpressionRef> const& filter);

}  // namespace arangodb::aql::match
