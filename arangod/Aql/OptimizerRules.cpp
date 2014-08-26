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
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryFiltersRule (Optimizer* opt, 
                                                 ExecutionPlan* plan, 
                                                 Optimizer::PlanList& out,
                                                 bool& keep) {
  keep = true; // plan will always be kept
  std::unordered_set<ExecutionNode*> toUnlink;
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER, true);
  
  for (auto n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || 
        setter->getType() != triagens::aql::ExecutionNode::CALCULATION) {
      // filter variable was not introduced by a calculation. 
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the expression
    auto s = static_cast<CalculationNode*>(setter);
    auto root = s->expression()->node();

    if (! root->isConstant()) {
      // filter expression can only be evaluated at runtime
      continue;
    }

    // filter expression is constant and thus cannot throw
    // we can now evaluate it safely
    TRI_ASSERT(! s->expression()->canThrow());

    if (root->toBoolean()) {
      // filter is always true
      // remove filter node and merge with following node
      toUnlink.insert(n);
    }
    else {
      // filter is always false
      // now insert a NoResults node below it
      auto noResults = new NoResultsNode(plan->nextId());
      plan->registerNode(noResults);
      plan->replaceNode(n, noResults);
    }
  }
  
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to 
/// avoid redundant calculations in inner loops
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::moveCalculationsUpRule (Optimizer* opt, 
                                           ExecutionPlan* plan, 
                                           Optimizer::PlanList& out,
                                           bool& keep) {
  keep = true; // plan will always be kept
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION, true);
  bool modified = false;
  
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
    for (auto dep : n->getDependencies()) {
      stack.push_back(dep);
    }

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool found = false;

      auto&& varsSet = current->getVariablesSetHere();
      for (auto v : varsSet) {
        for (auto it = neededVars.begin(); it != neededVars.end(); ++it) {
          if ((*it)->id == v->id) {
            // shared variable, cannot move up any more
            found = true;
            break;
          }
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }
        
      auto deps = current->getDependencies();
      if (deps.size() != 1) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note: this will also handle Singleton nodes
        break;
      }
      
      for (auto dep : deps) {
        stack.push_back(dep);
      }

      // first, unlink the calculation from the plan
      plan->unlinkNode(n);
      // and re-insert into before the current node
      plan->insertDependency(current, n);
      modified = true;
    }

  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::moveFiltersUpRule (Optimizer* opt, 
                                      ExecutionPlan* plan, 
                                      Optimizer::PlanList& out,
                                      bool& keep) {
  keep = true; // plan will always be kept
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER, true);
  bool modified = false;
  
  for (auto n : nodes) {
    auto neededVars = n->getVariablesUsedHere();
    TRI_ASSERT(neededVars.size() == 1);

    std::vector<ExecutionNode*> stack;
    for (auto dep : n->getDependencies()) {
      stack.push_back(dep);
    }

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == triagens::aql::ExecutionNode::LIMIT) {
        // cannot push a filter beyond a LIMIT node
        break;
      }

      bool found = false;

      auto&& varsSet = current->getVariablesSetHere();
      for (auto v : varsSet) {
        for (auto it = neededVars.begin(); it != neededVars.end(); ++it) {
          if ((*it)->id == v->id) {
            // shared variable, cannot move up any more
            found = true;
            break;
          }
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }
        
      auto deps = current->getDependencies();
      if (deps.size() != 1) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note: this will also handle Singleton nodes
        break;
      }
      
      for (auto dep : deps) {
        stack.push_back(dep);
      }

      // first, unlink the filter from the plan
      plan->unlinkNode(n);
      // and re-insert into before the current node
      plan->insertDependency(current, n);
      modified = true;
    }

  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are never needed
/// this modifies an existing plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalculationsRule (Optimizer* opt, 
                                                      ExecutionPlan* plan, 
                                                      Optimizer::PlanList& out, 
                                                      bool& keep) {
  keep = true;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION, true);
  std::unordered_set<ExecutionNode*> toUnlink;
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
      toUnlink.insert(n);
    }
  }

  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prefer IndexRange nodes over EnumerateCollection nodes
