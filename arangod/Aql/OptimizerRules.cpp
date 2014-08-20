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
#include "Aql/Variable.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief remove all unnecessary filters
/// filters that are always true are removed
/// filters that are always false will be removed plus their dependent nodes
/// this modifies the plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryFiltersRule (Optimizer* opt, 
                                                 ExecutionPlan* plan, 
                                                 Optimizer::PlanList& out,
                                                 bool& keep) {
  
  keep = true;
  std::unordered_set<ExecutionNode*> toRemove;
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER);
  
  for (auto n : nodes) {
    // filter has one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter != nullptr && 
        setter->getType() == triagens::aql::ExecutionNode::CALCULATION) {
      // if it was a CalculationNode, check its expression
      auto s = static_cast<CalculationNode*>(setter);
      auto root = s->expression()->node();

      if (root->isConstant()) {
        // the expression is a constant value
        if (root->toBoolean()) {
          // filter is always true
          // remove filter node and merge with following node
          toRemove.insert(n);
        }
        else {
          // filter is always false
         /* TODO 
          // get all dependent nodes of the filter node
          std::vector<ExecutionNode*> stack;
          stack.push_back(n);

          while (! stack.empty()) {
            // pop a node from the stack
            auto current = stack.back();
            stack.pop_back();

            bool removeNode = true;

            if (toRemove.find(current) != toRemove.end()) {
              // detected a cycle. TODO: decide whether a cycle is an error here or 
              // if it is valid
              break;
            }

            if (current->getType() == triagens::aql::ExecutionNode::SINGLETON) {
              // stop at a singleton node
              break;
            }

            if (current->getType() == triagens::aql::ExecutionNode::CALCULATION) {
              auto c = static_cast<CalculationNode*>(current);
              if (c->expression()->node()->canThrow()) {
                // the calculation may throw an exception. we must not remove it
                // because its removal might change the query result
                removeNode = false;
                std::cout << "FOUND A CALCULATION THAT CAN THROW\n";
              }
            }

            auto deps = current->getDependencies();
            for (auto it = deps.begin(); it != deps.end(); ++it) {
              stack.push_back((*it));
            }

            if (removeNode) {
              std::cout << "REMOVING NODE " << current << " OF TYPE: " << current->getTypeString() << "\n";
              toRemove.insert(current);
            }
          }
          */
        }
      }
    }
  }
  
  if (! toRemove.empty()) {
    std::cout << "Removing " << toRemove.size() << " unnecessary "
                 "nodes..." << std::endl;
    plan->removeNodes(toRemove);
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves calculations up in the plan
/// this modifies the plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::moveCalculationsUpRule (Optimizer* opt, 
                                           ExecutionPlan* plan, 
                                           Optimizer::PlanList& out,
                                           bool& keep) {
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION);

  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow()) {
      // we will only move expressions up that cannot throw
      continue;
    }
      
    auto neededVars = n->getVariablesUsedHere();
    // sort the list of variables that the expression needs as its input
    // (sorting is needed for intersection later)
    std::sort(neededVars.begin(), neededVars.end(), &Variable::Comparator);

    std::vector<ExecutionNode*> stack;
    auto deps = n->getDependencies();
    
    for (auto it = deps.begin(); it != deps.end(); ++it) {
      stack.push_back((*it));
    }

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      auto deps = current->getDependencies();

      if (deps.size() != 1) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note that this will also handle Singleton nodes
        break;
      }

      // check which variables the current node defines
      auto dependencyVars = current->getVariablesSetHere();
      // sort the variables (sorting needed for intersection)
      std::sort(dependencyVars.begin(), dependencyVars.end(), &Variable::Comparator);
    
      // create the intersection of variables
      std::vector<Variable const*> shared;
      std::set_intersection(neededVars.begin(), neededVars.end(), 
                            dependencyVars.begin(), dependencyVars.end(),
                            std::back_inserter(shared));

      if (! shared.empty()) {
        // shared variables found, meaning that the current node introduces a variable needed
        // by our calculation. we cannot move the calculation up the chain
        break;
      }
      
      // no shared variables found. we can move the calculation up the dependency chain

      // first, delete the calculation from the plan
      plan->removeNode(n);

      // fiddle dependencies of calculation node
      n->removeDependencies();
      n->addDependency(deps[0]);
      n->invalidateVarUsage();

      // fiddle dependencies of current node
      current->removeDependency(deps[0]);
      current->addDependency(n);
      current->invalidateVarUsage();
      deps[0]->invalidateVarUsage();
            
      for (auto it = deps.begin(); it != deps.end(); ++it) {
        stack.push_back((*it));
      }
    }
  }

  plan->findVarUsage();

  keep = true; 
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are never needed
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalculationsRule (Optimizer* opt, 
                                                      ExecutionPlan* plan, 
                                                      Optimizer::PlanList& out, 
                                                      bool& keep) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION);
  std::unordered_set<ExecutionNode*> toRemove;
  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow()) {
      // If this node can throw, we must not optimize it away!
      continue;
    }

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

