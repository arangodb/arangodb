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
#include "Aql/ExecutionNode.h"
#include "Aql/Indexes.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------

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
/// @brief remove a CalculationNode that is never needed
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
  if (toRemove.size() > 0) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief find nodes of a certain type
////////////////////////////////////////////////////////////////////////////////

class CalculationNodeFinder : public WalkerWorker<ExecutionNode> {
  RangesInfo* _ranges; 
  ExecutionPlan* _plan;
  Variable const* _var;

  public:
    CalculationNodeFinder (ExecutionPlan* plan, Variable const * var) 
      : _plan(plan), _var(var){
        _ranges = new RangesInfo();
    };

    void before (ExecutionNode* en) {
      if (en->getType() == triagens::aql::ExecutionNode::CALCULATION) {
        auto outvar = en->getVariablesSetHere();
        TRI_ASSERT(outvar.size() == 1);
        if(outvar[0]->id == _var->id){
          auto node = static_cast<CalculationNode*>(en);
          std::string name;
          buildRangeInfo(node->expression()->node(), name);
        }
      }
    }

    void buildRangeInfo (AstNode const* node, std::string& name){
      if(node->type == NODE_TYPE_REFERENCE){
        auto var = static_cast<Variable*>(node->getData());
        auto setter = _plan->getVarSetBy(var->id);
        if( setter != nullptr && 
            setter->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION){
          name = var->name;
        }
        return;
      }
      
      if(node->type == NODE_TYPE_ATTRIBUTE_ACCESS){
        char const* attributeName = node->getStringValue();
        buildRangeInfo(node->getMember(0), name);
        if(!name.empty()){
          name.push_back('.');
          name.append(attributeName);
        }
      }
      
      if(node->type == NODE_TYPE_OPERATOR_BINARY_EQ){
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);
        AstNode const* val;
        AstNode const* nextNode;
        if(rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->type == NODE_TYPE_VALUE){
          val = lhs;
          nextNode = rhs;
        }
        else if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->type == NODE_TYPE_VALUE){
          val = rhs;
          nextNode = lhs;
        }
        else {
          val = nullptr;
        }
        
        if(val != nullptr){
          buildRangeInfo(nextNode, name);
          if(!name.empty()){
            _ranges->insert(name, new RangeInfoBound(val, true), 
              new RangeInfoBound(val, true));
          }
        }

        std::cout << _ranges->toString() << "\n";
      }

      if(node->type == NODE_TYPE_OPERATOR_BINARY_LT || 
         node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
         node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
         node->type == NODE_TYPE_OPERATOR_BINARY_GE){
        
        bool include = (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
         node->type == NODE_TYPE_OPERATOR_BINARY_GE);
        
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);
        RangeInfoBound* low = nullptr;
        RangeInfoBound* high = nullptr;
        AstNode *nextNode;

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->type == NODE_TYPE_VALUE) {
          if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
           node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
            high = new RangeInfoBound(lhs, include);
            low = nullptr;
          } else {
            low = new RangeInfoBound(lhs, include);
            high =nullptr;
          }
          nextNode = rhs;
        }
        else if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->type == NODE_TYPE_VALUE) {
          if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
           node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
            low = new RangeInfoBound(rhs, include);
            high = nullptr;
          } else {
            high = new RangeInfoBound(rhs, include);
            low = nullptr;
          }
          nextNode = lhs;
        }
        else {
          low = nullptr;
          high = nullptr;
        }

        if(low != nullptr || high != nullptr){
          buildRangeInfo(nextNode, name);
          if(!name.empty()){
            _ranges->insert(name, low, high);
          }
        }
        std::cout << _ranges->toString() << "\n";
      }
      
      if(node->type == NODE_TYPE_OPERATOR_BINARY_AND){
        buildRangeInfo(node->getMember(0), name);
        buildRangeInfo(node->getMember(1), name);
        std::cout << _ranges->toString() << "\n";
      }
    }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief relaxRule, do not do anything
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::useIndexRange (Optimizer* opt, 
                                  ExecutionPlan* plan, 
                                  Optimizer::PlanList& out,
                                  bool& keep) {
  
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER);
 
  for (auto n : nodes) {
    auto nn = static_cast<FilterNode*>(n);
    auto invars = nn->getVariablesUsedHere();
    TRI_ASSERT(invars.size() == 1);
    CalculationNodeFinder finder(plan, invars[0]);
    nn->walk(&finder);
  }

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


