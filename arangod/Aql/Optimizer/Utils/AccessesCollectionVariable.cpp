////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/Expression.h"
#include "Aql/Ast.h"
#include "Aql/Optimizer/Utils/AccessesCollectionVariable.h"

namespace arangodb::aql {

// TODO: since this is a property of a ExecutionNode it might be appropriate to
// move this code into the ExecutionNode code
bool accessesCollectionVariable(arangodb::aql::ExecutionPlan const* plan,
                                arangodb::aql::ExecutionNode const* node,
                                arangodb::aql::VarSet& vars) {
  using EN = arangodb::aql::ExecutionNode;

  if (node->getType() == EN::CALCULATION) {
    auto nn = EN::castTo<arangodb::aql::CalculationNode const*>(node);
    vars.clear();
    arangodb::aql::Ast::getReferencedVariables(nn->expression()->node(), vars);
  } else if (node->getType() == EN::SUBQUERY) {
    auto nn = EN::castTo<arangodb::aql::SubqueryNode const*>(node);
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