////////////////////////////////////////////////////////////////////////////////
/// @brief find nodes of a certain type
////////////////////////////////////////////////////////////////////////////////

class CalculationNodeFinder : public WalkerWorker<ExecutionNode> {
  RangesInfo* _ranges; 
  ExecutionPlan* _plan;
  Variable const* _var;
  //EnumerateCollectionNode const* _enumColl;

  public:
    CalculationNodeFinder (ExecutionPlan* plan, Variable const * var, Optimizer::PlanList& out) 
      : _plan(plan), _var(var){
        _ranges = new RangesInfo();
    };

    void before (ExecutionNode* en) {
      if (en->getType() == triagens::aql::ExecutionNode::CALCULATION) {
        auto outvar = en->getVariablesSetHere();
        TRI_ASSERT(outvar.size() == 1);
        if(outvar[0]->id == _var->id){
          auto node = static_cast<CalculationNode*>(en);
          std::string attr;
          std::string enumCollVar;
          buildRangeInfo(node->expression()->node(), enumCollVar, attr);
        }
      }
      else if (en->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION) {
        auto node = static_cast<EnumerateCollectionNode*>(en);
        auto var = node->getVariablesSetHere()[0];  // should only be 1
        auto map = _ranges->find(var->name);        // check if we have any ranges with this var
        
        if (map != nullptr) {
          // check the first components of <map> against indexes of <node> . .
          std::vector<std::string> attrs;
          for (auto str : *map){
            attrs.push_back(str.first);
          }
          std::vector<TRI_index_t*> idxs = node->getIndexes(attrs);
          if (idxs.size() != 0) { //use RangeIndexNode . . . 
            std::cout << "FOUND INDEX!\n";
           
           // copy/clone the plan . . . 
           // keep map from the old nodes to the new nodes . . .
           // create RangeIndexNode
           // remove the enumerate coll node
           // remember the previous node 
           // make dependencies of new RangeIndexNode equal to the dep of
           // deleted enumerate coll node, make dependencies of previous node
           // equal to new RangeIndexNode . . .
           // push_back to out

            /*
            // delete the enumerate collection node (not any other nodes) from the plan . . .
            plan->removeNode(node);
            
            auto deps = node->getDependencies();
            
            // make one new plan per element in <idxs>
            for (auto idx: idxs){
              

            }*/

          }
        }
      }
    }

    void buildRangeInfo (AstNode const* node, std::string& enumCollVar, std::string& attr){
      //std::vector<std::string>& attrs){
      if(node->type == NODE_TYPE_REFERENCE){
        auto x = static_cast<Variable*>(node->getData());
        auto setter = _plan->getVarSetBy(x->id);
        if( setter != nullptr && 
          setter->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION){
          enumCollVar = x->name;
          //_enumColl = static_cast<EnumerateCollectionNode*>(setter);
        }
        return;
      }
      
      if(node->type == NODE_TYPE_ATTRIBUTE_ACCESS){
        char const* attributeName = node->getStringValue();
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        if(!enumCollVar.empty()){
          attr.append(attributeName);
          attr.push_back('.');
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
          buildRangeInfo(nextNode, enumCollVar, attr);
          if(!enumCollVar.empty()){
            _ranges->insert(enumCollVar, attr.substr(0, attr.size()-1), 
                new RangeInfoBound(val, true), new RangeInfoBound(val, true));
          }
        }
        //std::cout << _ranges->toString() << "\n";
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
          buildRangeInfo(nextNode, enumCollVar, attr);
          if(!enumCollVar.empty()){
            _ranges->insert(enumCollVar, attr.substr(0, attr.size()-1), low, high);
          }
        }
        //std::cout << _ranges->toString() << "\n";
      }
      
      if(node->type == NODE_TYPE_OPERATOR_BINARY_AND){
        attr = "";
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        attr = "";
        buildRangeInfo(node->getMember(1), enumCollVar, attr);
        //std::cout << _ranges->toString() << "\n";
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
  keep = true;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER);
 
  for (auto n : nodes) {
    auto nn = static_cast<FilterNode*>(n);
    auto invars = nn->getVariablesUsedHere();
    TRI_ASSERT(invars.size() == 1);
    CalculationNodeFinder finder(plan, invars[0], out);
    nn->walk(&finder);
  }

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

