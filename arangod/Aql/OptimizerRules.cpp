////////////////////////////////////////////////////////////////////////////////
/// @brief rules for the query optimizer
///
/// @file arangod/Aql/OptimizerRules.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/OptimizerRules.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all unnecessary filters
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryFiltersRule (Optimizer* opt, 
                                                 ExecutionPlan* plan, 
                                                 Optimizer::PlanList& out,
                                                 bool& keep) {
  
  keep = true;
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER);
  
  for (auto n : nodes) {
    // filter has one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter != nullptr && setter->getType() == triagens::aql::ExecutionNode::CALCULATION) {
      // if it was a CalculationNode, check its expression
      auto s = static_cast<CalculationNode*>(setter);
      auto root = s->expression()->node();

      if (root->isConstant()) {
        // the expression is a constant value
        if (root->toBoolean()) {
          // TODO: remove filter node and merge with following node
          std::cout << "FOUND A CONSTANT FILTER WHICH IS ALWAYS TRUE\n";
        }
        else {
          // TODO: remove filter node plus all dependencies
          std::cout << "FOUND A CONSTANT FILTER WHICH IS ALWAYS FALSE\n";
        }
      }
    }
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief relaxRule, do not do anything
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::relaxRule (Optimizer* opt, 
                              ExecutionPlan* plan, 
                              Optimizer::PlanList& out,
                              bool& keep) {
  keep = true;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are never needed
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalc (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::PlanList& out, 
                                          bool& keep) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION);
  std::unordered_set<ExecutionNode*> toRemove;
  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (! nn->expression()->canThrow()) {
      // If this node can throw, we must not optimize it away!
      auto outvar = n->getVariablesSetHere();
      TRI_ASSERT(outvar.size() == 1);
      auto varsUsedLater = n->getVarsUsedLater();
      if (varsUsedLater.find(outvar[0]) == varsUsedLater.end()) {
        // The variable whose value is calculated here is not used at
        // all further down the pipeline! We remove the whole
        // calculation node, 
        toRemove.insert(n);
      }
    }
  }
  if (! toRemove.empty()) {
    std::cout << "Removing " << toRemove.size() << " unnecessary "
                 "CalculationNodes..." << std::endl;
    plan->removeNodes(toRemove);
    out.push_back(plan);
    keep = false;
  }
  else {
    keep = true;
  }
  return TRI_ERROR_NO_ERROR;
}


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

