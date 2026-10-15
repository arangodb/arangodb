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

#include <optional>
#include <span>
#include <vector>

namespace arangodb::aql {
class Ast;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Build storage/index projections for a MATCH-projected collection
/// scan: mandatory attrs ∪ RETURN paths ∪ attrs used by properties/WHERE/alias
/// expressions on the pattern variable. Empty optional means "do not restrict"
/// (full document).
[[nodiscard]] std::optional<Projections> buildEnumerateProjections(
    Ast& ast, Variable const* patternVariable,
    std::optional<Projection> const& projection,
    std::span<std::string_view const> mandatoryAttributes,
    std::vector<PropertyConstraint> const& properties,
    std::optional<ExpressionRef> const& filter);

}  // namespace arangodb::aql::match
