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

#include "Aql/MatchPatternTypes.h"
#include "Aql/types.h"

#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>

namespace arangodb::aql {

class Ast;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief Lowers MatchProjection into CalculationNode fragments.
class MatchProjectionBuilder {
 public:
  MatchProjectionBuilder(ExecutionPlan& plan, Ast* ast);

  ExecutionNode* createDocumentPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<MatchProjection> const& projection,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionNode* createEdgeDocumentPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<MatchProjection> const& projection,
      std::unordered_map<VariableId, Variable const*> const& subst);

 private:
  /// @brief Shared MATCH projection lowering. @p mandatoryAttributes are
  /// auto-injected and treated as reserved for user projection handling.
  ExecutionNode* createPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<MatchProjection> const& projection,
      std::span<std::string_view const> mandatoryAttributes,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql
