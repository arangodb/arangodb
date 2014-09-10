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
/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeRedundantSorts (Optimizer* opt, 
                                         ExecutionPlan* plan,
                                         Optimizer::Rule const* rule) { 
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(triagens::aql::ExecutionNode::SORT, true);
  std::unordered_set<ExecutionNode*> toUnlink;
 
  for (auto n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = static_cast<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation(plan);

    if (sortInfo.isValid && ! sortInfo.criteria.empty()) {
      // we found a sort that we can understand
    
      std::vector<ExecutionNode*> stack;
      for (auto dep : sortNode->getDependencies()) {
        stack.push_back(dep);
      }

      int nodesRelyingOnSort = 0;

      while (! stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == triagens::aql::ExecutionNode::SORT) {
          // we found another sort. now check if they are compatible!
    
          auto other = static_cast<SortNode*>(current)->getSortInformation(plan);

          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              // different sort criteria
              if (nodesRelyingOnSort == 0) {
                // a sort directly followed by another sort: now remove one of them

                if (other.canThrow) {
                  // if the sort can throw, we must not remove it
                  break;
                }

                if (sortNode->isStable()) {
                  // we should not optimize predecessors of a stable sort (used in a COLLECT node)
                  // the stable sort is for a reason, and removing any predecessors sorts might
                  // change the result
                  break;
                }

                // remove sort that is a direct predecessor of a sort
                toUnlink.insert(current);
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.insert(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              // the sort at the start of the pipeline makes the sort at the end
              // superfluous, so we'll remove it
              toUnlink.insert(n);
              break;
            }
            
            case SortInformation::allEqual: {
              // the sort at the end of the pipeline makes the sort at the start
              // superfluous, so we'll remove it
              toUnlink.insert(current);
              break;
            }
          }
        }
        else if (current->getType() == triagens::aql::ExecutionNode::FILTER) {
          // ok: a filter does not depend on sort order
        }
        else if (current->getType() == triagens::aql::ExecutionNode::CALCULATION) {
          // ok: a filter does not depend on sort order only if it does not throw
          if (current->canThrow()) {
            ++nodesRelyingOnSort;
          }
        }
        else if (current->getType() == triagens::aql::ExecutionNode::ENUMERATE_LIST ||
                 current->getType() == triagens::aql::ExecutionNode::ENUMERATE_COLLECTION) {
          // ok, but we cannot remove two different sorts if one of these node types is between them
          // example: in the following query, the one sort will be optimized away:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC SORT i.a DESC RETURN i
          // but in the following query, the sorts will stay:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC LET a = i.a SORT i.a DESC RETURN i
          ++nodesRelyingOnSort;
        }
        else {
          // abort at all other type of nodes. we cannot remove a sort beyond them
          // this include COLLECT and LIMIT
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
      }
    }
  }
            
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule->level, ! toUnlink.empty());

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
                                                 Optimizer::Rule const* rule) {
  bool modified = false;
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
      modified = true;
    }
    else {
      // filter is always false
      // now insert a NoResults node below it
      auto noResults = new NoResultsNode(plan->nextId());
      plan->registerNode(noResults);
      plan->replaceNode(n, noResults);
      modified = true;
    }
  }
  
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule->level, modified);

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
                                           Optimizer::Rule const* rule) {
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
  
  opt->addPlan(plan, rule->level, modified);

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
                                      Optimizer::Rule const* rule) {
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
  
  opt->addPlan(plan, rule->level, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are never needed
/// this modifies an existing plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalculationsRule (Optimizer* opt, 
                                                      ExecutionPlan* plan,
                                                      Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::CALCULATION, true);
  std::unordered_set<ExecutionNode*> toUnlink;
  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (nn->expression()->canThrow() ||
        ! nn->expression()->isDeterministic()) {
      // If this node can throw or is non-deterministic, we must not optimize it away!
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

  opt->addPlan(plan, rule->level, ! toUnlink.empty());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prefer IndexRange nodes over EnumerateCollection nodes
////////////////////////////////////////////////////////////////////////////////

class FilterToEnumCollFinder : public WalkerWorker<ExecutionNode> {
  RangesInfo* _ranges;
  Optimizer* _opt;
  ExecutionPlan* _plan;
  std::unordered_set<VariableId> _varIds;
  bool _canThrow; 
  Optimizer::RuleLevel _level;
  
  public:

    FilterToEnumCollFinder (Optimizer* opt,
                            ExecutionPlan* plan,
                            Variable const* var,
                            Optimizer::RuleLevel level) 
      : _opt(opt),
        _plan(plan), 
        _canThrow(false),
        _level(level) {
      _ranges = new RangesInfo();
      _varIds.insert(var->id);
    };

    ~FilterToEnumCollFinder () {
      delete _ranges;
    }

    bool before (ExecutionNode* en) {
      _canThrow = (_canThrow || en->canThrow()); // can any node walked over throw?

      switch (en->getType()) {
        case EN::ENUMERATE_LIST:
          break;
        case EN::CALCULATION: {
          auto outvar = en->getVariablesSetHere();
          TRI_ASSERT(outvar.size() == 1);
          if (_varIds.find(outvar[0]->id) != _varIds.end()) {
            auto node = static_cast<CalculationNode*>(en);
            std::string attr;
            std::string enumCollVar;
            buildRangeInfo(node->expression()->node(), enumCollVar, attr);
          }
          break;
        }
        case EN::SUBQUERY:        
          break;
        case EN::FILTER:{
          std::vector<Variable const*> inVar = en->getVariablesUsedHere();
          TRI_ASSERT(inVar.size() == 1);
          _varIds.insert(inVar[0]->id);
          break;
        }
        case EN::INTERSECTION:
        case EN::AGGREGATE:
        case EN::LOOKUP_JOIN:
        case EN::MERGE_JOIN:
        case EN::LOOKUP_INDEX_UNIQUE:
        case EN::LOOKUP_INDEX_RANGE:
        case EN::LOOKUP_FULL_COLLECTION:
        case EN::CONCATENATION:
        case EN::MERGE:
        case EN::REMOTE:
          // in these cases we simply ignore the intermediate nodes, note
          // that we have taken care of nodes that could throw exceptions
          // above.
          break;
        case EN::SINGLETON:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::ILLEGAL:
          // in all these cases something is seriously wrong and we better abort
          return true;
        case EN::LIMIT:           
          // if we meet a limit node between a filter and an enumerate
          // collection, we abort . . .
          return true;
        case EN::SORT:
        case EN::INDEX_RANGE:
          break;
        case EN::ENUMERATE_COLLECTION:{
          auto node = static_cast<EnumerateCollectionNode*>(en);
          auto var = node->getVariablesSetHere()[0];  // should only be 1
          auto map = _ranges->find(var->name);        // check if we have any ranges with this var
          
          if (map != nullptr) {
            // check the first components of <map> against indexes of <node>...
            std::unordered_set<std::string> attrs;
            
            bool valid = true;     // are all the range infos valid?

            for(auto x: *map) {
              valid &= x.second.isValid(); 
              if (!valid) {
                break;
              }
              attrs.insert(x.first);
            }

            if (! _canThrow) {
              if (! valid) { // ranges are not valid . . . 
                
                auto newPlan = _plan->clone();
                try {
                  auto parents = newPlan->getNodeById(node->id())->getParents();
                  for (auto x: parents) {
                    auto noRes = new NoResultsNode(newPlan->nextId());
                    newPlan->registerNode(noRes);
                    newPlan->insertDependency(x, noRes);
                    _opt->addPlan(newPlan, _level, true);
                  }
                }
                catch (...) {
                  delete newPlan;
                  throw;
                }
              }
              else {
                std::vector<TRI_index_t*> idxs;
                std::vector<size_t> prefixes;
                // {idxs.at(i)->_fields[0]..idxs.at(i)->_fields[prefixes.at(i)]}
                // is a subset of <attrs>

                node->getIndexesForIndexRangeNode(attrs, idxs, prefixes);

                // make one new plan for every index in <idxs> that replaces the
                // enumerate collection node with a IndexRangeNode ... 
                
                for (size_t i = 0; i < idxs.size(); i++) {
                  std::vector<std::vector<RangeInfo>> rangeInfo;
                  rangeInfo.push_back(std::vector<RangeInfo>());
                  
                  // ranges must be valid and all comparisons == if hash index or ==
                  // followed by a single <, >, >=, or <= if a skip index in the
                  // order of the fields of the index.
                  auto idx = idxs.at(i);
                  if (idx->_type == TRI_IDX_TYPE_HASH_INDEX) {
                    for (size_t j = 0; j < idx->_fields._length; j++) {
                      auto range = map->find(std::string(idx->_fields._buffer[j]));
                   
                      if (! range->second.isConstant() ||
                          ! range->second.is1ValueRangeInfo()) {
                        rangeInfo.at(0).clear();   // not usable
                        break;
                      }
                      rangeInfo.at(0).push_back(range->second);
                    }
                  }
                  
                  if (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
                    size_t j = 0;
                    auto range = map->find(std::string(idx->_fields._buffer[0]));
                    if (range->second.isConstant()) {
                      rangeInfo.at(0).push_back(range->second);
                      bool equality = range->second.is1ValueRangeInfo();
                      while (++j < prefixes.at(i) && equality) {
                        range = map->find(std::string(idx->_fields._buffer[j]));
                        if (! range->second.isConstant()) {
                          break;
                        }
                        rangeInfo.at(0).push_back(range->second);
                        equality = equality && range->second.is1ValueRangeInfo();
                      }
                    }
                  }
                  
                  if (rangeInfo.at(0).size() != 0) {
                    auto newPlan = _plan->clone();
                    try {
                      ExecutionNode* newNode = new IndexRangeNode(newPlan->nextId(), node->vocbase(), 
                        node->collection(), node->outVariable(), idx, rangeInfo);
                      newPlan->registerNode(newNode);
                      newPlan->replaceNode(newPlan->getNodeById(node->id()), newNode);
                      _opt->addPlan(newPlan, _level, true);
                    }
                    catch (...) {
                      delete newPlan;
                      throw;
                    }
                  }
                }
              }
            }
          }
          break;
        }
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
        return;
      }
      
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);
        bool found = false;
        AstNode const* val = nullptr;
        if(rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          buildRangeInfo(rhs, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            // Found a multiple attribute access of a variable
            if (lhs->type == NODE_TYPE_VALUE) {
              val = lhs;
              found = true;
            }
            else {
              enumCollVar.clear();
            }
          }
        }
        if (! found && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          buildRangeInfo(lhs, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            // Found a multiple attribute access of a variable
            if (rhs->type == NODE_TYPE_VALUE) {
              val = rhs;
              found = true;
            }
            else {
              enumCollVar.clear();
            }
          }
        }
        
        if (found) {
          _ranges->insert(enumCollVar, attr.substr(0, attr.size() - 1), 
              RangeInfoBound(val, true), RangeInfoBound(val, true), true);
        }
        attr.clear();
        enumCollVar.clear();
        return;
      }

      if(node->type == NODE_TYPE_OPERATOR_BINARY_LT || 
         node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
         node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
         node->type == NODE_TYPE_OPERATOR_BINARY_GE) {
        
        bool include = (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
                        node->type == NODE_TYPE_OPERATOR_BINARY_GE);
        
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);
        RangeInfoBound low;
        RangeInfoBound high;

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Attribute access on the right:
          // First find out whether there is a multiple attribute access
          // of a variable on the right:
          buildRangeInfo(rhs, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            // Constant value on the left, so insert a constant condition:
            if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
                node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
              high.assign(lhs, include);
            } 
            else {
              low.assign(lhs, include);
            }
            _ranges->insert(enumCollVar, attr.substr(0, attr.size()-1), 
                            low, high, false);
          }
        }
        else if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Attribute access on the left:
          // First find out whether there is a multiple attribute access
          // of a variable on the left:
          buildRangeInfo(lhs, enumCollVar, attr);
          if (! enumCollVar.empty()) {
            // Constant value on the right, so insert a constant condition:
            if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
                node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
              low.assign(rhs, include);
            } 
            else {
              high.assign(rhs, include);
            }
            _ranges->insert(enumCollVar, attr.substr(0, attr.size()-1), 
                            low, high, false);
          }
        }
      }
      
      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        buildRangeInfo(node->getMember(1), enumCollVar, attr);
      }
      /* TODO: or isn't implemented yet.
      if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
        buildRangeInfo(node->getMember(0), enumCollVar, attr);
        buildRangeInfo(node->getMember(1), enumCollVar, attr);
      }
      */
      attr = "";
      enumCollVar = "";
      return;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief useIndexRange, try to use an index for filtering
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::useIndexRange (Optimizer* opt, 
                                  ExecutionPlan* plan, 
                                  Optimizer::Rule const* rule) {
  
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::FILTER, true);
  
  for (auto n : nodes) {
    auto nn = static_cast<FilterNode*>(n);
    auto invars = nn->getVariablesUsedHere();
    TRI_ASSERT(invars.size() == 1);
    FilterToEnumCollFinder finder(opt, plan, invars[0], rule->level);
    nn->walk(&finder);
  }
  opt->addPlan(plan, rule->level, false);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyse the sortnode and its calculation nodes
