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
using EN   = triagens::aql::ExecutionNode;

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief dummyrule
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::dummyRule (Optimizer*,
                              ExecutionPlan*,
                              int level,
                              Optimizer::PlanList&) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeRedundantSorts (Optimizer* opt, 
                                         ExecutionPlan* plan, 
                                         int level, 
                                         Optimizer::PlanList& out) {
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::SORT, true);
  std::unordered_set<ExecutionNode*> toUnlink;
  
  for (auto n : nodes) {
    auto sortInfo = static_cast<SortNode*>(n)->getSortInformation(plan);

    if (sortInfo.isValid && ! sortInfo.criteria.empty()) {
      // we found a sort that we can understand
    
      std::vector<ExecutionNode*> stack;
      for (auto dep : n->getDependencies()) {
        stack.push_back(dep);
      }

      while (! stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == triagens::aql::ExecutionNode::SORT) {
          // we found another sort. now check if they are compatible!
          auto other = static_cast<SortNode*>(current)->getSortInformation(plan);

          if (sortInfo.isCoveredBy(other)) {
            // the sort at the start of the pipeline makes the sort at the end
            // superfluous, so we'll remove it
            toUnlink.insert(n);
            break;
          }
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
      }
    }
  }
            
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  out.push_back(plan, level);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryFiltersRule (Optimizer* opt, 
                                                 ExecutionPlan* plan, 
                                                 int level,
                                                 Optimizer::PlanList& out) {
  std::unordered_set<ExecutionNode*> toUnlink;
  // should we enter subqueries??
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
  
  out.push_back(plan, level);

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
                                           int level,
                                           Optimizer::PlanList& out) {
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION, true);
  bool modified = false;
  
  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow()) {
      // we will only move expressions up that cannot throw
      continue;
    }

    auto const neededVars = n->getVariablesUsedHere();

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
  
  out.push_back(plan, level);

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
                                      int level,
                                      Optimizer::PlanList& out) {
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
      // and re-insert into plan in front of the current node
      plan->insertDependency(current, n);
      modified = true;
    }

  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  out.push_back(plan, level);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are never needed
/// this modifies an existing plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalculationsRule (Optimizer* opt, 
                                                      ExecutionPlan* plan,
                                                      int level,
                                                      Optimizer::PlanList& out) {
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

  out.push_back(plan, level);

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
          rangeInfo.push_back(std::vector<RangeInfo*>());
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
            std::cout << "INVALID RANGE!\n";
              
              auto newPlan = _plan->clone();
              auto parents = node->getParents();
              for(auto x: parents) {
                auto noRes = new NoResultsNode(newPlan->nextId());
                newPlan->registerNode(noRes);
                newPlan->insertDependency(x, noRes);
                _out->push_back(newPlan, 0);
              }
            }
            else {
              std::vector<TRI_index_t*> idxs = node->getIndicesUnordered(attrs);
              // make one new plan for every index in <idxs> that replaces the
              // enumerate collection node with a RangeIndexNode . . . 
              for (auto idx: idxs) {
                if ( idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX || 
                   (idx->_type == TRI_IDX_TYPE_HASH_INDEX && eq)) {
                  //can only use the index if it is a skip list or (a hash and we
                  //are checking equality)
                  std::cout << "FOUND INDEX!\n";
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
                  _out->push_back(newPlan, 0);
                }
              }
            }
          }
        }
      }
      else if (en->getType() == triagens::aql::ExecutionNode::LIMIT) {
        // if we meet a limit node between a filter and an enumerate collection,
        // we abort . . . 
        return true;
      }
      
      return false;
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
                                  int level,
                                  Optimizer::PlanList& out) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER, true);
 
  for (auto n : nodes) {
    auto nn = static_cast<FilterNode*>(n);
    auto invars = nn->getVariablesUsedHere();
    TRI_ASSERT(invars.size() == 1);
    FilterToEnumCollFinder finder(plan, invars[0], &out);
    nn->walk(&finder);
  }

  out.push_back(plan, level);

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief analyse the sortnode and its calculation nodes
////////////////////////////////////////////////////////////////////////////////
class sortAnalysis
{
  using ECN = triagens::aql::EnumerateCollectionNode;