////////////////////////////////////////////////////////////////////////////////

class FilterToEnumCollFinder : public WalkerWorker<ExecutionNode> {
  RangesInfo* _ranges; 
  ExecutionPlan* _plan;
  Variable const* _var;
  Optimizer::PlanList* _out;
  bool _canThrow; 
  
  public:

    FilterToEnumCollFinder (ExecutionPlan* plan, Variable const * var, Optimizer::PlanList* out) 
      : _plan(plan), 
        _var(var), 
        _out(out), 
        _canThrow(false) {
      _ranges = new RangesInfo();

      // TODO: who is going to free _ranges??
    };

    ~FilterToEnumCollFinder () {
      delete _ranges;
    }

    bool before (ExecutionNode* en) {
      _canThrow = (_canThrow || en->canThrow()); // can any node walked over throw?

      if (en->getType() == triagens::aql::ExecutionNode::CALCULATION) {
        auto outvar = en->getVariablesSetHere();
        TRI_ASSERT(outvar.size() == 1);
        if (outvar[0]->id == _var->id) {
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
          // check the first components of <map> against indexes of <node> . . .
          std::vector<std::string> attrs;
          std::vector<std::vector<RangeInfo*>> rangeInfo;
          bool valid = true;
          bool eq = true;
          for (auto x : *map) {
            attrs.push_back(x.first);
            rangeInfo.at(0).push_back(x.second);
            valid = valid && x.second->_valid; // check the ranges are all valid
            eq = eq && x.second->is1ValueRangeInfo(); 
            // check if the condition is equality (i.e. the ranges only contain
            // 1 value)
          }
          if (! _canThrow) {
            if (! valid){ // ranges are not valid . . . 
            // std::cout << "INVALID RANGE!\n";
              
              auto newPlan = _plan->clone();
              auto parents = node->getParents();
              for(auto x: parents) {
                auto noRes = new NoResultsNode(newPlan->nextId());
                newPlan->registerNode(noRes);
                newPlan->insertDependency(x, noRes);
                _out->push_back(newPlan);
              }
            }
            else {
              std::vector<TRI_index_t*> idxs = node->getIndexes(attrs);
              // make one new plan for every index in <idxs> that replaces the
              // enumerate collection node with a RangeIndexNode . . . 
              for (auto idx: idxs) {
                if ( idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX || 
                   (idx->_type == TRI_IDX_TYPE_HASH_INDEX && eq)) {
                  //can only use the index if it is a skip list or (a hash and we
                  //are checking equality)
                  // std::cout << "FOUND INDEX!\n";
                  auto newPlan = _plan->clone();
                  ExecutionNode* newNode = nullptr;
                  try{
                    newNode = new IndexRangeNode(newPlan->nextId(), node->vocbase(), 
                        node->collection(), node->outVariable(), idx, rangeInfo);
                    newPlan->registerNode(newNode);
                  }
                  catch (...) {
                    if (newNode != nullptr) {
                      delete newNode;
                    }
                    delete newPlan;
                    throw;
                  }
                  newPlan->replaceNode(newPlan->getNodeById(node->id()), newNode);
                  _out->push_back(newPlan);
                }
              }
            }
          }
        }
      }
      return true;
    }

