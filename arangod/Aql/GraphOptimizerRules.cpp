////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "GraphOptimizerRules.h"

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/TraversalNode.h"
#include "Containers/SmallVector.h"


#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

void arangodb::aql::replaceLastAccessOnGraphPathRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  // Only pick Traversal nodes, all others do not provide allowed output format
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, ExecutionNode::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }
  // Unfortunately we do not have a reverse lookup on where a variable is used.
  // So we first select all traversal candidates, and afterwards check where
  // they are used.
  containers::SmallVector<Variable const*, 8> candidates;
  candidates.reserve(tNodes.size());
  for (auto const& n : tNodes) {
    auto* traversal = ExecutionNode::castTo<TraversalNode*>(n);

    if (traversal->isPathOutVariableUsedLater()) {
      auto pathOutVariable = traversal->pathOutVariable();
      TRI_ASSERT(pathOutVariable != nullptr);
      candidates.emplace_back(pathOutVariable);
      LOG_DEVEL << "Identified Candidate " << traversal->id();
    }
  }

  if (candidates.empty()) {
    // no traversals with path output present nothing to be done
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  // Path Access always has to be in a Calculation Node.
  // So let us check for those
  tNodes.clear();
  plan->findNodesOfType(tNodes, ExecutionNode::CALCULATION, true);

  if (tNodes.empty()) {
    // no calculations, so no one that could access the path
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  VarSet variablesUsedHere;
  for (auto& n : tNodes) {
    auto* calculation = ExecutionNode::castTo<CalculationNode*>(n);
    variablesUsedHere.clear();
    calculation->getVariablesUsedHere(variablesUsedHere);
    for (auto const& p : candidates) {
      if (variablesUsedHere.contains(p)) {
        calculation->expression()->node()->dump(0);
        LOG_DEVEL << "Found user! " << n->id();
      }
    }
  }
  // Nothing done
  opt->addPlan(std::move(plan), rule, false);
}