  typedef std::pair<ECN::IndexMatchVec, RangeInfoVec> Range_IndexPair;

  struct sortNodeData {
    bool ASC;
    size_t calculationNodeID;
    std::string variableName;
    std::string attributevec;
  };

  std::vector<sortNodeData*> _sortNodeData;

public:
  size_t const sortNodeID;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor; fetches the referenced calculation nodes and builds 
///   _sortNodeData for later use.
////////////////////////////////////////////////////////////////////////////////
  sortAnalysis (SortNode * node)
  : sortNodeID(node->id())
  {
    auto sortParams = node->getCalcNodePairs();

    for (size_t n = 0; n < sortParams.size(); n++) {
      auto d = new sortNodeData;
      d->ASC = sortParams[n].second;
      d->calculationNodeID = sortParams[n].first->id();

      if (sortParams[n].first->getType() == EN::CALCULATION) {
        auto cn = static_cast<triagens::aql::CalculationNode*>(sortParams[n].first);
        auto oneSortExpression = cn->expression();
        
        if (oneSortExpression->isAttributeAccess()) {
          auto simpleExpression = oneSortExpression->getMultipleAttributes();
          d->variableName = simpleExpression.first;
          d->attributevec = simpleExpression.second;
        }
      }
      _sortNodeData.push_back(d);
    }
  }