    void buildRangeInfo (AstNode const* node, std::string& enumCollVar, std::string& attr) {
      if (node->type == NODE_TYPE_REFERENCE) {
        auto x = static_cast<Variable*>(node->getData());
        auto setter = _plan->getVarSetBy(x->id);
        if (setter != nullptr && 
            setter->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION) {
          enumCollVar = x->name;
        }
        return;
      }
      
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        char const* attributeName = node->getStringValue();
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        if (! enumCollVar.empty()) {
          attr.append(attributeName);
          attr.push_back('.');
        }
      }
      
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);
        AstNode const* val;
        AstNode const* nextNode;
        if(rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->type == NODE_TYPE_VALUE) {
          val = lhs;
          nextNode = rhs;
        }
        else if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->type == NODE_TYPE_VALUE) {
          val = rhs;
          nextNode = lhs;
        }
        else {
          val = nullptr;
        }
        
        if (val != nullptr) {
          buildRangeInfo(nextNode, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            _ranges->insert(enumCollVar, attr.substr(0, attr.size() - 1), 
                new RangeInfoBound(val, true), new RangeInfoBound(val, true));
          }
        }
      }

      if(node->type == NODE_TYPE_OPERATOR_BINARY_LT || 
         node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
         node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
         node->type == NODE_TYPE_OPERATOR_BINARY_GE) {
        
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
          } 
          else {
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
          } 
          else {
            high = new RangeInfoBound(rhs, include);
            low = nullptr;
          }
          nextNode = lhs;
        }
        else {
          low = nullptr;
          high = nullptr;
        }

        if (low != nullptr || high != nullptr) {
          buildRangeInfo(nextNode, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            _ranges->insert(enumCollVar, attr.substr(0, attr.size()-1), low, high);
          }
        }
      }
      
      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        attr = "";
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        attr = "";
        buildRangeInfo(node->getMember(1), enumCollVar, attr);
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
    = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER, true);
 
  for (auto n : nodes) {
    auto nn = static_cast<FilterNode*>(n);
    auto invars = nn->getVariablesUsedHere();
    TRI_ASSERT(invars.size() == 1);
    FilterToEnumCollFinder finder(plan, invars[0], &out);
    nn->walk(&finder);
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief try to match sorts to indices
////////////////////////////////////////////////////////////////////////////////

class sortToIndexNode : public WalkerWorker<ExecutionNode> {
  RangesInfo* _ranges; 
  ExecutionPlan* _plan;
  std::vector<Variable const*> _vars;
  std::vector<CalculationNode*> _myVars;
  std::vector<size_t> _idsToRemove;
  Optimizer::PlanList _out;
  size_t _executionNodesFound;

  public:
    sortToIndexNode (ExecutionPlan* plan,
                     std::vector<Variable const*>& vars,
                     Optimizer::PlanList& out,
                     size_t sortNodeID) 
      : _plan(plan),
        _vars(vars),
        ///        _idsToRemove(sortNodeID),
        _out(out),
        _executionNodesFound(0) {
      _ranges = new RangesInfo();
      // TODO: who is going to free _ranges??
      _myVars.reserve(vars.size());
      _idsToRemove.push_back(sortNodeID);
    }

    bool before (ExecutionNode* en) {
      std::cout << "type:" << en->getTypeString() << "\n";
      size_t n = _vars.size();
      auto outvar = en->getVariablesSetHere();

      if ((_executionNodesFound < n) &&
          en->getType() == triagens::aql::ExecutionNode::CALCULATION) {

        // Look up whether this is one of the calculation nodes we reference.
        for (size_t i = 0; i < n; i++) {
          if (_vars[i]->id == outvar[0]->id) {
            _myVars[i] = static_cast<triagens::aql::CalculationNode*>(en);
            _executionNodesFound++;
            _idsToRemove.push_back (en->id());
            break;
          }
        }
        if (_executionNodesFound == n) {
          // ok we got all, study them.
          // TODO
        }
      }
      else if (_executionNodesFound == n) {
        if (en->getType() == triagens::aql::ExecutionNode::FILTER) {
          ///      TODO: check whether to ABORT here?
        }
        if (en->getType() == triagens::aql::ExecutionNode::SORT) {
          // TODO: subsequent sort - check whether its still needed.
        }
        if (en->getType() == triagens::aql::ExecutionNode::INDEX_RANGE) {
          // TODO: we should also match INDEX_RANGE later on.
        }
        else if (en->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION) {
          std::cout << "blub\n";
          auto JsonPlan = _plan->toJson(TRI_UNKNOWN_MEM_ZONE, false);
          auto JsonString = JsonPlan.toString();
          std::cout <<"Old Plan: \n" << JsonString << "\n";

          std::vector<std::vector<RangeInfo*>> rangeInfo;

          std::vector<std::string> attributeVector;

          std::vector<std::string> attrs;
          
          auto node = static_cast<EnumerateCollectionNode*>(en);
          auto var = node->getVariablesSetHere()[0];  // should only be 1
          auto exp = _myVars[0]->expression();

          if (!exp->isSimple()) {
            return true;
          }

          auto expNode = exp->node();
          
          // digg through nested Attributes:
          while (expNode->type == triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
            std::cout  << expNode->getStringValue() << " \n";
            attributeVector.push_back(expNode->getStringValue());
            expNode = expNode->getMember (0);
          }

          // And concatenate them again in reverse order:
          std::string attributeVectorStr = "";
          for (auto oneAttr = attributeVector.rbegin();
               oneAttr != attributeVector.rend();
               ++oneAttr) {
            if (attributeVectorStr.size() > 0)
              attributeVectorStr += std::string(".");
            attributeVectorStr += *oneAttr;
          }
          // we now should have the Collection Reference:
          std::cout  << attributeVectorStr << var->name << " \n";
          if (expNode->type != triagens::aql::NODE_TYPE_REFERENCE) {
            return true;
          }
          auto subVar = static_cast<Variable*>(expNode->getData());
          if (subVar->name != var->name) {
            // No, the requested collection is not a reference to this.
            return true;
          }
          expNode = exp->node();
          
          std::cout << expNode->getStringValue() << " -- " << var->name << " \n";

          attrs.push_back(attributeVectorStr);

          std::vector<TRI_index_t*> idxs = node->getIndexes(attrs);

          rangeInfo.at(0).push_back(new RangeInfo(var->name, 
                expNode->getStringValue(), nullptr, nullptr));
          // make one new plan for every index in <idxs> that replaces the
          // enumerate collection node with a RangeIndexNode . . . 
          if (idxs.size() == 0) {
            return true;
          }
            
          for (auto idx: idxs) {
            if ((idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) || 
                (idx->_type == TRI_IDX_TYPE_HASH_INDEX) ) {
              //can only use the index if it is a skip list or (a hash and we
              //are checking equality)
              std::cout << "FOUND INDEX!\n";

              auto newPlan = _plan->clone();
              ExecutionNode* newNode = nullptr;
              try{
                newNode = new IndexRangeNode( newPlan->nextId(), node->vocbase(), 
                                              node->collection(),
                                              node->outVariable(), idx,
                                              rangeInfo);
                newPlan->registerNode(newNode);
              }
              catch (...) {
                if (newNode != nullptr) {
                  delete newNode;
                }
                delete newPlan;
                throw;
              }
              newPlan->replaceNode(newPlan->getNodeById(node->id()), newNode);

              auto JsonPlan = newPlan->toJson(TRI_UNKNOWN_MEM_ZONE, false);
              auto JsonString = JsonPlan.toString();
              std::cout <<"New Plan: \n" << JsonString << "\n";

              for (auto idToRemove = _idsToRemove.begin();
                   idToRemove != _idsToRemove.end();
                   ++idToRemove) {
                newPlan->unlinkNode(newPlan->getNodeById(*idToRemove));
              }
              JsonPlan = newPlan->toJson(TRI_UNKNOWN_MEM_ZONE, false);
              JsonString = JsonPlan.toString();
              std::cout <<"removed foo \n" << JsonString << "\n";
              
              _out.push_back(newPlan);

            }
          }

        }
      }
      return false;
    }
};
  



int triagens::aql::useIndexForSort (Optimizer* opt, 
                                    ExecutionPlan* plan, 
                                    Optimizer::PlanList& out,
                                    bool& keep) {
  keep = true;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::SORT, true);
  for (auto n : nodes) {
    auto oneNode = static_cast<SortNode*>(n);
    auto invars = oneNode->getVariablesUsedHere();
    ////TRI_ASSERT(invars.size() == 1);/// todo: do we care about the invars? <- yes there may be more.
    //Json json = Json(Json::List, 0);
    //oneNode->toJsonHelper(json, TRI_UNKNOWN_MEM_ZONE, false);
    //std::cout << " original sort node:" << json.toString () << "\n";
    sortToIndexNode finder(plan, invars, out, oneNode->id());
    ///_thisNode = oneNode;
    oneNode->walk(&finder);
  }

  return TRI_ERROR_NO_ERROR;


}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

