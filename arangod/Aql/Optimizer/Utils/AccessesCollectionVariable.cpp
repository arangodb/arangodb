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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AccessesCollectionVariable.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"

namespace arangodb::aql {
using EN = ExecutionNode;

bool accessesCollectionVariable(ExecutionPlan const* plan,
                                               ExecutionNode const* node,
                                               VarSet& vars) {
  if (node->getType() == EN::CALCULATION) {
    auto nn = EN::castTo<CalculationNode const*>(node);
    vars.clear();
    Ast::getReferencedVariables(nn->expression()->node(), vars);
  } else if (node->getType() == EN::SUBQUERY) {
    auto nn = EN::castTo<SubqueryNode const*>(node);
    vars.clear();
    nn->getVariablesUsedHere(vars);
  }

  for (auto const& it : vars) {
    auto setter = plan->getVarSetBy(it->id);
    if (setter == nullptr) {
      continue;
    }
    if (setter->getType() == EN::INDEX ||
        setter->getType() == EN::ENUMERATE_COLLECTION ||
        setter->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
        setter->getType() == EN::SUBQUERY ||
        setter->getType() == EN::TRAVERSAL ||
        setter->getType() == EN::ENUMERATE_PATHS ||
        setter->getType() == EN::SHORTEST_PATH) {
      return true;
    }
  }

  return false;
}
}  // namespace arangodb::aql