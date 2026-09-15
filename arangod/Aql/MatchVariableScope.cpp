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

#include "MatchVariableScope.h"

#include "Aql/Variable.h"
#include "Basics/debugging.h"

namespace arangodb::aql {

void MatchVariableScope::registerSubstitution(Variable const* userVariable,
                                              Variable const* planVariable) {
  TRI_ASSERT(userVariable != nullptr);
  TRI_ASSERT(planVariable != nullptr);
  _substitutions.emplace(userVariable->id, planVariable);
}

Variable const* MatchVariableScope::resolve(Variable const* variable) const {
  TRI_ASSERT(variable != nullptr);
  if (auto it = _substitutions.find(variable->id);
      it != std::end(_substitutions)) {
    return it->second;
  }
  return variable;
}

}  // namespace arangodb::aql
