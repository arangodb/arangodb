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

#include "Aql/types.h"

#include <unordered_map>

namespace arangodb::aql {

struct Variable;

/// @brief Tracks MATCH variable substitutions for projection temporaries.
/// User-facing MATCH variables may temporarily map to full-document plan
/// variables until projections are applied at the end of a pattern.
class MatchVariableScope {
 public:
  void registerSubstitution(Variable const* userVariable,
                            Variable const* planVariable);

  /// @brief Resolve @p variable through any registered substitution.
  [[nodiscard]] Variable const* resolve(Variable const* variable) const;

  [[nodiscard]] std::unordered_map<VariableId, Variable const*> const& map()
      const noexcept {
    return _substitutions;
  }

 private:
  std::unordered_map<VariableId, Variable const*> _substitutions;
};

}  // namespace arangodb::aql
