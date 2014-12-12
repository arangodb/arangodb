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
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using EN   = triagens::aql::ExecutionNode;

//#if 0
#define ENTER_BLOCK try { (void) 0;
#define LEAVE_BLOCK } catch (...) { std::cout << "caught an exception in " << __FUNCTION__ << ", " << __FILE__ << ":" << __LINE__ << "!\n"; throw; }
//#else
//#define ENTER_BLOCK
//#define LEAVE_BLOCK
//#endif

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeRedundantSortsRule (Optimizer* opt, 
                                             ExecutionPlan* plan,
                                             Optimizer::Rule const* rule) { 
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::SORT, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
 
  for (auto n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = static_cast<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation(plan, &buffer);

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

        if (current->getType() == EN::SORT) {
          // we found another sort. now check if they are compatible!
    
          auto other = static_cast<SortNode*>(current)->getSortInformation(plan, &buffer);

          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              // different sort criteria
              if (nodesRelyingOnSort == 0) {
                // a sort directly followed by another sort: now remove one of them

                if (other.canThrow || ! other.isDeterministic) {
                  // if the sort can throw or is non-deterministic, we must not remove it
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
        else if (current->getType() == EN::FILTER) {
          // ok: a filter does not depend on sort order
        }
        else if (current->getType() == EN::CALCULATION) {
          // ok: a filter does not depend on sort order only if it does not throw
          if (current->canThrow()) {
            ++nodesRelyingOnSort;
          }
        }
        else if (current->getType() == EN::ENUMERATE_LIST ||
                 current->getType() == EN::ENUMERATE_COLLECTION) {
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
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::FILTER, true);
  
  for (auto n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || 
        setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation. 
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the expression
    auto s = static_cast<CalculationNode*>(setter);
    auto root = s->expression()->node();

    TRI_ASSERT(root != nullptr);

    if (root->canThrow() || ! root->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    // filter expression is constant and thus cannot throw
    // we can now evaluate it safely
    TRI_ASSERT(! s->expression()->canThrow());

    if (root->isTrue()) {
      // filter is always true
      // remove filter node and merge with following node
      toUnlink.insert(n);
      modified = true;
    }
    else if (root->isFalse()) {
      // filter is always false
      // now insert a NoResults node below it
      auto noResults = new NoResultsNode(plan, plan->nextId());
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
/// @brief remove INTO of a COLLECT if not used
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeCollectIntoRule (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::Rule const* rule) {
  bool modified = false;
  std::unordered_set<ExecutionNode*> toUnlink;
  // should we enter subqueries??
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::AGGREGATE, true);
  
  for (auto n : nodes) {
    auto collectNode = static_cast<AggregateNode*>(n);
    TRI_ASSERT(collectNode != nullptr);

    auto outVariable = collectNode->outVariable();

    if (outVariable == nullptr) {
      // no out variable. nothing to do
      continue;
    }

    auto varsUsedLater = n->getVarsUsedLater();
    if (varsUsedLater.find(outVariable) != varsUsedLater.end()) {
      // outVariable is used later
      continue;
    }

    // outVariable is not used later. remove it!
    collectNode->clearOutVariable();
    modified = true;
  }
  
  if (modified) {
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
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::CALCULATION, true);
  bool modified = false;
  
  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow() || 
        ! nn->expression()->isDeterministic()) {
      // we will only move expressions up that cannot throw and that are deterministic
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
/// @brief split and-combined filters and break them into smaller parts
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::splitFiltersRule (Optimizer* opt, 
                                     ExecutionPlan* plan,
                                     Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::FILTER, true);
  bool modified = false;
  
  for (auto n : nodes) {
    auto const&& inVar = n->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);
    auto setter = plan->getVarSetBy(inVar[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = static_cast<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression->canThrow() || 
        ! expression->isDeterministic() ||
        expression->node()->type != NODE_TYPE_OPERATOR_BINARY_AND) {  
      continue;
    }

    std::vector<AstNode const*> stack;
    stack.push_back(expression->node());

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();
    
      if (current->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        stack.push_back(current->getMember(0));
        stack.push_back(current->getMember(1));
      }
      else {
        modified = true;
  
        ExecutionNode* calculationNode = nullptr;
        auto outVar = plan->getAst()->variables()->createTemporaryVariable();
        auto expression = new Expression(plan->getAst(), current);
        try {
          calculationNode = new CalculationNode(plan, plan->nextId(), expression, outVar);
        }
        catch (...) {
          delete expression;
          throw;
        }
        plan->registerNode(calculationNode);

        plan->insertDependency(n, calculationNode);

        auto filterNode = new FilterNode(plan, plan->nextId(), outVar);
        plan->registerNode(filterNode);

        plan->insertDependency(n, filterNode);
      }
    }

    if (modified) {
      plan->unlinkNode(n, false);
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
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::FILTER, true);
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

      if (current->getType() == EN::LIMIT) {
        // cannot push a filter beyond a LIMIT node
        break;
      }

      if (current->canThrow()) {
        // must not move a filter beyond a node that can throw
        break;
      }

      if (current->getType() == EN::CALCULATION) {
        // must not move a filter beyond a node with a non-deterministic result
        auto calculation = static_cast<CalculationNode const*>(current);
        if (! calculation->expression()->isDeterministic()) {
          break;
        }
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


class triagens::aql::RedundantCalculationsReplacer : public WalkerWorker<ExecutionNode> {

  public:

    RedundantCalculationsReplacer (std::unordered_map<VariableId, Variable const*> const& replacements)
      : _replacements(replacements) {
    }

    template<typename T>
    void replaceInVariable (ExecutionNode* en) {
      auto node = static_cast<T*>(en);
   
      node->_inVariable = Variable::replace(node->_inVariable, _replacements); 
    }

    void replaceInCalculation (ExecutionNode* en) {
      auto node = static_cast<CalculationNode*>(en);
      auto&& variables = node->expression()->variables();
          
      // check if the calculation uses any of the variables that we want to replace
      for (auto it = variables.begin(); it != variables.end(); ++it) {
        if (_replacements.find((*it)->id) != _replacements.end()) {
          // calculation uses a to-be-replaced variable
          node->expression()->replaceVariables(_replacements);
          return;
        }
      }
    }

    bool before (ExecutionNode* en) override final {
      switch (en->getType()) {
        case EN::ENUMERATE_LIST: {
          replaceInVariable<EnumerateListNode>(en);
          break;
        }
      
        case EN::RETURN: {
          replaceInVariable<ReturnNode>(en);
          break;
        }
  
        case EN::CALCULATION: {
          replaceInCalculation(en);
          break;
        }

        case EN::FILTER: {
          replaceInVariable<FilterNode>(en);
          break;
        }

        case EN::AGGREGATE: {
          auto node = static_cast<AggregateNode*>(en);
          for (auto variable : node->_aggregateVariables) {
            variable.second = Variable::replace(variable.second, _replacements);
          }
          break;
        }

        case EN::SORT: {
          auto node = static_cast<SortNode*>(en);
          for (auto variable : node->_elements) {
            variable.first = Variable::replace(variable.first, _replacements);
          }
          break;
        }

        default: {
          // ignore all other types of nodes
        }
      }

      // always continue
      return false;
    }
 
  private:
   
    std::unordered_map<VariableId, Variable const*> const& _replacements;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are repeatedly used in a query
/// (i.e. common expressions)
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeRedundantCalculationsRule (Optimizer* opt, 
                                                    ExecutionPlan* plan,
                                                    Optimizer::Rule const* rule) {
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  std::unordered_map<VariableId, Variable const*> replacements;

  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::CALCULATION, true);

  for (auto n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (! nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }
    
    auto outvar = n->getVariablesSetHere();
    TRI_ASSERT(outvar.size() == 1);

    try {
      nn->expression()->stringify(&buffer);
    }
    catch (...) {
      // expression could not be stringified (maybe because not all node types
      // are supported). this is not an error, we just skip the optimization
      buffer.reset();
      continue;
    }
     
    std::string const referenceExpression(buffer.c_str(), buffer.length());
    buffer.reset();

    std::vector<ExecutionNode*> stack;
    for (auto dep : n->getDependencies()) {
      stack.push_back(dep);
    }

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        try {
          static_cast<CalculationNode*>(current)->expression()->stringify(&buffer);
        }
        catch (...) {
          // expression could not be stringified (maybe because not all node types
          // are supported). this is not an error, we just skip the optimization
          buffer.reset();
          continue;
        }
      
        std::string const compareExpression(buffer.c_str(), buffer.length());
        buffer.reset();
        
        if (compareExpression == referenceExpression) {
          // expressions are identical
          auto outvars = current->getVariablesSetHere();
          TRI_ASSERT(outvars.size() == 1);
          
          // check if target variable is already registered as a replacement
          // this covers the following case: 
          // - replacements is set to B => C
          // - we're now inserting a replacement A => B
          // the goal now is to enter a replacement A => C instead of A => B
          auto target = outvars[0];
          while (target != nullptr) {
            auto it = replacements.find(target->id);
            if (it != replacements.end()) {
              target = (*it).second;
            }
            else {
              break;
            }
          }
          replacements.emplace(std::make_pair(outvar[0]->id, target));
          
          // also check if the insertion enables further shortcuts
          // this covers the following case:
          // - replacements is set to A => B
          // - we have just inserted a replacement B => C
          // the goal now is to change the replacement A => B to A => C
          for (auto it = replacements.begin(); it != replacements.end(); ++it) {
            if ((*it).second == outvar[0]) {
              (*it).second = target;
            }
          }
        }
      }

      if (current->getType() == EN::AGGREGATE) {
        if (static_cast<AggregateNode*>(current)->hasOutVariable()) {
          // COLLECT ... INTO is evil (tm): it needs to keep all already defined variables
          // we need to abort optimization here
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

  if (! replacements.empty()) {
    // finally replace the variables

    RedundantCalculationsReplacer finder(replacements);
    plan->root()->walk(&finder);
    plan->findVarUsage();

    opt->addPlan(plan, rule->level, true);
  }
  else {
    // no changes
    opt->addPlan(plan, rule->level, false);
  }


  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNodes and SubqueryNodes that are never needed
/// this modifies an existing plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryCalculationsRule (Optimizer* opt, 
                                                      ExecutionPlan* plan,
                                                      Optimizer::Rule const* rule) {
  std::vector<ExecutionNode::NodeType> const types = {
    EN::CALCULATION,
    EN::SUBQUERY
  };

  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(types, true);
  std::unordered_set<ExecutionNode*> toUnlink;
  for (auto n : nodes) {
    if (n->getType() == EN::CALCULATION) {
      auto nn = static_cast<CalculationNode*>(n);

      if (nn->canThrow()) {
        // If this node can throw, we must not optimize it away!
        continue;
      }
    }
    else {
      auto nn = static_cast<SubqueryNode*>(n);
      if (nn->canThrow()) {
        // subqueries that can throw must not be optimized away
        continue;
      }
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
  ExecutionPlan* _plan;
  bool _canThrow; 
  bool _modified;
  std::unordered_set<VariableId> _varIds;
  RangesInfo* _ranges;
  // The following maps ids of EnumerateCollectionNodes in the original
  // plan to an index in the (outer vector) of the _changes container.
  std::unordered_map<size_t, size_t>& _changesPlaces;
  // The outer vector is for the different ids of EnumerateCollectionNodes
  // in the original plan that could be replaced. For each one, the pair
  // contains the id of the node in the original plan and a vector 
  // that holds the possible replacements.
  std::vector<std::pair<size_t, std::vector<ExecutionNode*>>>& _changes;
  
  public:

    FilterToEnumCollFinder (ExecutionPlan* plan,
          Variable const* var,
          std::unordered_map<size_t, size_t>& changesPlaces,
          std::vector<std::pair<size_t, std::vector<ExecutionNode*>>>& changes) 
      : _plan(plan), 
        _canThrow(false),
        _modified(false),
        _varIds(),
        _ranges(new RangesInfo()),
        _changesPlaces(changesPlaces),
        _changes(changes) {

      _varIds.insert(var->id);
    };

    ~FilterToEnumCollFinder () {
      delete _ranges;
    }

    bool modified () {
      return _modified;
    }

    bool before (ExecutionNode* en) override final {
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
            Variable const* enumCollVar = nullptr;
            buildRangeInfo(node->expression()->node(), enumCollVar, attr);
          }
          break;
        }
        case EN::SUBQUERY:        
          break;
        case EN::FILTER: {
          std::vector<Variable const*> inVar = en->getVariablesUsedHere();
          TRI_ASSERT(inVar.size() == 1);
          _varIds.insert(inVar[0]->id);
          break;
        }
        case EN::AGGREGATE:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
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
        case EN::ENUMERATE_COLLECTION: {
          auto node = static_cast<EnumerateCollectionNode*>(en);
          auto var = node->getVariablesSetHere()[0];  // should only be 1
          std::unordered_map<std::string, RangeInfo>* map
              = _ranges->find(var->name);        
              // check if we have any ranges with this var

          if (map != nullptr) {
            // Remove all variable bounds that are no longer defined here:
            std::unordered_set<Variable const*> varsDefined 
                = node->getVarsValid();
            // Take out the variable we define only here, because we are
            // not allowed to use it in a variable bound expression:
            std::vector<Variable const*> varsSetHere
                = node->getVariablesSetHere();
            for (auto v : varsSetHere) {
              varsDefined.erase(v);
            }
            for (auto& x : *map) {
              auto worker = [&] (std::list<RangeInfoBound>& bounds) -> void {
                for (auto it = bounds.begin(); it != bounds.end();
                     /* no hoisting */) {
                  AstNode const* a = it->getExpressionAst(_plan->getAst());
                  std::unordered_set<Variable*> varsUsed
                      = Ast::getReferencedVariables(a);
                  bool bad = false;
                  for (auto v : varsUsed) {
                    if (varsDefined.find(const_cast<Variable const*>(v))
                        == varsDefined.end()) {
                      bad = true;
                    }
                  }
                  if (bad) {
                    it = bounds.erase(it);
                    x.second.revokeEquality();  // just to be sure
                  }
                  else {
                    it++;
                  }
                }
              };
              worker(x.second._lows);
              worker(x.second._highs);
            }
            // Now remove empty conditions:
            for (auto it = map->begin(); it != map->end(); /* no hoisting */ ) {
              if (it->second._lows.empty() &&
                  it->second._highs.empty() &&
                  ! it->second._lowConst.isDefined() &&
                  ! it->second._highConst.isDefined()) {
                it = map->erase(it);
              }
              else {
                it++;
              }
            }

            // check the first components of <map> against indexes of <node>...
            std::unordered_set<std::string> attrs;
            
            bool valid = true;     // are all the range infos valid?

            for (auto x: *map) {
              valid &= x.second.isValid(); 
              if (! valid) {
                break;
              }
              attrs.insert(x.first);
            }
               
            if (! _canThrow) {
              if (! valid) { // ranges are not valid . . . 
                auto parents = node->getParents();
                for (auto x : parents) {
                  auto noRes = new NoResultsNode(_plan, _plan->nextId());
                  _plan->registerNode(noRes);
                  _plan->insertDependency(x, noRes);
                  _modified = true;
                }
              }
              else {
                std::vector<Index*> idxs;
                std::vector<size_t> prefixes;
                // {idxs.at(i)->_fields[0]..idxs.at(i)->_fields[prefixes.at(i)]}
                // is a subset of <attrs>

                // note: prefixes are only used for skiplist indexes
                // for all other index types, the prefix value will always be 0
                node->getIndexesForIndexRangeNode(attrs, idxs, prefixes);

                // make one new plan for every index in <idxs> that replaces the
                // enumerate collection node with a IndexRangeNode ... 
                
                for (size_t i = 0; i < idxs.size(); i++) {
                  std::vector<std::vector<RangeInfo>> rangeInfo;
                  rangeInfo.push_back(std::vector<RangeInfo>());
                  
                  // ranges must be valid and all comparisons == if hash
                  // index or == followed by a single <, >, >=, or <=
                  // if a skip index in the order of the fields of the
                  // index.
                  auto idx = idxs.at(i);
                  TRI_ASSERT(idx != nullptr);
               
                  if (idx->type == TRI_IDX_TYPE_PRIMARY_INDEX) {
                    bool handled = false;
                    auto range = map->find(std::string(TRI_VOC_ATTRIBUTE_ID));
                
                    if (range != map->end()) { 
                      if (! range->second.is1ValueRangeInfo()) {
                        rangeInfo.at(0).clear();   // not usable
                      }
                      else {
                        rangeInfo.at(0).push_back(range->second);
                        handled = true;
                      }
                    }

                    if (! handled) {
                      range = map->find(std::string(TRI_VOC_ATTRIBUTE_KEY));

                      if (range != map->end()) {
                        if (! range->second.is1ValueRangeInfo()) {
                          rangeInfo.at(0).clear();   // not usable
                        }
                        else {
                          rangeInfo.at(0).push_back(range->second);
                        }
                      }
                    }
                  }
                  else if (idx->type == TRI_IDX_TYPE_HASH_INDEX) {
                    for (size_t j = 0; j < idx->fields.size(); j++) {
                      auto range = map->find(idx->fields[j]);
                   
                      if (! range->second.is1ValueRangeInfo()) {
                        rangeInfo.at(0).clear();   // not usable
                        break;
                      }
                      rangeInfo.at(0).push_back(range->second);
                    }
                  }
                  else if (idx->type == TRI_IDX_TYPE_EDGE_INDEX) {
                    bool handled = false;
                    auto range = map->find(std::string(TRI_VOC_ATTRIBUTE_FROM));
                
                    if (range != map->end()) { 
                      if (! range->second.is1ValueRangeInfo()) {
                        rangeInfo.at(0).clear();   // not usable
                      }
                      else {
                        rangeInfo.at(0).push_back(range->second);
                        handled = true;
                      }
                    }

                    if (! handled) {
                      range = map->find(std::string(TRI_VOC_ATTRIBUTE_TO));

                      if (range != map->end()) {
                        if (! range->second.is1ValueRangeInfo()) {
                          rangeInfo.at(0).clear();   // not usable
                        }
                        else {
                          rangeInfo.at(0).push_back(range->second);
                        }
                      }
                    }
                  }
                  else if (idx->type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
                    size_t j = 0;
                    auto range = map->find(idx->fields[0]);
                    TRI_ASSERT(range != map->end());
                    rangeInfo.at(0).push_back(range->second);
                    bool equality = range->second.is1ValueRangeInfo();
                    while (++j < prefixes.at(i) && equality) {
                      range = map->find(idx->fields[j]);
                      rangeInfo.at(0).push_back(range->second);
                      equality = equality && range->second.is1ValueRangeInfo();
                    }
                  }
                  
                  if (! rangeInfo.at(0).empty()) {
                    std::unique_ptr<ExecutionNode> newNode
                      (new IndexRangeNode(_plan, 
                          _plan->nextId(), node->vocbase(), node->collection(), 
                          node->outVariable(), idx, rangeInfo, false));
                    size_t place = node->id();
                    std::unordered_map<size_t, size_t>::iterator it 
                         = _changesPlaces.find(place);
                    if (it == _changesPlaces.end()) {
                      _changes.push_back(std::make_pair(place, std::vector<ExecutionNode*>()));
                      it = _changesPlaces.emplace(place, _changes.size()-1).first;
                    }
                    std::vector<ExecutionNode*>& vec = _changes[it->second].second;
                    vec.push_back(newNode.release());
                    // if all goes well, this node will be used, if an 
                    // exception happens, the destructor will free it
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

    void buildRangeInfo (AstNode const* node, 
                         Variable const*& enumCollVar,
                         std::string& attr) {

      if (node->type == NODE_TYPE_REFERENCE) {
        auto x = static_cast<Variable*>(node->getData());
        auto setter = _plan->getVarSetBy(x->id);
        if (setter != nullptr && 
            setter->getType() == EN::ENUMERATE_COLLECTION) {
          enumCollVar = x;
        }
        return;
      }
      
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        buildRangeInfo(node->getMember(0), enumCollVar, attr);

        if (enumCollVar != nullptr) {
          char const* attributeName = node->getStringValue();
          attr.append(attributeName);
          attr.push_back('.');
        }
        return;
      }
      
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          buildRangeInfo(rhs, enumCollVar, attr);
          if (enumCollVar != nullptr) {
            std::unordered_set<Variable*> varsUsed 
                = Ast::getReferencedVariables(lhs);
            if (varsUsed.find(const_cast<Variable*>(enumCollVar)) 
                == varsUsed.end()) {
              // Found a multiple attribute access of a variable and an
              // expression which does not involve that variable:
              _ranges->insert(enumCollVar->name, 
                              attr.substr(0, attr.size() - 1), 
                              RangeInfoBound(lhs, true), 
                              RangeInfoBound(lhs, true), true);
            }
            enumCollVar = nullptr;
            attr.clear();
          }
        }
          
        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          buildRangeInfo(lhs, enumCollVar, attr);
          if (enumCollVar != nullptr) {
            std::unordered_set<Variable*> varsUsed 
                = Ast::getReferencedVariables(rhs);
            if (varsUsed.find(const_cast<Variable*>(enumCollVar)) 
                == varsUsed.end()) {
              // Found a multiple attribute access of a variable and an
              // expression which does not involve that variable:
              _ranges->insert(enumCollVar->name, 
                              attr.substr(0, attr.size() - 1), 
                              RangeInfoBound(rhs, true),
                              RangeInfoBound(rhs, true), true);
            }
            enumCollVar = nullptr;
            attr.clear();
          }
        }
        return;
      }

      if (node->type == NODE_TYPE_OPERATOR_BINARY_LT || 
          node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GE) {
      
        bool include = (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
                        node->type == NODE_TYPE_OPERATOR_BINARY_GE);
        
        auto lhs = node->getMember(0);
        auto rhs = node->getMember(1);

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Attribute access on the right:
          // First find out whether there is a multiple attribute access
          // of a variable on the right:
          buildRangeInfo(rhs, enumCollVar, attr);
          if (enumCollVar != nullptr) {
            RangeInfoBound low;
            RangeInfoBound high;
          
            // Constant value on the left, so insert a constant condition:
            if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
                node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
              high.assign(lhs, include);
            } 
            else {
              low.assign(lhs, include);
            }
            _ranges->insert(enumCollVar->name, attr.substr(0, attr.size() - 1), 
                            low, high, false);
          
            enumCollVar = nullptr;
            attr.clear();
          }
        }

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Attribute access on the left:
          // First find out whether there is a multiple attribute access
          // of a variable on the left:
          buildRangeInfo(lhs, enumCollVar, attr);
          if (enumCollVar != nullptr) {
            RangeInfoBound low;
            RangeInfoBound high;
          
            // Constant value on the right, so insert a constant condition:
            if (node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
                node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
              low.assign(rhs, include);
            } 
            else {
              high.assign(rhs, include);
            }
            _ranges->insert(enumCollVar->name, attr.substr(0, attr.size() - 1), 
                            low, high, false);

            enumCollVar = nullptr;
            attr.clear();
          }
        }

        return;
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
      // default case
      attr.clear();
      enumCollVar = nullptr;
    }

    bool enterSubquery (ExecutionNode* super, ExecutionNode* sub) final {
      return false;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief useIndexRange, try to use an index for filtering
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::useIndexRangeRule (Optimizer* opt, 
                                      ExecutionPlan* plan, 
                                      Optimizer::Rule const* rule) {
  
  // The following maps ids of EnumerateCollectionNodes in the original
  // plan to an index in the (outer vector) of the _changes container.
  std::unordered_map<size_t, size_t> changesPlaces;
  // The outer vector is for the different ids of EnumerateCollectionNodes
  // in the original plan that could be replaced. For each one, the pair
  // contains the id of the node in the original plan and a vector 
  // that holds the possible replacements.
  std::vector<std::pair<size_t, std::vector<ExecutionNode*>>> changes;

  auto cleanupChanges = [&] () -> void {
    for (auto& v : changes) {
      for (ExecutionNode* n : v.second) {
        delete n;
      }
    }
    changes.clear();
    changesPlaces.clear();
  };

  // These are all the FILTER nodes where we start:
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::FILTER, true);

  bool modified = false;
  // In the following loop we only collect changes, maybe we introduce some
  // NoResultsNode, possibly in subqueries.
  try {
    for (auto n : nodes) {
      auto nn = static_cast<FilterNode*>(n);
      auto invars = nn->getVariablesUsedHere();
      TRI_ASSERT(invars.size() == 1);
      FilterToEnumCollFinder finder(plan, invars[0], changesPlaces, changes);
      nn->walk(&finder);
      modified |= finder.modified();
    }
  }
  catch (...) {
    cleanupChanges();
    throw;
  }

  // First find out how many possibilities for plan changes we actually have:
  size_t nrPlans = opt->numberOfPlans();
  size_t possibilities = 1;
  size_t i = 0;
  while (i < changes.size()) {
    possibilities *= changes[i].second.size();
    i++;
    if (possibilities + nrPlans > 30) {
      break;
    }
  }

  // We will apply the first possible change for changes[i..changes.size()-1]
  // and all possible changes for changes[0..i-1] and create all these plans.
  // First make all the changes from i on in the original plan and those
  // for which there is only one possibility:
  try {
    for (size_t j = 0; j < changes.size(); j++) {
      std::vector<ExecutionNode*>& v = changes[j].second;
      if (j >= i || v.size() == 1) {
        size_t choice = 0;
        if (v.size() > 1) {
          // If in doubt, take a skiplist index:
          for (size_t k = 0; k < v.size(); k++) {
            auto n = static_cast<IndexRangeNode*>(v[k]);
            if (n->getIndex()->type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
              choice = k;
              break;
            }
          }
        }
        size_t id = changes[j].first;
        // Just in case:
        if (! v.empty()) {
          plan->registerNode(v[choice]);
          plan->replaceNode(plan->getNodeById(id), v[choice]);
          modified = true;
          // Free the other nodes, if they are there:
          for (size_t k = 0; k < v.size(); k++) {
            if (k != choice) {
              delete v[k];
            }
          }
          v.clear();   // take the new node away from changes such that 
                       // cleanupChanges does not touch it
        }
      }
    }
  }
  catch (...) {
    cleanupChanges();
    throw;
  }

  // Now see whether it is actually only one plan we make:
  if (possibilities == 1) {
    try {
      opt->addPlan(plan, rule->level, modified);
      cleanupChanges();
    }
    catch (...) {
      cleanupChanges();
    }
    return TRI_ERROR_NO_ERROR;
  }

  // Now we have to create more than one plan, we have to use those from
  // changes[0..i-1] which have more than one possibility. Note that those
  // with exactly 1 possibility have already been done above. This amounts
  // to doing a cartesian product, which we do recursively. The result will
  // be in the todo variable:

  std::function <void(size_t, size_t, std::vector<size_t>&)> doworkrecursive;
  std::vector<std::vector<size_t>> todo;
  std::vector<size_t> work;


  doworkrecursive = [&doworkrecursive, &changes, &todo] 
                    (size_t index, size_t limit, std::vector<size_t>& v) {
    if (index >= limit) {
      todo.push_back(v);  // intentionally copy vector
    }
    else if (changes[index].second.size() < 2) {
      doworkrecursive(index+1, limit, v);
    }
    else {
      for (size_t l = 0; l < changes[index].second.size(); l++) {
        v[index] = l;
        doworkrecursive(index+1, limit, v);
      }
    }
  };
 
  // if we get here, we can choose between multiple plans... 
  TRI_ASSERT(possibilities != 1);

  try {
    work.reserve(i);
    for (size_t l = 0; l < i; l++) {
      work.push_back(0);
    }

    doworkrecursive(0, i, work);
  }
  catch (...) {
    cleanupChanges();
    throw;
  }

  // Now we only have to go through todo and do what needs doing:
  try {
    for (auto const& v : todo) {
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());
      for (size_t l = 0; l < i; l++) {
        if (changes[l].second.size() >= 2) {
          ExecutionNode* newNode 
              = changes[l].second[v[l]]->clone(newPlan.get(), true, false);
          newPlan->registerNode(newNode);
          newPlan->replaceNode(newPlan->getNodeById(changes[l].first), newNode);
        }
      }
      opt->addPlan(newPlan.release(), rule->level, true);
    }
  }
  catch (...) {
    cleanupChanges();
    throw;
  }

  cleanupChanges();
  // finally delete the original plan. all plans created in this rule will be better(tm)
  delete plan;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyse the sortnode and its calculation nodes
////////////////////////////////////////////////////////////////////////////////

class SortAnalysis {
  using ECN = triagens::aql::EnumerateCollectionNode;

  typedef std::pair<ECN::IndexMatchVec, IndexOrCondition> Range_IndexPair;

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

  SortAnalysis (SortNode* node)
    : sortNodeID(node->id()) {
    auto sortParams = node->getCalcNodePairs();

    for (size_t n = 0; n < sortParams.size(); n++) {
      auto d = new sortNodeData;
      try {
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
      catch (...) {
        delete d;
        throw;
      }
    }
  }

  ~SortAnalysis () {
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
    for (size_t j = 0; j < _sortNodeData.size(); j ++) {
      if (_sortNodeData[j]->variableName.length() == 0) {
        return false;
      }
    }

    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether our calculation nodes reference variableName; 
/// @returns pair used for further processing with the indices.
////////////////////////////////////////////////////////////////////////////////

  Range_IndexPair getAttrsForVariableName (std::string &variableName) {
    ECN::IndexMatchVec v;
    IndexOrCondition rangeInfo;

    for (size_t j = 0; j < _sortNodeData.size(); j ++) {
      if (_sortNodeData[j]->variableName != variableName) {
        return std::make_pair(v, rangeInfo); // for now, no mixed support.
      }
    }
    // Collect the right data for the sorting:
    for (size_t j = 0; j < _sortNodeData.size(); j ++) {
      v.push_back(std::make_pair(_sortNodeData[j]->attributevec,
                                 _sortNodeData[j]->ASC));
    }
    // We only need one or-condition (because this is mandatory) which
    // refers to 0 of the attributes:
    rangeInfo.push_back(std::vector<RangeInfo>());
    return std::make_pair(v, rangeInfo);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the sortNode and its referenced Calculationnodes from
/// the plan.
////////////////////////////////////////////////////////////////////////////////

  void removeSortNodeFromPlan (ExecutionPlan* newPlan) {
    newPlan->unlinkNode(newPlan->getNodeById(sortNodeID));
  }
};

class SortToIndexNode : public WalkerWorker<ExecutionNode> {
  using ECN = triagens::aql::EnumerateCollectionNode;

  ExecutionPlan*       _plan;
  SortAnalysis*        _sortNode;
  Optimizer::RuleLevel _level;
  bool                 _modified;


  public:

    SortToIndexNode (ExecutionPlan* plan,
                     SortAnalysis* Node,
                     Optimizer::RuleLevel level)
      : _plan(plan),
        _sortNode(Node),
        _level(level),
        _modified(false) {
    }

    bool modified () const {
      return _modified;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief check if an enumerate collection or index range node is part of an
/// outer loop - this is necessary to ensure that the overall query result
/// does not change by replacing a SortNode with an IndexRangeNode
/// Example:
/// FOR i IN [ 1, 2 ] FOR j IN collectionWithIndex SORT j.indexdedAttr RETURN j
/// this must not be optimized because removing the sort and using the index
/// would only guarantee the sortedness within each iteration of the outer for
/// loop but not for the total result
////////////////////////////////////////////////////////////////////////////////

    bool isInnerLoop (ExecutionNode const* node) const {
      while (node != nullptr) {
        auto deps = node->getDependencies();
        if (deps.size() != 1) {
          return false;
        }
        node = deps[0];
        TRI_ASSERT(node != nullptr);

        if (node->getType() == EN::ENUMERATE_COLLECTION ||
            node->getType() == EN::INDEX_RANGE ||
            node->getType() == EN::ENUMERATE_LIST) {
          // we are contained in an outer loop
          return true;

          // future potential optimization: check if the outer loop has 0 or 1 
          // iterations. in this case it is still possible to remove the sort
        }
      }

      return false;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief if the sort is already done by an indexrange, remove the sort.
////////////////////////////////////////////////////////////////////////////////

    bool handleIndexRangeNode (IndexRangeNode* node) {
      if (isInnerLoop(node)) {
        // index range contained in an outer loop. must not optimize away the sort!
        return true;
      }

      auto variableName = node->getVariablesSetHere()[0]->name;
      auto result = _sortNode->getAttrsForVariableName(variableName);

      auto const& match = node->MatchesIndex(result.first);
      if (match.doesMatch) {
        if (match.reverse) {
          node->reverse(true); 
        } 
        _sortNode->removeSortNodeFromPlan(_plan);
        _modified = true;
      }
      return true;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we can sort via an index.
////////////////////////////////////////////////////////////////////////////////

    bool handleEnumerateCollectionNode (EnumerateCollectionNode* node, 
                                        Optimizer::RuleLevel level) {
      if (isInnerLoop(node)) {
        // index range contained in an outer loop. must not optimize away the sort!
        return true;
      }

      auto variableName = node->getVariablesSetHere()[0]->name;
      auto result = _sortNode->getAttrsForVariableName(variableName);

      if (result.first.size() == 0) {
        return true; // we didn't find anything replaceable by indice
      }

      for (auto idx : node->getIndicesOrdered(result.first)) {
        // make one new plan for each index that replaces this
        // EnumerateCollectionNode with an IndexRangeNode

        // can only use the index if it is a skip list or (a hash and we
        // are checking equality)

        ExecutionNode* newNode = new IndexRangeNode(_plan,
                                                    _plan->nextId(),
                                                    node->vocbase(), 
                                                    node->collection(),
                                                    node->outVariable(),
                                                    idx.index,
                                                    result.second,
                                                    (idx.doesMatch && idx.reverse));
        _plan->registerNode(newNode);
        _plan->replaceNode(node, newNode);

        if (idx.doesMatch) { // if the index superseedes the sort, remove it.
          _sortNode->removeSortNodeFromPlan(_plan);
        }
        
        _modified = true;
      }
      
      return true;
    }

    bool enterSubquery (ExecutionNode*, ExecutionNode*) override final { 
      return false; 
    }

    bool before (ExecutionNode* en) override final {
      switch (en->getType()) {
      case EN::ENUMERATE_LIST:
      case EN::CALCULATION:
      case EN::SUBQUERY:
      case EN::FILTER:
        return false;                           // skip. we don't care.

      case EN::SINGLETON:
      case EN::AGGREGATE:
      case EN::INSERT:
      case EN::REMOVE:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
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

int triagens::aql::useIndexForSortRule (Optimizer* opt, 
                                        ExecutionPlan* plan,
                                        Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::SORT, true);
  for (auto n : nodes) {
    auto thisSortNode = static_cast<SortNode*>(n);
    SortAnalysis node(thisSortNode);
    if (node.isAnalyzeable() && ! n->getDependencies().empty()) {
      SortToIndexNode finder(plan, &node, rule->level);
      thisSortNode->getDependencies()[0]->walk(&finder);
      if (finder.modified()) {
        modified = true;
      }
    }
  }
    
  if (modified) {
    plan->findVarUsage();
  }
        
  opt->addPlan(plan,
               modified ? Optimizer::RuleLevel::pass5 : rule->level,
               modified);

  return TRI_ERROR_NO_ERROR;
}

// TODO: finish rule and test it
struct FilterCondition {
  std::string variableName;
  std::string attributeName;
  AstNode const* lowNode  = nullptr;
  AstNode const* highNode = nullptr;
  bool lowInclusive       = false;
  bool highInclusive      = false;
  
  FilterCondition () {
  }

  bool isFullyCoveredBy (RangeInfo const& other) {
    if (! other.isConstant()) {
      return false;
    }

    if (other._var != variableName ||
        other._attr != attributeName) {
      return false;
    }

    bool const lowDefined = (lowNode != nullptr);
    bool const highDefined = (highNode != nullptr);

    if (lowDefined != other._lowConst.isDefined()) {
      return false;
    }

    if (highDefined != other._highConst.isDefined()) {
      return false;
    }

    if (lowDefined) {
      if (other._lowConst.inclusive() != lowInclusive) {
        return false;
      }

      Json json(TRI_UNKNOWN_MEM_ZONE, lowNode->toJsonValue(TRI_UNKNOWN_MEM_ZONE));

      if (TRI_CompareValuesJson(other._lowConst.bound().json(), json.json()) != 0) {
        return false;
      } 
    }

    if (highDefined) {
      if (other._highConst.inclusive() != highInclusive) {
        return false;
      }

      Json json(TRI_UNKNOWN_MEM_ZONE, highNode->toJsonValue(TRI_UNKNOWN_MEM_ZONE));

      if (TRI_CompareValuesJson(other._highConst.bound().json(), json.json()) != 0) {
        return false;
      } 
    }

    return true;
  }

  bool analyze (AstNode const* node) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GE) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      AstNodeType op = node->type;
      bool found = false;

      if (lhs->isConstant() && 
         rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        found = true;
      }
      else if (rhs->isConstant() &&
               lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        found = true;
        // reverse the nodes
        lhs = node->getMember(1);
        rhs = node->getMember(0);

        op = Ast::ReverseOperator(node->type);
      }

      if (found) {
        TRI_ASSERT(lhs->type == NODE_TYPE_VALUE);
        TRI_ASSERT(rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS);

        std::function<void(AstNode const*, std::string&, std::string&)> buildName = 
          [&] (AstNode const* node, std::string& variableName, std::string& attributeName) -> void {
          if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            buildName(node->getMember(0), variableName, attributeName);

            if (! attributeName.empty()) {
              attributeName.push_back('.');
            } 

            attributeName.append(node->getStringValue()); 
          }
          else if (node->type == NODE_TYPE_REFERENCE) { 
            auto variable = static_cast<Variable const*>(node->getData());
            variableName = variable->name;
          }
        }; 

        if (attributeName.empty()) {

          buildName(rhs, variableName, attributeName);
          if (op == NODE_TYPE_OPERATOR_BINARY_EQ ||
              op == NODE_TYPE_OPERATOR_BINARY_NE) {
            lowInclusive  = true;
            lowNode       = lhs;
            highInclusive = true;
            highNode      = lhs;
          }
          else if (op == NODE_TYPE_OPERATOR_BINARY_LT) {
            lowInclusive  = false;
            lowNode       = lhs;
          }
          else if (op == NODE_TYPE_OPERATOR_BINARY_LE) {
            lowInclusive  = true;
            lowNode       = lhs;
          } 
          else if (op == NODE_TYPE_OPERATOR_BINARY_GT) {
            highInclusive = false;
            highNode      = lhs;
          }
          else if (op == NODE_TYPE_OPERATOR_BINARY_GE) {
            highInclusive = true;
            highNode      = lhs;
          }

          return true;
        }
        else {
          // already have collected something, now check if the next condition
          // is for the same variable / attribute
          std::string compareVariableName;
          std::string compareAttributeName;
          buildName(rhs, compareVariableName, compareAttributeName);

          if (variableName == compareVariableName && 
              attributeName == compareAttributeName) {
            // same attribute
            // TODO
          }
        }

        // fall-through 
      }

      return false;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      return (analyze(lhs) && analyze(rhs));
    } 

    return false;
  }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief try to remove filters which are covered by indexes
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeFiltersCoveredByIndexRule (Optimizer* opt,
                                                    ExecutionPlan* plan,
                                                    Optimizer::Rule const* rule) {
  std::unordered_set<ExecutionNode*> toUnlink;
  std::vector<ExecutionNode*>&& nodes= plan->findNodesOfType(EN::FILTER, true); 
  
  for (auto n : nodes) {
    auto fn = static_cast<FilterNode*>(n);
    // find the node with the filter expression
    auto inVar = fn->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);
    // auto outVar = cn->getVariablesSetHere();
          
    auto setter = plan->getVarSetBy(inVar[0]->id);
    if (setter == nullptr) {
      continue;
    }

    if (setter->getType() != EN::CALCULATION) {
      continue;
    }
   
    // check the filter condition 
    FilterCondition condition;
    if (! condition.analyze(static_cast<CalculationNode const*>(setter)->expression()->node())) {
      continue;
    }

    bool handled = false;
    auto current = n;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX_RANGE) {
        // found an index range, now check if the expression is covered by the index
        auto const& ranges = static_cast<IndexRangeNode const*>(current)->ranges();

        // TODO: this is not prepared for OR conditions
        for (auto it : ranges) {
          for (auto it2 : it) {
            if (condition.isFullyCoveredBy(it2)) {
              toUnlink.insert(setter);
              toUnlink.insert(n);
              break;
            }
          } 

          if (handled) {
            break;
          }
        }
      }

      if (handled) {
        break;
      }

      auto deps = current->getDependencies();
      if (deps.size() != 1) {
        break;
      }

      current = deps[0];
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
    if (i == starts.size() - 1) {
      to = data.end();
    }
    else {
      to = begin + starts[i + 1];
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

int triagens::aql::interchangeAdjacentEnumerationsRule (Optimizer* opt,
                                                        ExecutionPlan* plan,
                                                        Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes
   = plan->findNodesOfType(EN::ENUMERATE_COLLECTION, 
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
            EN::ENUMERATE_COLLECTION) {
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
    bool ok = true;

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
          // have enough plans. stop permutations
          ok = false;
          break;
        }
      }
      catch (...) {
        delete newPlan;
        throw;
      }

    } 
    while (ok && nextPermutationTuple(permTuple, starts));
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scatter operations in cluster
/// this rule inserts scatter, gather and remote nodes so operations on sharded
/// collections actually work
/// it will change plans in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::scatterInClusterRule (Optimizer* opt,
                                         ExecutionPlan* plan,
                                         Optimizer::Rule const* rule) {
  bool wasModified = false;

  if (ExecutionEngine::isCoordinator()) {
    // we are a coordinator. now look in the plan for nodes of type
    // EnumerateCollectionNode and IndexRangeNode
    std::vector<ExecutionNode::NodeType> const types = { 
      ExecutionNode::ENUMERATE_COLLECTION, 
      ExecutionNode::INDEX_RANGE,
      ExecutionNode::INSERT,
      ExecutionNode::UPDATE,
      ExecutionNode::REPLACE,
      ExecutionNode::REMOVE
    }; 

    std::vector<ExecutionNode*> nodes = plan->findNodesOfType(types, true);

    for (auto& node: nodes) {
      // found a node we need to replace in the plan

      auto parents = node->getParents();
      auto deps = node->getDependencies();
      TRI_ASSERT(deps.size() == 1);
      bool const isRootNode = plan->isRoot(node);
      // don't do this if we are already distributing!
      if (deps[0]->getType() == ExecutionNode::REMOTE &&
          deps[0]->getDependencies()[0]->getType() == ExecutionNode::DISTRIBUTE){
        continue;
      }
      plan->unlinkNode(node, isRootNode);

      auto const nodeType = node->getType();

      // extract database and collection from plan node
      TRI_vocbase_t* vocbase = nullptr;
      Collection const* collection = nullptr;

      if (nodeType == ExecutionNode::ENUMERATE_COLLECTION) {
        vocbase = static_cast<EnumerateCollectionNode*>(node)->vocbase();
        collection = static_cast<EnumerateCollectionNode*>(node)->collection();
      }
      else if (nodeType == ExecutionNode::INDEX_RANGE) {
        vocbase = static_cast<IndexRangeNode*>(node)->vocbase();
        collection = static_cast<IndexRangeNode*>(node)->collection();
      }
      else if (nodeType == ExecutionNode::INSERT ||
               nodeType == ExecutionNode::UPDATE ||
               nodeType == ExecutionNode::REPLACE ||
               nodeType == ExecutionNode::REMOVE) {
        vocbase = static_cast<ModificationNode*>(node)->vocbase();
        collection = static_cast<ModificationNode*>(node)->collection();
      }
      else {
        TRI_ASSERT(false);
      }

      // insert a scatter node
      ExecutionNode* scatterNode = new ScatterNode(plan, plan->nextId(),
          vocbase, collection);
      plan->registerNode(scatterNode);
      scatterNode->addDependency(deps[0]);

      // insert a remote node
      ExecutionNode* remoteNode = new RemoteNode(plan, plan->nextId(), vocbase,
          collection, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(scatterNode);
        
      // re-link with the remote node
      node->addDependency(remoteNode);

      // insert another remote node
      remoteNode = new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(node);
      
      // insert a gather node 
      ExecutionNode* gatherNode = new GatherNode(plan, plan->nextId(), vocbase,
          collection);
      plan->registerNode(gatherNode);
      gatherNode->addDependency(remoteNode);

      // and now link the gather node with the rest of the plan
      if (parents.size() == 1) {
        parents[0]->replaceDependency(deps[0], gatherNode);
      }

      if (isRootNode) {
        // if we replaced the root node, set a new root node
        plan->root(gatherNode);
      }
      fprintf(stderr, "BLARG!");
      wasModified = true;
    }
  }
   
  opt->addPlan(plan, rule->level, wasModified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only set to one shard and not all as in scatterInCluster
///
/// it will change plans in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::distributeInClusterRule (Optimizer* opt,
                                            ExecutionPlan* plan,
                                            Optimizer::Rule const* rule) {
  bool wasModified = false;

  if (ExecutionEngine::isCoordinator()) {
    // we are a coordinator, we replace the root if it is a modification node
    
    // only replace if it is the last node in the plan
    auto const& node = plan->root();
    auto const nodeType = node->getType();
    
    if (nodeType != ExecutionNode::INSERT  &&
        nodeType != ExecutionNode::REMOVE) {
      opt->addPlan(plan, rule->level, wasModified);
      return TRI_ERROR_NO_ERROR;
    }
    
    Collection const* collection = static_cast<ModificationNode*>(node)->collection();
    
    if (nodeType == ExecutionNode::REMOVE) {
      // check if collection shard keys are only _key
      std::vector<std::string> shardKeys = collection->shardKeys();
      if (shardKeys.size() != 1 || shardKeys[0] != TRI_VOC_ATTRIBUTE_KEY) {
        opt->addPlan(plan, rule->level, wasModified);
        return TRI_ERROR_NO_ERROR;
      }
    }

    auto deps = node->getDependencies();
    TRI_ASSERT(deps.size() == 1);

    // unlink the node
    plan->unlinkNode(node, true);

    // extract database from plan node
    TRI_vocbase_t* vocbase = static_cast<ModificationNode*>(node)->vocbase();

    // insert a distribute node
    TRI_ASSERT(node->getVariablesUsedHere().size() == 1);
    ExecutionNode* distNode = new DistributeNode(plan, plan->nextId(), 
        vocbase, collection, node->getVariablesUsedHere()[0]->id);
    plan->registerNode(distNode);
    distNode->addDependency(deps[0]);

    // insert a remote node
    ExecutionNode* remoteNode = new RemoteNode(plan, plan->nextId(), vocbase,
        collection, "", "", "");
    plan->registerNode(remoteNode);
    remoteNode->addDependency(distNode);
      
    // re-link with the remote node
    node->addDependency(remoteNode);

    // insert another remote node
    remoteNode = new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
    plan->registerNode(remoteNode);
    remoteNode->addDependency(node);
    
    // insert a gather node 
    ExecutionNode* gatherNode = new GatherNode(plan, plan->nextId(), vocbase,
        collection);
    plan->registerNode(gatherNode);
    gatherNode->addDependency(remoteNode);

    // we replaced the root node, set a new root node
    plan->root(gatherNode);
    wasModified = true;
  }
   
  opt->addPlan(plan, rule->level, wasModified);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move filters up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::distributeFilternCalcToClusterRule (Optimizer* opt, 
                                                       ExecutionPlan* plan,
                                                       Optimizer::Rule const* rule) {
  bool modified = false;

  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::GATHER, true);

  
  for (auto n : nodes) {
    auto remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];
    auto parents = n->getParents();
    if (parents.size() < 1) {
      continue;
    }
    while (1) {
      bool stopSearching = false;

      auto inspectNode = parents[0];

      switch (inspectNode->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::AGGREGATE:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
          parents = inspectNode->getParents();
          continue;
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::ILLEGAL:
          //do break
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::SORT:
        case EN::INDEX_RANGE:
        case EN::ENUMERATE_COLLECTION:
          stopSearching = true;
          break;
        case EN::CALCULATION: {
          auto calc = static_cast<CalculationNode const*>(inspectNode);
          // check if the expression can be executed on a DB server safely
          if (! calc->expression()->canRunOnDBServer()) {
            stopSearching = true;
            break;
          }
          // intentionally fall through here
        }
        case EN::FILTER:
          // remember our cursor...
          parents = inspectNode->getParents();
          // then unlink the filter/calculator from the plan
          plan->unlinkNode(inspectNode);
          // and re-insert into plan in front of the remoteNode
          plan->insertDependency(rn, inspectNode);

          modified = true;
          //ready to rumble!
          break;
      }

      if (stopSearching) {
        break;
      }
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule->level, modified);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move sorts up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// sorts are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// 
/// filters are not pushed beyond limits
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::distributeSortToClusterRule (Optimizer* opt, 
                                                ExecutionPlan* plan,
                                                Optimizer::Rule const* rule) {
  bool modified = false;

  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::GATHER, true);
  
  for (auto n : nodes) {
    auto remoteNodeList = n->getDependencies();
    auto gatherNode = static_cast<GatherNode*>(n);
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];
    auto parents = n->getParents();
    if (parents.size() < 1) {
      continue;
    }
    while (1) {
      bool stopSearching = false;

      auto inspectNode = parents[0];

      switch (inspectNode->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::AGGREGATE:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::CALCULATION:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::ILLEGAL:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX_RANGE:
        case EN::ENUMERATE_COLLECTION:
          // For all these, we do not want to pull a SortNode further down
          // out to the DBservers, note that potential FilterNodes and
          // CalculationNodes that can be moved to the DBservers have 
          // already been moved over by the distribute-filtercalc-to-cluster
          // rule which is done first.
          stopSearching = true;
          break;
        case EN::SORT:
          auto thisSortNode = static_cast<SortNode*>(inspectNode);
      
          // remember our cursor...
          parents = inspectNode->getParents();
          // then unlink the filter/calculator from the plan
          plan->unlinkNode(inspectNode);
          // and re-insert into plan in front of the remoteNode
          plan->insertDependency(rn, inspectNode);
          gatherNode->setElements(thisSortNode->getElements());
          modified = true;
          //ready to rumble!
      }

      if (stopSearching) {
        break;
      }
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule->level, modified);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryRemoteScatterRule (Optimizer* opt, 
                                                       ExecutionPlan* plan, 
                                                       Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::REMOTE, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto n : nodes) {
    // check if the remote node is preceeded by a scatter node and any number of
    // calculation and singleton nodes. if yes, remove remote and scatter

    auto const& deps = n->getDependencies();
    if (deps.size() != 1) {
      continue;
    }

    if (deps[0]->getType() != EN::SCATTER) {
      continue;
    }

    bool canOptimize = true;
    auto node = deps[0];
    while (node != nullptr) {
      auto const& d = node->getDependencies();

      if (d.size() != 1) {
        break;
      }

      node = d[0];
      if (node->getType() != EN::SINGLETON && 
          node->getType() != EN::CALCULATION) {
        // found some other node type...
        // this disqualifies the optimization
        canOptimize = false;
        break;
      }

      if (node->getType() == EN::CALCULATION) {
        auto calc = static_cast<CalculationNode const*>(node);
        // check if the expression can be executed on a DB server safely
        if (! calc->expression()->canRunOnDBServer()) {
          canOptimize = false;
          break;
        }
      }
    }

    if (canOptimize) {
      toUnlink.insert(n);
      toUnlink.insert(deps[0]);
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
/// WalkerWorker for undistributeRemoveAfterEnumColl
////////////////////////////////////////////////////////////////////////////////

class RemoveToEnumCollFinder: public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  std::unordered_set<ExecutionNode*>& _toUnlink;
  bool _remove;
  bool _scatter;
  bool _gather;
  EnumerateCollectionNode* _enumColl;
  ExecutionNode* _setter;
  const Variable* _variable;
  ExecutionNode* _lastNode;
  
  public: 
    RemoveToEnumCollFinder (ExecutionPlan* plan,
                            std::unordered_set<ExecutionNode*>& toUnlink)
      : _plan(plan),
        _toUnlink(toUnlink),
        _remove(false),
        _scatter(false),
        _gather(false),
        _enumColl(nullptr),
        _setter(nullptr),
        _variable(nullptr), 
        _lastNode(nullptr){
    };

    ~RemoveToEnumCollFinder () {
    }

    bool before (ExecutionNode* en) override final {
      switch (en->getType()) {
        case EN::REMOVE: {
          TRI_ASSERT(_remove == false);
            
          // find the variable we are removing . . .
          auto rn = static_cast<RemoveNode*>(en);
          auto varsToRemove = rn->getVariablesUsedHere();

          // remove nodes always have one input variable
          TRI_ASSERT(varsToRemove.size() == 1);
          _setter = _plan->getVarSetBy(varsToRemove[0]->id);
          TRI_ASSERT(_setter != nullptr);
          auto enumColl = _setter;

          if (_setter->getType() == EN::CALCULATION) {
            // this should be an attribute access for _key
            auto cn = static_cast<CalculationNode*>(_setter);
            if (! cn->expression()->isAttributeAccess()) {
              break; // abort . . .
            }
            // check the variable is the same as the remove variable
            auto vars = cn->getVariablesSetHere();
            if (vars.size() != 1 || vars[0]->id != varsToRemove[0]->id) {
              break; // abort . . . 
            }
            // check the remove node's collection is sharded over _key
            std::vector<std::string> shardKeys = rn->collection()->shardKeys();
            if (shardKeys.size() != 1 || shardKeys[0] != TRI_VOC_ATTRIBUTE_KEY) {
              break; // abort . . .
            }

            // set the varsToRemove to the variable in the expression of this
            // node and also define enumColl
            varsToRemove = cn->getVariablesUsedHere();
            TRI_ASSERT(varsToRemove.size() == 1);
            enumColl = _plan->getVarSetBy(varsToRemove[0]->id);
            TRI_ASSERT(_setter != nullptr);
          } 
          
          if (enumColl->getType() != EN::ENUMERATE_COLLECTION) {
            break; // abort . . .
          }

          _enumColl = static_cast<EnumerateCollectionNode*>(enumColl);

          if (_enumColl->collection() != rn->collection()) {
            break; // abort . . . 
          }
           
          _variable = varsToRemove[0];    // the variable we'll remove
          _remove = true;
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::REMOTE: {
          _toUnlink.insert(en);
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::DISTRIBUTE:
        case EN::SCATTER: {
          if (_scatter) { // met more than one scatter node
            break;        // abort . . . 
          }
          _scatter = true;
          _toUnlink.insert(en);
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::GATHER: {
          if (_gather) { // met more than one gather node
            break;       // abort . . . 
          }
          _gather = true;
          _toUnlink.insert(en);
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::FILTER: { 
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::CALCULATION: {
          TRI_ASSERT(_setter != nullptr);
          if (_setter->getType() == EN::CALCULATION && _setter->id() == en->id()) {
            _lastNode = en;
            return false; // continue . . .
          }
          if (_lastNode == nullptr || _lastNode->getType() != EN::FILTER) { 
            // doesn't match the last filter node
            break; // abort . . .
          }
          auto cn = static_cast<CalculationNode*>(en);
          auto fn = static_cast<FilterNode*>(_lastNode);
          
          // check these are a Calc-Filter pair
          if (cn->getVariablesSetHere()[0]->id != fn->getVariablesUsedHere()[0]->id) {
            break; // abort . . .
          }

          // check that we are filtering/calculating something with the variable
          // we are to remove
          auto varsUsedHere = cn->getVariablesUsedHere();
         
          if (varsUsedHere.size() != 1) {
            break; //abort . . .
          }
          if (varsUsedHere[0]->id != _variable->id) {
            break;
          }
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::ENUMERATE_COLLECTION: {
          // check that we are enumerating the variable we are to remove
          // and that we have already seen a remove node
          TRI_ASSERT(_enumColl != nullptr);
          if (en->id() != _enumColl->id()) {
            break;
          }
          return true; // reached the end!
        }
        case EN::SINGLETON:
        case EN::ENUMERATE_LIST:
        case EN::SUBQUERY:        
        case EN::AGGREGATE:
        case EN::INSERT:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::ILLEGAL:
        case EN::LIMIT:           
        case EN::SORT:
        case EN::INDEX_RANGE: {
          // if we meet any of the above, then we abort . . .
        }
    }
    _toUnlink.clear();
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief recognises that a RemoveNode can be moved to the shards.
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::undistributeRemoveAfterEnumCollRule (Optimizer* opt, 
                                                        ExecutionPlan* plan, 
                                                        Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::REMOVE, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto n : nodes) {
    RemoveToEnumCollFinder finder(plan, toUnlink);
    n->walk(&finder);
  }

  bool modified = false;
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
    modified = true;
  }
  
  opt->addPlan(plan, rule->level, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief auxilliary struct for finding common nodes in OR conditions
////////////////////////////////////////////////////////////////////////////////

struct CommonNodeFinder {
  std::vector<AstNode const*> possibleNodes;
  
  bool find (AstNode const*  node, 
             AstNodeType     condition,
             AstNode const*& commonNode,
             std::string&    commonName) {
  
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (find(node->getMember(0), condition, commonNode, commonName) 
           && find(node->getMember(1), condition, commonNode, commonName));
    }

    if (node->type == NODE_TYPE_VALUE) {
      possibleNodes.clear();
      return true;
    }
    
    if (node->type == condition 
        || (condition != NODE_TYPE_OPERATOR_BINARY_EQ 
            && ( node->type == NODE_TYPE_OPERATOR_BINARY_LE 
              || node->type == NODE_TYPE_OPERATOR_BINARY_LT
              || node->type == NODE_TYPE_OPERATOR_BINARY_GE 
              || node->type == NODE_TYPE_OPERATOR_BINARY_GT ))) {

      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (lhs->isConstant()) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->isConstant()) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->type == NODE_TYPE_FCALL || 
          rhs->type == NODE_TYPE_FCALL_USER ||
          rhs->type == NODE_TYPE_REFERENCE) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (lhs->type == NODE_TYPE_FCALL || 
          lhs->type == NODE_TYPE_FCALL_USER ||
          lhs->type == NODE_TYPE_REFERENCE) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          lhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (lhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          // don't return, must consider the other side of the condition
        } 
        else {
          possibleNodes.push_back(lhs);
        }
      }
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (rhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          return false;
        } 
        else {
          possibleNodes.push_back(rhs);
          return true;
        }
      }
    }
    possibleNodes.clear();
    return (! commonName.empty());
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auxilliary struct for the OR-to-IN conversion
////////////////////////////////////////////////////////////////////////////////

struct OrToInConverter {

  std::vector<AstNode const*> valueNodes;
  CommonNodeFinder            finder;
  AstNode const*              commonNode = nullptr;
  std::string                 commonName;

  AstNode* buildInExpression (Ast* ast) {
    // the list of comparison values
    auto list = ast->createNodeList();
    for (auto x : valueNodes) {
      list->addMember(x);
    }

    // return a new IN operator node
    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN,
                                         commonNode->clone(ast),
                                         list);
  }

  bool canConvertExpression (AstNode const* node) {
    if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_EQ, commonNode, commonName)){
      return canConvertExpressionWalker(node);
    }
    return false;
  }

  bool canConvertExpressionWalker (AstNode const* node) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (canConvertExpressionWalker(node->getMember(0)) &&
              canConvertExpressionWalker(node->getMember(1)));
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (canConvertExpressionWalker(rhs) && ! canConvertExpressionWalker(lhs)) {
        valueNodes.push_back(lhs);
        return true;
      } 
      
      if (canConvertExpressionWalker(lhs) && ! canConvertExpressionWalker(rhs)) {
        valueNodes.push_back(rhs);
        return true;
      } 
      // if canConvertExpressionWalker(lhs) and canConvertExpressionWalker(rhs), then one of
      // the equalities in the OR statement is of the form x == x
      // fall-through intentional
    }
    else if (node->type == NODE_TYPE_REFERENCE ||
             node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
             node->type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons 
      return (node->toString() == commonName);
    } 

    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief this rule replaces expressions of the type: 
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::replaceOrWithInRule (Optimizer* opt, 
                                        ExecutionPlan* plan, 
                                        Optimizer::Rule const* rule) {
  ENTER_BLOCK;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::FILTER, true);

  bool modified = false;
  for (auto n : nodes) {
    auto deps = n->getDependencies();
    TRI_ASSERT(deps.size() == 1);
    if (deps[0]->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto cn = static_cast<CalculationNode*>(deps[0]);

    auto inVar  = fn->getVariablesUsedHere();
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    OrToInConverter converter;
    if (converter.canConvertExpression(cn->expression()->node())) {
      Expression* expr = nullptr;
      ExecutionNode* newNode = nullptr;
      auto inNode = converter.buildInExpression(plan->getAst());

      try {
        expr = new Expression(plan->getAst(), inNode);
      }
      catch (...) {
        delete inNode;
        throw;
      }

      try {
        newNode = new CalculationNode(plan, plan->nextId(), expr, outVar[0]);
      }
      catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }
 
  if (modified) {
    plan->findVarUsage();
  }
  opt->addPlan(plan, rule->level, modified);

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK;
}

struct RemoveRedundantOr {
  AstNode const*    bestValue = nullptr;
  AstNodeType       comparison;
  bool              inclusive;
  bool              isComparisonSet = false;
  CommonNodeFinder  finder;
  AstNode const*    commonNode = nullptr;
  std::string       commonName;

  AstNode* createReplacementNode (Ast* ast) {
    TRI_ASSERT(commonNode != nullptr);
    TRI_ASSERT(bestValue != nullptr);
    TRI_ASSERT(isComparisonSet == true);
    return ast->createNodeBinaryOperator(comparison, commonNode->clone(ast),
        bestValue);
  }

  bool isInclusiveBound (AstNodeType type) {
    return (type == NODE_TYPE_OPERATOR_BINARY_GE || type == NODE_TYPE_OPERATOR_BINARY_LE);
  }

  int isCompatibleBound (AstNodeType type, AstNode const* value) {
    
    if ((comparison == NODE_TYPE_OPERATOR_BINARY_LE
          || comparison == NODE_TYPE_OPERATOR_BINARY_LT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_LE
         || type == NODE_TYPE_OPERATOR_BINARY_LT)) {
      return -1; //high bound
    } 
    else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE
          || comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_GE 
         || type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1; //low bound
    }
    return 0; //incompatible bounds
  }

  // returns false if the existing value is better and true if the input value is
  // better
  bool compareBounds(AstNodeType type, AstNode const* value, int lowhigh) { 

    int cmp = CompareAstNodes(bestValue, value);

    if (cmp == 0 && (isInclusiveBound(comparison) != isInclusiveBound(type))) {
      return (isInclusiveBound(type) ? true : false);
    }
    return (cmp * lowhigh == 1);
  }

  bool hasRedundantCondition (AstNode const* node) {
    if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_LT, commonNode, commonName)) {
      return hasRedundantConditionWalker(node);
    }
    return false;
  }

  bool hasRedundantConditionWalker (AstNode const* node) {
    AstNodeType type = node->type;

    if (type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (hasRedundantConditionWalker(node->getMember(0)) &&
              hasRedundantConditionWalker(node->getMember(1)));
    }
    
    if (type == NODE_TYPE_OPERATOR_BINARY_LE 
     || type == NODE_TYPE_OPERATOR_BINARY_LT
     || type == NODE_TYPE_OPERATOR_BINARY_GE 
     || type == NODE_TYPE_OPERATOR_BINARY_GT) {

      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      
      if (hasRedundantConditionWalker(rhs) 
          && ! hasRedundantConditionWalker(lhs)
          && lhs->isConstant()) {
        
        if (! isComparisonSet) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(Ast::ReverseOperator(type), lhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, lhs, lowhigh)) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
        }
        return true;
      }
      if (hasRedundantConditionWalker(lhs) 
          && ! hasRedundantConditionWalker(rhs)
          && rhs->isConstant()) {
        if (! isComparisonSet) {
          comparison = type;
          bestValue = rhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(type, rhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, rhs, lowhigh)) {
            comparison = type;
            bestValue = rhs;
        }
        return true;
      }
      // if hasRedundantConditionWalker(lhs) and
      // hasRedundantConditionWalker(rhs), then one of the conditions in the OR
      // statement is of the form x == x fall-through intentional
    }
    else if (type == NODE_TYPE_REFERENCE ||
             type == NODE_TYPE_ATTRIBUTE_ACCESS ||
             type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons 
      return (node->toString() == commonName);
    } 
    else if (node->isBoolValue()) {
      return true;
    }

    return false;
  }
};

int triagens::aql::removeRedundantOrRule (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::Rule const* rule) {
  ENTER_BLOCK;
  std::vector<ExecutionNode*> nodes
    = plan->findNodesOfType(EN::FILTER, true);

  bool modified = false;
  for (auto n : nodes) {
    auto deps = n->getDependencies();
    TRI_ASSERT(deps.size() == 1);
    if (deps[0]->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto cn = static_cast<CalculationNode*>(deps[0]);

    auto inVar  = fn->getVariablesUsedHere();
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    RemoveRedundantOr remover;
    if (remover.hasRedundantCondition(cn->expression()->node())) {
      Expression* expr = nullptr;
      ExecutionNode* newNode = nullptr;
      auto astNode = remover.createReplacementNode(plan->getAst());

      expr = new Expression(plan->getAst(), astNode);

      try {
        newNode = new CalculationNode(plan, plan->nextId(), expr, outVar[0]);
      }
      catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }
 
  if (modified) {
    plan->findVarUsage();
  }
  opt->addPlan(plan, rule->level, modified);

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