  ~sortAnalysis () {
    for (auto x : _sortNodeData){
      delete x;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the whether we only have simple calculation nodes
////////////////////////////////////////////////////////////////////////////////
  bool isAnalyzeable () {
    if (_sortNodeData.size() == 0) {
      return false;
    }
    size_t j;
    for (j = 0; j < _sortNodeData.size(); j ++) {
      if (_sortNodeData[j]->variableName.length() == 0) {
        return false;
      }
    }

    /*  are we all from one variable? * /
    int j = 0;
    for (; (j < _sortNodeData.size() && 
            sortNodeData[j]->variableName.length() == 0);
         j ++);
    last = sortNodeData[j];
    j ++;
    for (j < _sortNodeData.size(); j++) {
      if (sortNodeData[j]->variableName.length)
      if (last->variableName != sortNodeData[j]->variableName) {
        return false;
      }
      last = sortNodeData[j];
    }
     alle nodes gesetzt, ja.
    */ 
    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether our calculation nodes reference variableName; 
/// @returns pair used for further processing with the indices.
////////////////////////////////////////////////////////////////////////////////
  Range_IndexPair getAttrsForVariableName (std::string &variableName) {
    ECN::IndexMatchVec v;
    RangeInfoVec rangeInfo;

    for (size_t j = 0; j < _sortNodeData.size(); j ++) {
      if (_sortNodeData[j]->variableName != variableName) {
        return std::make_pair(v, rangeInfo); // for now, no mixed support.
      }
    }
    for (size_t j = 0; j < _sortNodeData.size(); j ++) {
      v.push_back(std::make_pair(_sortNodeData[j]->attributevec,
                                 _sortNodeData[j]->ASC));
      rangeInfo.push_back(std::vector<RangeInfo*>());

      rangeInfo.at(j).push_back(new RangeInfo(variableName,
                                              _sortNodeData[j]->attributevec,
                                              nullptr, nullptr));
    }
    return std::make_pair(v, rangeInfo);;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the sortNode and its referenced Calculationnodes from the plan.
////////////////////////////////////////////////////////////////////////////////
  void removeSortNodeFromPlan (ExecutionPlan *newPlan) {
    newPlan->unlinkNode(newPlan->getNodeById(sortNodeID));

    for (auto idToRemove = _sortNodeData.begin();
         idToRemove != _sortNodeData.end();
         ++idToRemove) {
      newPlan->unlinkNode(newPlan->getNodeById((*idToRemove)->calculationNodeID));
    }
  }
};

class sortToIndexNode : public WalkerWorker<ExecutionNode> {
  using ECN = triagens::aql::EnumerateCollectionNode;

  ExecutionPlan*       _plan;
  Optimizer::PlanList& _out;
  sortAnalysis*        _sortNode;
  int                  _level;

  public:
  sortToIndexNode (ExecutionPlan* plan,
                   Optimizer::PlanList& out,
                   sortAnalysis* Node,
                   int level)
    : _plan(plan),
      _out(out),
      _sortNode(Node),
      _level(level) {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief if the sort is already done by an indexrange, remove the sort.
////////////////////////////////////////////////////////////////////////////////
  bool handleIndexRangeNode(IndexRangeNode* node) {
    auto variableName = node->getVariablesSetHere()[0]->name;
    auto result = _sortNode->getAttrsForVariableName(variableName);

    if (node->MatchesIndex(result.first)) {
        _sortNode->removeSortNodeFromPlan(_plan);
    }
    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we can sort via an index.
////////////////////////////////////////////////////////////////////////////////
  bool handleEnumerateCollectionNode(EnumerateCollectionNode* node, int level)
  {
    auto variableName = node->getVariablesSetHere()[0]->name;
    auto result = _sortNode->getAttrsForVariableName(variableName);

    if (result.first.size() == 0) {
      return false; // we didn't find anything replaceable by indice
    }

    for (auto idx: node->getIndicesOrdered(result.first)) {
      // make one new plan for each index that replaces this
      // EnumerateCollectionNode with an IndexRangeNode

      //can only use the index if it is a skip list or (a hash and we
      //are checking equality)
      auto newPlan = _plan->clone();
      ExecutionNode* newNode = nullptr;
      try {
        newNode = new IndexRangeNode( newPlan->nextId(),
                                      node->vocbase(), 
                                      node->collection(),
                                      node->outVariable(),
                                      idx.index,/// TODO: estimate cost on match quality
                                      result.second);
        newPlan->registerNode(newNode);
      }
      catch (...) {
        delete newNode;
        delete newPlan;
        throw;
      }

      newPlan->replaceNode(newPlan->getNodeById(node->id()), newNode);

      if (idx.fullmatch) { // if the index superseedes the sort, remove it.
        _sortNode->removeSortNodeFromPlan(newPlan);
      }
      _out.push_back(newPlan, level);
    }
    for (auto x : result.second) {
      for (auto y : x) {
        delete y;
      }
    }
    return true;
  }

  bool enterSubQuery () { return false; }

  bool before (ExecutionNode* en) {
    switch (en->getType()) {
    case EN::ENUMERATE_LIST:
    case EN::CALCULATION:
    case EN::SUBQUERY:        /// TODO: find out whether it may throw
    case EN::FILTER:
      return false;                           // skip. we don't care.

    case EN::INTERSECTION:
    case EN::SINGLETON:
    case EN::AGGREGATE:
    case EN::LOOKUP_JOIN:
    case EN::MERGE_JOIN:
    case EN::LOOKUP_INDEX_UNIQUE:
    case EN::LOOKUP_INDEX_RANGE:
    case EN::LOOKUP_FULL_COLLECTION:
    case EN::CONCATENATION:
    case EN::MERGE:
    case EN::REMOTE:
    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::RETURN:
    case EN::NORESULTS:
    case EN::ILLEGAL:
    case EN::LIMIT:                      // LIMIT is criterion to stop
      return true;  // abort.

    case EN::SORT:     // pulling two sorts together is done elsewhere.
      return en->id() != _sortNode->sortNodeID;    // ignore ourselves.

    case EN::INDEX_RANGE:
      return handleIndexRangeNode(static_cast<IndexRangeNode*>(en));

    case EN::ENUMERATE_COLLECTION:
      return handleEnumerateCollectionNode(static_cast<EnumerateCollectionNode*>(en), _level);
    }
    return true;
  }
};

int triagens::aql::useIndexForSort (Optimizer* opt, 
                                    ExecutionPlan* plan,
                                    int level,
                                    Optimizer::PlanList& out) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::SORT, true);
  for (auto n : nodes) {
    auto thisSortNode = static_cast<SortNode*>(n);
    sortAnalysis node(thisSortNode);
    if (node.isAnalyzeable()) {
      sortToIndexNode finder(plan, out, &node, level);
      thisSortNode->walk(&finder);/// todo auf der dependency anfangen
    }
  }

  out.push_back(plan, level);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to compute lots of permutation tuples
/// a permutation tuple is represented as a single vector together with
/// another vector describing the boundaries of the tuples.
/// Example:
/// data:   0,1,2, 3,4, 5,6
/// starts: 0,     3,   5,      (indices of starts of sections)
/// means a tuple of 3 permutations of 3, 2 and 2 points respectively
/// This function computes the next permutation tuple among the 
/// lexicographically sorted list of all such tuples. It returns true
/// if it has successfully computed this and false if the tuple is already
/// the lexicographically largest one. If false is returned, the permutation
/// tuple is back to the beginning.
////////////////////////////////////////////////////////////////////////////////

static bool nextPermutationTuple (std::vector<size_t>& data,
                                  std::vector<size_t>& starts) {
  auto begin = data.begin();  // a random access iterator
  for (size_t i = starts.size(); i-- != 0; ) {
    std::vector<size_t>::iterator from = begin + starts[i];
    std::vector<size_t>::iterator to;
    if (i == starts.size()-1) {
      to = data.end();
    }
    else {
      to = begin + starts[i+1];
    }
    if (std::next_permutation(from, to)) {
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::interchangeAdjacentEnumerations (Optimizer* opt,
                                                    ExecutionPlan* plan,
                                                    int level,
                                                    Optimizer::PlanList& out) {

  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::ENUMERATE_COLLECTION, 
                            true);
  std::unordered_set<ExecutionNode*> nodesSet;
  for (auto n : nodes) {
    TRI_ASSERT(nodesSet.find(n) == nodesSet.end());
    nodesSet.insert(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto n : nodes) {

    if (nodesSet.find(n) != nodesSet.end()) {
      std::vector<ExecutionNode*> nn;
      nn.push_back(n);
      nodesSet.erase(n);

      // Now follow the dependencies as long as we see further such nodes:
      auto nwalker = n;
      while (true) {
        auto deps = nwalker->getDependencies();
        if (deps.size() == 0) {
          break;
        }
        if (deps[0]->getType() != 
            triagens::aql::ExecutionNode::ENUMERATE_COLLECTION) {
          break;
        }
        nwalker = deps[0];
        nn.push_back(nwalker);
        nodesSet.erase(nwalker);
      }
      if (nn.size() > 1) {
        // Move it into the permutation tuple:
        starts.push_back(permTuple.size());
        for (auto nnn : nn) {
          nodesToPermute.push_back(nnn);
          permTuple.push_back(permTuple.size());
        }
      }
    }
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  out.push_back(plan, level);
  if (! starts.empty()) {
    nextPermutationTuple(permTuple, starts);  // will never return false
    do {
      // Clone the plan:
      auto newPlan = plan->clone();

      try {   // get rid of plan if any of this fails
        // Find the nodes in the new plan corresponding to the ones in the
        // old plan that we want to permute:
        std::vector<ExecutionNode*> newNodes;
        for (size_t j = 0; j < nodesToPermute.size(); j++) {
          newNodes.push_back(newPlan->getNodeById(nodesToPermute[j]->id()));
        }

        // Now get going with the permutations:
        for (size_t i = 0; i < starts.size(); i++) {
          size_t lowBound = starts[i];
          size_t highBound = (i < starts.size()-1) 
                           ? starts[i+1]
                           : permTuple.size();
          // We need to remove the nodes 
          // newNodes[lowBound..highBound-1] in newPlan and replace
          // them by the same ones in a different order, given by
          // permTuple[lowBound..highBound-1].
          auto parents = newNodes[lowBound]->getParents();
          TRI_ASSERT(parents.size() == 1);
          auto parent = parents[0];  // needed for insertion later

          // Unlink all those nodes:
          for (size_t j = lowBound; j < highBound; j++) {
            newPlan->unlinkNode(newNodes[j]);
          }

          // And insert them in the new order:
          for (size_t j = highBound; j-- != lowBound; ) {
            newPlan->insertDependency(parent, newNodes[permTuple[j]]);
          }
        }

        // OK, the new plan is ready, let's report it:
        out.push_back(newPlan, level);

        // Stop if this gets out of hand:
        if (out.size() > opt->maxNumberOfPlans) {
          break;
        }
      }
      catch (...) {
        delete newPlan;
        throw;
      }

    } while(nextPermutationTuple(permTuple, starts));
  }

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