////////////////////////////////////////////////////////////////////////////////

class sortAnalysis {
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
      rangeInfo.push_back(std::vector<RangeInfo>());
    }
    return std::make_pair(v, rangeInfo);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the sortNode and its referenced Calculationnodes from the plan.
////////////////////////////////////////////////////////////////////////////////
  void removeSortNodeFromPlan (ExecutionPlan *newPlan) {
    newPlan->unlinkNode(newPlan->getNodeById(sortNodeID));
  }
};

class sortToIndexNode : public WalkerWorker<ExecutionNode> {
  using ECN = triagens::aql::EnumerateCollectionNode;

  Optimizer*           _opt;
  ExecutionPlan*       _plan;
  sortAnalysis*        _sortNode;
  Optimizer::RuleLevel _level;

  public:
  bool                 planModified;


  sortToIndexNode (Optimizer* opt,
                   ExecutionPlan* plan,
                   sortAnalysis* Node,
                   Optimizer::RuleLevel level)
    : _opt(opt),
      _plan(plan),
      _sortNode(Node),
      _level(level) {
    planModified = false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief if the sort is already done by an indexrange, remove the sort.
////////////////////////////////////////////////////////////////////////////////

  bool handleIndexRangeNode (IndexRangeNode* node) {
    auto variableName = node->getVariablesSetHere()[0]->name;
    auto result = _sortNode->getAttrsForVariableName(variableName);

    if (node->MatchesIndex(result.first)) {
      _sortNode->removeSortNodeFromPlan(_plan);
      planModified = true;
    }
    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we can sort via an index.
////////////////////////////////////////////////////////////////////////////////

  bool handleEnumerateCollectionNode (EnumerateCollectionNode* node, Optimizer::RuleLevel level) {
    auto variableName = node->getVariablesSetHere()[0]->name;
    auto result = _sortNode->getAttrsForVariableName(variableName);

    if (result.first.size() == 0) {
      return true; // we didn't find anything replaceable by indice
    }

    for (auto idx: node->getIndicesOrdered(result.first)) {
      // make one new plan for each index that replaces this
      // EnumerateCollectionNode with an IndexRangeNode

      // can only use the index if it is a skip list or (a hash and we
      // are checking equality)
      auto newPlan = _plan->clone();
      try {
        ExecutionNode* newNode = new IndexRangeNode( newPlan->nextId(),
                                      node->vocbase(), 
                                      node->collection(),
                                      node->outVariable(),
                                      idx.index,/// TODO: estimate cost on match quality
                                      result.second);
        newPlan->registerNode(newNode);
        newPlan->replaceNode(newPlan->getNodeById(node->id()), newNode);

        if (idx.fullmatch) { // if the index superseedes the sort, remove it.
          _sortNode->removeSortNodeFromPlan(newPlan);
          _opt->addPlan(newPlan, Optimizer::RuleLevel::pass5, true);
        }
        else {
          _opt->addPlan(newPlan, level, true);
        }
      }
      catch (...) {
        delete newPlan;
        throw;
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
                                    Optimizer::Rule const* rule) {
  bool planModified = false;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(triagens::aql::ExecutionNode::SORT, true);
  for (auto n : nodes) {
    auto thisSortNode = static_cast<SortNode*>(n);
    sortAnalysis node(thisSortNode);
    if (node.isAnalyzeable()) {
      sortToIndexNode finder(opt, plan, &node, rule->level);
      thisSortNode->walk(&finder);/// todo auf der dependency anfangen
      if (finder.planModified) {
        planModified = true;
      }
    }
  }
  opt->addPlan(plan,
               planModified ? Optimizer::RuleLevel::pass5 : rule->level,
               planModified);

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
                                                    Optimizer::Rule const* rule) {
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

  opt->addPlan(plan, rule->level, false);
  
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
        if (! opt->addPlan(newPlan, rule->level, true)) {
          break;
        }
      }
      catch (...) {
        delete newPlan;
        throw;
      }

    } 
    while (nextPermutationTuple(permTuple, starts));
  }

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

