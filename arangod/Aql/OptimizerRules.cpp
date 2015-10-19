////////////////////////////////////////////////////////////////////////////////
/// @brief rules for the query optimizer
///
/// @file 
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
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"
#include "Aql/AggregateNode.h"
#include "Aql/AggregationOptions.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Function.h"
#include "Aql/Index.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/SortNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/json-utilities.h"

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

int triagens::aql::removeRedundantSortsRule (Optimizer* opt, 
                                             ExecutionPlan* plan,
                                             Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::SORT, true);

  if (nodes.empty()) {
    // quick exit
    opt->addPlan(plan, rule, false);
    return TRI_ERROR_NO_ERROR;
  }

  std::unordered_set<ExecutionNode*> toUnlink;

  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
 
  for (auto const& n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = static_cast<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation(plan, &buffer);

    if (sortInfo.isValid && ! sortInfo.criteria.empty()) {
      // we found a sort that we can understand
      std::vector<ExecutionNode*> stack;

      sortNode->addDependencies(stack);

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
                toUnlink.emplace(current);
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.emplace(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              // the sort at the start of the pipeline makes the sort at the end
              // superfluous, so we'll remove it
              toUnlink.emplace(n);
              break;
            }
            
            case SortInformation::allEqual: {
              // the sort at the end of the pipeline makes the sort at the start
              // superfluous, so we'll remove it
              toUnlink.emplace(current);
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
          // this includes COLLECT and LIMIT
          break;
        }
                
        if (! current->hasDependency()) {
          // node either has no or more than one dependency. we don't know what to do and must abort
          // note: this will also handle Singleton nodes
          break;
        }
        
        current->addDependencies(stack); 
      }

      if (toUnlink.find(n) == toUnlink.end() &&
          sortNode->simplify(plan)) {
        // sort node had only constant expressions. it will make no difference if we execute it or not
        // so we can remove it
        toUnlink.emplace(n);
      }
    }
  }

  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, ! toUnlink.empty());

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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true);
  
  for (auto const& n : nodes) {
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
      toUnlink.emplace(n);
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
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

#if 0
struct CollectVariableFinder {
  Variable const* searchVariable;
  std::unordered_set<std::string>& attributeNames;
  std::vector<AstNode const*> stack;
  bool canUseOptimization;
  bool isArgumentToLength;

  CollectVariableFinder (AggregateNode const* collectNode,
                         std::unordered_set<std::string>& attributeNames)
    : searchVariable(collectNode->outVariable()),
      attributeNames(attributeNames),
      stack(),
      canUseOptimization(true),
      isArgumentToLength(false) {

    TRI_ASSERT(searchVariable != nullptr);
    stack.reserve(4);
  }

  void analyze (AstNode const* node) {
    TRI_ASSERT(node != nullptr);

    if (! canUseOptimization) {
      // we already know we cannot apply this optimization
      return;
    }

    stack.push_back(node);

    size_t const n = node->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto sub = node->getMember(i);
      if (sub != nullptr) {
        // recurse into subnodes
        analyze(sub);
      }
    }

    if (node->type == NODE_TYPE_REFERENCE) {
      auto variable = static_cast<Variable const*>(node->getData());

      TRI_ASSERT(variable != nullptr);

      if (variable->id == searchVariable->id) {
        bool handled = false;
        auto const size = stack.size();

        if (size >= 3 &&
            stack[size - 3]->type == NODE_TYPE_EXPANSION) {
          // our variable is used in an expansion, e.g. g[*].attribute
          auto expandNode = stack[size - 3];
          TRI_ASSERT(expandNode->numMembers() == 2);
          TRI_ASSERT(expandNode->getMember(0)->type == NODE_TYPE_ITERATOR);

          auto expansion = expandNode->getMember(1);
          TRI_ASSERT(expansion != nullptr);
          while (expansion->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            // note which attribute is used with our variable
            if (expansion->getMember(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
              expansion = expansion->getMember(0);
            }
            else {
              attributeNames.emplace(expansion->getStringValue());
              handled = true;
              break;
            }
          }
        } 
        else if (size >= 3 &&
                 stack[size - 2]->type == NODE_TYPE_ARRAY &&
                 stack[size - 3]->type == NODE_TYPE_FCALL) {
          auto func = static_cast<Function const*>(stack[size - 3]->getData());

          if (func->externalName == "LENGTH" &&
              stack[size - 2]->numMembers() == 1) {
            // call to function LENGTH() with our variable as its single argument
            handled = true;
            isArgumentToLength = true;
          }
        }
      
        if (! handled) {
          canUseOptimization = false;
        }
      }
    }

    stack.pop_back();
  }

};
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the variables used in a COLLECT INTO
////////////////////////////////////////////////////////////////////////////////

#if 0    
int triagens::aql::specializeCollectVariables (Optimizer* opt, 
                                               ExecutionPlan* plan, 
                                               Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*> nodes = plan->findNodesOfType(EN::AGGREGATE, true);
  
  for (auto n : nodes) {
    auto collectNode = static_cast<AggregateNode*>(n);
    TRI_ASSERT(collectNode != nullptr);
          
    auto const&& deps = collectNode->getDependencies();
    if (deps.size() != 1) {
      continue;
    }

    if (! collectNode->hasOutVariable() ||
        collectNode->hasExpressionVariable() ||
        collectNode->count()) {
      // COLLECT without INTO or a COLLECT that already uses an 
      // expression variable or a COLLECT that only counts
      continue;
    }

    auto outVariable = collectNode->outVariable();
    // must have an outVariable if we got here
    TRI_ASSERT(outVariable != nullptr);
       
    std::unordered_set<std::string> attributeNames; 
    CollectVariableFinder finder(collectNode, attributeNames);

    // check all following nodes for usage of the out variable
    std::vector<ExecutionNode*> parents(n->getParents());
    
    while (! parents.empty() &&
           finder.canUseOptimization) {
      auto current = parents.back();
      parents.pop_back();

      for (auto it : current->getParents()) {
        parents.emplace_back(it);
      }

      // now check current node for usage of out variable 
      auto const&& variablesUsed = current->getVariablesUsedHere();

      bool found = false;
      for (auto it : variablesUsed) {
        if (it == outVariable) {
          found = true;
          break;
        }
      }

      if (found) {
        // variable is used. now find out how it is used
        if (current->getType() != EN::CALCULATION) {
          // variable is used outside of a calculation... skip optimization
          // TODO
          break;
        }

        auto calculationNode = static_cast<CalculationNode*>(current);
        auto expression = calculationNode->expression(); 
        TRI_ASSERT(expression != nullptr);

        finder.analyze(expression->node());
      }
    }

    if (finder.canUseOptimization) {
      // can use the optimization

      if (! finder.attributeNames.empty()) {
        auto obj = plan->getAst()->createNodeObject();

        for (auto const& attributeName : finder.attributeNames) {
          for (auto it : collectNode->getVariablesUsedHere()) {
            if (it->name == attributeName) {
              auto refNode = plan->getAst()->createNodeReference(it);
              auto element = plan->getAst()->createNodeObjectElement(it->name.c_str(), refNode);
              obj->addMember(element);
            }
          }
        }

        if (obj->numMembers() == attributeNames.size()) {
          collectNode->removeDependency(deps[0]);
          auto calculationNode = plan->createTemporaryCalculation(obj);
          calculationNode->addDependency(deps[0]);
          collectNode->addDependency(calculationNode);
 
          collectNode->setExpressionVariable(calculationNode->outVariable());
          modified = true;
        }
      }
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }
 
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief remove INTO of a COLLECT if not used
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeCollectIntoRule (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::AGGREGATE, true);
  
  for (auto const& n : nodes) {
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
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                  helper class for propagateConstantAttributesRule
// -----------------------------------------------------------------------------

class PropagateConstantAttributesHelper {

  public:
 
    PropagateConstantAttributesHelper () 
      : _constants(),
        _modified(false) {
    }

    bool modified () const {
      return _modified;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief inspects a plan and propages constant values in expressions
////////////////////////////////////////////////////////////////////////////////

    void propagateConstants (ExecutionPlan* plan) {
      std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true); 
      
      for (auto const& node : nodes) {
        auto fn = static_cast<FilterNode*>(node);

        auto inVar = fn->getVariablesUsedHere();
        TRI_ASSERT(inVar.size() == 1);
                
        auto setter = plan->getVarSetBy(inVar[0]->id);
        if (setter != nullptr &&
            setter->getType() == EN::CALCULATION) {
          auto cn = static_cast<CalculationNode*>(setter);
          auto expression = cn->expression();

          if (expression != nullptr) {
            collectConstantAttributes(const_cast<AstNode*>(expression->node()));
          }
        }
      }

      if (! _constants.empty()) {
        for (auto const& node : nodes) {
          auto fn = static_cast<FilterNode*>(node);

          auto inVar = fn->getVariablesUsedHere();
          TRI_ASSERT(inVar.size() == 1);
                  
          auto setter = plan->getVarSetBy(inVar[0]->id);
          if (setter != nullptr &&
              setter->getType() == EN::CALCULATION) {
            auto cn = static_cast<CalculationNode*>(setter);
            auto expression = cn->expression();

            if (expression != nullptr) {
              insertConstantAttributes(const_cast<AstNode*>(expression->node()));
            }
          }
        }
      }
    }

  private:

    AstNode const* getConstant (Variable const* variable,
                                std::string const& attribute) const {
      auto it = _constants.find(variable);

      if (it == _constants.end()) {
        return nullptr;
      }

      auto it2 = (*it).second.find(attribute);

      if (it2 == (*it).second.end()) {
        return nullptr;
      }

      return (*it2).second;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief inspects an expression (recursively) and notes constant attribute
/// values so they can be propagated later
////////////////////////////////////////////////////////////////////////////////

    void collectConstantAttributes (AstNode* node) {
      if (node == nullptr) {
        return;
      }

      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        auto lhs = node->getMember(0); 
        auto rhs = node->getMember(1); 

        collectConstantAttributes(lhs);
        collectConstantAttributes(rhs);
      }
      else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        auto lhs = node->getMember(0); 
        auto rhs = node->getMember(1); 
        
        if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          inspectConstantAttribute(rhs, lhs);
        }
        else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          inspectConstantAttribute(lhs, rhs);
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief traverses an AST part recursively and patches it by inserting
/// constant values
////////////////////////////////////////////////////////////////////////////////

    void insertConstantAttributes (AstNode* node) {
      if (node == nullptr) {
        return;
      }

      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        auto lhs = node->getMember(0); 
        auto rhs = node->getMember(1); 

        insertConstantAttributes(lhs);
        insertConstantAttributes(rhs);
      }
      else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        auto lhs = node->getMember(0); 
        auto rhs = node->getMember(1); 

        if (! lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          insertConstantAttribute(node, 1);
        }
        if (! rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          insertConstantAttribute(node, 0);
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an attribute and its variable from an attribute access
/// (e.g. `a.b.c` will return variable `a` and attribute name `b.c.`.
////////////////////////////////////////////////////////////////////////////////

    bool getAttribute (AstNode const* attribute,
                       Variable const*& variable,
                       std::string& name) {
      TRI_ASSERT(attribute != nullptr && 
                 attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS);
      TRI_ASSERT(name.empty());

      while (attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        name = std::string(".") + std::string(attribute->getStringValue(), attribute->getStringLength()) + name;
        attribute = attribute->getMember(0);
      }

      if (attribute->type != NODE_TYPE_REFERENCE) {
        return false;
      }

      variable = static_cast<Variable const*>(attribute->getData());
      TRI_ASSERT(variable != nullptr);

      return true;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief inspect the constant value assigned to an attribute
/// the attribute value will be stored so it can be inserted for the attribute
/// later
////////////////////////////////////////////////////////////////////////////////

    void inspectConstantAttribute (AstNode const* attribute,
                                   AstNode const* value) {
      Variable const* variable = nullptr;
      std::string name;

      if (! getAttribute(attribute, variable, name)) {
        return;
      }

      auto it = _constants.find(variable);

      if (it == _constants.end()) {
        _constants.emplace(std::make_pair(variable, std::unordered_map<std::string, AstNode const*>{ { name, value } }));
        return;
      }
        
      auto it2 = (*it).second.find(name);

      if (it2 == (*it).second.end()) {
        // first value for the attribute
        (*it).second.emplace(std::make_pair(name, value));
      }
      else {
        auto previous = (*it2).second;

        if (previous == nullptr) {
          // we have multiple different values for the attribute. better not use this attribute
          return;
        }

        if (TRI_CompareValuesJson(value->computeJson(), previous->computeJson(), true) != 0) {
          // different value found for an already tracked attribute. better not use this attribute 
          (*it2).second = nullptr;
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief patches an AstNode by inserting a constant value into it
////////////////////////////////////////////////////////////////////////////////

    void insertConstantAttribute (AstNode* parentNode,
                                  size_t accessIndex) {
      Variable const* variable = nullptr;
      std::string name;

      if (! getAttribute(parentNode->getMember(accessIndex), variable, name)) {
        return;
      }

      auto constantValue = getConstant(variable, name);

      if (constantValue != nullptr) {
        parentNode->changeMember(accessIndex, const_cast<AstNode*>(constantValue)); 
        _modified = true; 
      }
    }

    std::unordered_map<Variable const*, std::unordered_map<std::string, AstNode const*>> _constants;
  
    bool _modified;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief propagate constant attributes in FILTERs
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::propagateConstantAttributesRule (Optimizer* opt, 
                                                    ExecutionPlan* plan, 
                                                    Optimizer::Rule const* rule) {
  PropagateConstantAttributesHelper helper;
  helper.propagateConstants(plan);

  bool const modified = helper.modified();

  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove SORT RAND() if appropriate
////////////////////////////////////////////////////////////////////////////////
    
int triagens::aql::removeSortRandRule (Optimizer* opt, 
                                       ExecutionPlan* plan, 
                                       Optimizer::Rule const* rule) {
  bool modified = false;
  // should we enter subqueries??
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::SORT, true);
  
  for (auto const& n : nodes) {
    auto node = static_cast<SortNode*>(n);
    auto const& elements = node->getElements();
    if (elements.size() != 1) {
      // we're looking for "SORT RAND()", which has just one sort criterion
      continue;
    }

    auto const variable = elements[0].first;
    TRI_ASSERT(variable != nullptr);

    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || 
        setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = static_cast<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression == nullptr ||
        expression->node() == nullptr ||
        expression->node()->type != NODE_TYPE_FCALL) {
      // not the right type of node
      continue;
    }

    auto funcNode = expression->node();
    auto func = static_cast<Function const*>(funcNode->getData());

    // we're looking for "RAND()", which is a function call
    // with an empty parameters array
    if (func->externalName != "RAND" ||
        funcNode->numMembers() != 1 ||
        funcNode->getMember(0)->numMembers() != 0) { 
      continue;
    }
 
    // now we're sure we got SORT RAND() ! 

    // we found what we were looking for!
    // now check if the dependencies qualify
    if (! n->hasDependency()) {
      break;
    }

    auto current = n->getFirstDependency();
    ExecutionNode* collectionNode = nullptr;

    while (current != nullptr) {
      if (current->canThrow()) {
        // we shouldn't bypass a node that can throw
        collectionNode = nullptr;
        break;
      }

      switch (current->getType()) {
        case EN::SORT: 
        case EN::AGGREGATE: 
        case EN::FILTER: 
        case EN::SUBQUERY:
        case EN::ENUMERATE_LIST:
        case EN::INDEX: { 
          // if we found another SortNode, an AggregateNode, FilterNode, a SubqueryNode, 
          // an EnumerateListNode or an IndexNode
          // this means we cannot apply our optimization
          collectionNode = nullptr;
          current = nullptr;
          continue; // this will exit the while loop
        }

        case EN::ENUMERATE_COLLECTION: {
          if (collectionNode == nullptr) {
            // note this node
            collectionNode = current;
            break;
          }
          else {
            // we already found another collection node before. this means we
            // should not apply our optimization
            collectionNode = nullptr;
            current = nullptr;
            continue; // this will exit the while loop
          }
          // cannot get here
          TRI_ASSERT(false);
        }

        default: {
          // ignore all other nodes
        }
      }

      if (! current->hasDependency()) {
        break;
      }
          
      current = current->getFirstDependency();
    }

    if (collectionNode != nullptr) {
      // we found a node to modify!
      TRI_ASSERT(collectionNode->getType() == EN::ENUMERATE_COLLECTION);
      // set the random iteration flag for the EnumerateCollectionNode
      static_cast<EnumerateCollectionNode*>(collectionNode)->setRandom();

      // remove the SortNode
      // note: the CalculationNode will be removed by "remove-unnecessary-calculations"
      // rule if not used

      plan->unlinkNode(n);
      modified = true;
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, modified);

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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::CALCULATION, true);
  bool modified = false;
  
  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (nn->expression()->canThrow() || 
        ! nn->expression()->isDeterministic()) {
      // we will only move expressions up that cannot throw and that are deterministic
      continue;
    }

    std::unordered_set<Variable const*> neededVars;
    n->getVariablesUsedHere(neededVars);

    std::vector<ExecutionNode*> stack;

    n->addDependencies(stack);

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        if (neededVars.find(v) != neededVars.end()) {
          // shared variable, cannot move up any more
          found = true;
          break;
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }
    
         
      if (! current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note: this will also handle Singleton nodes
        break;
      }
      
      current->addDependencies(stack);

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
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move calculations as far down in the plan as possible, beyond 
/// FILTER and LIMIT operations
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::moveCalculationsDownRule (Optimizer* opt, 
                                             ExecutionPlan* plan, 
                                             Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::CALCULATION, true);
  bool modified = false;
  
  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow() || 
        ! nn->expression()->isDeterministic()) {
      // we will only move expressions down that cannot throw and that are deterministic
      continue;
    }

    // this is the variable that the calculation will set
    auto variable = nn->outVariable();

    std::vector<ExecutionNode*> stack;
    n->addParents(stack);
      
    bool shouldMove = false;
    ExecutionNode* lastNode = nullptr;

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      lastNode = current;
      bool done = false;

      auto&& varsUsed = current->getVariablesUsedHere();

      for (auto const& v : varsUsed) {
        if (v == variable) {
          // the node we're looking at needs the variable we're setting. 
          // can't push further!
          done = true;
          break;
        }
      }

      if (done) {
        // done with optimizing this calculation node
        break;
      }

      auto const currentType = current->getType();

      if (currentType == EN::FILTER ||
          currentType == EN::SORT || 
          currentType == EN::LIMIT ||
          currentType == EN::SUBQUERY) {
        // we found something interesting that justifies moving our node down
        shouldMove = true;
      } 
      else if (currentType == EN::INDEX ||
               currentType == EN::ENUMERATE_COLLECTION ||
               currentType == EN::ENUMERATE_LIST ||
               currentType == EN::AGGREGATE ||
               currentType == EN::NORESULTS) {
        // we will not push further down than such nodes
        shouldMove = false;
        break;
      }
       
      if (! current->hasParent()) {
        break;
      }
       
      current->addParents(stack);
    }

    if (shouldMove && lastNode != nullptr) {
      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(lastNode, n);
      modified = true;
    }

  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fuse calculations in the plan
/// this rule modifies the plan in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::fuseCalculationsRule (Optimizer* opt, 
                                         ExecutionPlan* plan, 
                                         Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::CALCULATION, true);
  
  if (nodes.size() < 2) {
    opt->addPlan(plan, rule, false);
    return TRI_ERROR_NO_ERROR;
  }

  std::unordered_set<ExecutionNode*> toUnlink;
 
  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow() || 
        ! nn->expression()->isDeterministic()) {
      // we will only fuse calculations of expressions that cannot throw and that are deterministic
      continue;
    }

    if (toUnlink.find(n) != toUnlink.end()) {
      // do not process the same node twice
      continue;
    }
     
    std::unordered_map<Variable const*, ExecutionNode*> toInsert;
    for (auto&& it : nn->getVariablesUsedHere()) {
      if (! n->isVarUsedLater(it)) {
        toInsert.emplace(it, n);
      }
    }

    TRI_ASSERT(n->hasDependency());
    std::vector<ExecutionNode*> stack{ n->getFirstDependency() };
      
    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool handled = false;

      if (current->getType() == EN::CALCULATION) {
        auto otherExpression = static_cast<CalculationNode const*>(current)->expression();

        if (otherExpression->isDeterministic() && 
            ! otherExpression->canThrow() &&
            otherExpression->canRunOnDBServer() == nn->expression()->canRunOnDBServer()) {
          // found another calculation node
          auto&& varsSet = current->getVariablesSetHere();
          if (varsSet.size() == 1) {
            // check if it is a calculation for a variable that we are looking for
            auto it = toInsert.find(varsSet[0]);

            if (it != toInsert.end()) {
              // remove the variable from the list of search variables
              toInsert.erase(it);

              // replace the variable reference in the original expression with the expression for that variable 
              auto expression = nn->expression();
              TRI_ASSERT(expression != nullptr);
              expression->replaceVariableReference((*it).first, otherExpression->node());

              toUnlink.emplace(current);
     
              // insert the calculations' own referenced variables into the list of search variables
              for (auto&& it2 : current->getVariablesUsedHere()) {
                if (! n->isVarUsedLater(it2)) {
                  toInsert.emplace(it2, n);
                }
              }

              handled = true;
            }
          }
        }
      }
      
      if (! handled) {
        // remove all variables from our list that might be used elsewhere
        for (auto&& it : current->getVariablesUsedHere()) {
          toInsert.erase(it);
        }
      }

      if (toInsert.empty()) {
        // done
        break;
      }

      if (! current->hasDependency()) {
        break;
      }
      
      stack.emplace_back(current->getFirstDependency());
    }
  }
        
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, ! toUnlink.empty());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the "right" type of AggregateNode and 
/// add a sort node for each COLLECT (note: the sort may be removed later) 
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::specializeCollectRule (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::AGGREGATE, true);
  bool modified = false;
  
  for (auto const& n : nodes) {
    auto collectNode = static_cast<AggregateNode*>(n);

    if (collectNode->isSpecialized()) {
      // already specialized this node
      continue;
    }

    auto const& aggregateVariables = collectNode->aggregateVariables();

    // test if we can use an alternative version of COLLECT with a hash table
    bool const canUseHashAggregation = (! aggregateVariables.empty() &&
                                        (! collectNode->hasOutVariable() || collectNode->count()) &&
                                        collectNode->getOptions().canUseHashMethod());
  
    if (canUseHashAggregation) {
      // create a new plan with the adjusted COLLECT node
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());
   
      // use the cloned COLLECT node 
      auto newCollectNode = static_cast<AggregateNode*>(newPlan->getNodeById(collectNode->id()));
      TRI_ASSERT(newCollectNode != nullptr);
      
      // specialize the AggregateNode so it will become a HashAggregateBlock later
      // additionally, add a SortNode BEHIND the AggregateNode (to sort the final result)
      newCollectNode->aggregationMethod(AggregationOptions::AggregationMethod::AGGREGATION_METHOD_HASH);
      newCollectNode->specialized();

      if (! collectNode->isDistinctCommand()) {
        // add the post-SORT
        std::vector<std::pair<Variable const*, bool>> sortElements;
        for (auto const& v : newCollectNode->aggregateVariables()) {
          sortElements.emplace_back(std::make_pair(v.first, true));
        }  

        auto sortNode = new SortNode(newPlan.get(), newPlan->nextId(), sortElements, false);
        newPlan->registerNode(sortNode);
       
        TRI_ASSERT(newCollectNode->hasParent()); 
        auto const& parents = newCollectNode->getParents();
        auto parent = parents[0];

        sortNode->addDependency(newCollectNode);
        parent->replaceDependency(newCollectNode, sortNode);
      }
      newPlan->findVarUsage();
      
      if (nodes.size() > 1) {
        // this will tell the optimizer to optimize the cloned plan with this specific rule again
        opt->addPlan(newPlan.release(), rule, true, static_cast<int>(rule->level - 1));
      }
      else {
        // no need to run this specific rule again on the cloned plan
        opt->addPlan(newPlan.release(), rule, true);
      }
    }
    
    // mark node as specialized, so we do not process it again
    collectNode->specialized();
    
    // finally, adjust the original plan and create a sorted version of COLLECT    
    
    // specialize the AggregateNode so it will become a SortedAggregateBlock later
    collectNode->aggregationMethod(AggregationOptions::AggregationMethod::AGGREGATION_METHOD_SORTED);
     
    // insert a SortNode IN FRONT OF the AggregateNode
    if (! aggregateVariables.empty()) { 
      std::vector<std::pair<Variable const*, bool>> sortElements;
      for (auto const& v : aggregateVariables) {
        sortElements.emplace_back(std::make_pair(v.second, true));
      }

      auto sortNode = new SortNode(plan, plan->nextId(), sortElements, true);
      plan->registerNode(sortNode);
      
      TRI_ASSERT(collectNode->hasDependency());
      auto dep = collectNode->getFirstDependency();
      sortNode->addDependency(dep);
      collectNode->replaceDependency(dep, sortNode);

      modified = true;
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }

  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief split and-combined filters and break them into smaller parts
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::splitFiltersRule (Optimizer* opt, 
                                     ExecutionPlan* plan,
                                     Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true);
  bool modified = false;
  
  for (auto const& n : nodes) {
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

    std::vector<AstNode const*> stack{ expression->node() };

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();
    
      if (current->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        stack.emplace_back(current->getMember(0));
        stack.emplace_back(current->getMember(1));
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
  
  opt->addPlan(plan, rule, modified);

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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true);
  bool modified = false;
  
  for (auto const& n : nodes) {
    auto neededVars = n->getVariablesUsedHere();
    TRI_ASSERT(neededVars.size() == 1);

    std::vector<ExecutionNode*> stack;
    n->addDependencies(stack);

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
      for (auto const& v : varsSet) {
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
        
      if (! current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note: this will also handle Singleton nodes
        break;
      }
      
      current->addDependencies(stack);

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
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}


class triagens::aql::RedundantCalculationsReplacer final : public WalkerWorker<ExecutionNode> {

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
      std::unordered_set<Variable const*> variables;
      node->expression()->variables(variables);
          
      // check if the calculation uses any of the variables that we want to replace
      for (auto const& it : variables) {
        if (_replacements.find(it->id) != _replacements.end()) {
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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::CALCULATION, true);
 
  if (nodes.size() < 2) {
    // quick exit
    opt->addPlan(plan, rule, false);
    return TRI_ERROR_NO_ERROR;
  }
 
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  std::unordered_map<VariableId, Variable const*> replacements;


  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (! nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }
    
    auto outvar = n->getVariablesSetHere();
    TRI_ASSERT(outvar.size() == 1);

    try {
      nn->expression()->stringifyIfNotTooLong(&buffer);
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
    n->addDependencies(stack);

    while (! stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        try {
          static_cast<CalculationNode*>(current)->expression()->stringifyIfNotTooLong(&buffer);
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

      if (! current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to do and must abort
        // note: this will also handle Singleton nodes
        break;
      }
     
      current->addDependencies(stack); 
    }
  }

  if (! replacements.empty()) {
    // finally replace the variables
    RedundantCalculationsReplacer finder(replacements);
    plan->root()->walk(&finder);
    plan->findVarUsage();

    opt->addPlan(plan, rule, true);
  }
  else {
    // no changes
    opt->addPlan(plan, rule, false);
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

  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(types, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
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
      toUnlink.emplace(n);
    }
  }

  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }

  opt->addPlan(plan, rule, ! toUnlink.empty());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief useIndex, try to use an index for filtering
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::useIndexesRule (Optimizer* opt, 
                                   ExecutionPlan* plan, 
                                   Optimizer::Rule const* rule) {
  
  // These are all the FILTER nodes where we start
  std::vector<ExecutionNode*>&& nodes = plan->findEndNodes(true);

  std::unordered_map<size_t, ExecutionNode*> changes;

  auto cleanupChanges = [&changes] () -> void {
    for (auto& v : changes) {
      delete v.second;
    }
    changes.clear();
  };

  TRI_DEFER(cleanupChanges());
  bool hasEmptyResult = false; 
  for (auto const& n : nodes) {
    ConditionFinder finder(plan, &changes, &hasEmptyResult);
    n->walk(&finder);
  }

  if (! changes.empty()) {
    for (auto& it : changes) {
      plan->registerNode(it.second); 
      plan->replaceNode(plan->getNodeById(it.first), it.second);

      // prevent double deletion by cleanupChanges()
      it.second = nullptr;
    }
    opt->addPlan(plan, rule, true);
    plan->findVarUsage();
  }
  else {
    opt->addPlan(plan, rule, hasEmptyResult);
  }

  return TRI_ERROR_NO_ERROR;
}

struct SortToIndexNode final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan*                                 _plan;
  SortNode*                                      _sortNode;
  std::vector<std::pair<VariableId, bool>>       _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  bool                                           _modified;

  public:

    SortToIndexNode (ExecutionPlan* plan)
      : _plan(plan),
        _sortNode(nullptr),
        _sorts(),
        _variableDefinitions(),
        _modified(false) {
    }

    bool handleEnumerateCollectionNode (EnumerateCollectionNode* enumerateCollectionNode) {
      if (_sortNode == nullptr) {
        return true;
      }

      if (enumerateCollectionNode->isInInnerLoop()) {
        // index node contained in an outer loop. must not optimize away the sort!
        return true;
      }
      
      SortCondition sortCondition(_sorts, _variableDefinitions);

      if (! sortCondition.isEmpty() &&
          sortCondition.isOnlyAttributeAccess() &&
          sortCondition.isUnidirectional()) {
          // we have found a sort condition, which is unidirectionl
          // now check if any of the collection's indexes covers it

        Variable const* outVariable = enumerateCollectionNode->outVariable();
        auto const& indexes = enumerateCollectionNode->collection()->getIndexes();

        for (auto& index : indexes) {
          if (! index->isSorted() || index->sparse) {
            // can only use a sorted index
            // cannot use a sparse index for sorting
            continue;
          }

          auto numCovered = sortCondition.coveredAttributes(outVariable, index->fields);

          if (numCovered == sortCondition.numAttributes()) {
            std::unique_ptr<Condition> condition(new Condition(_plan->getAst()));
            condition->normalize(_plan);
             
            std::unique_ptr<ExecutionNode> newNode(new IndexNode(
              _plan, 
              _plan->nextId(), 
              enumerateCollectionNode->vocbase(), 
              enumerateCollectionNode->collection(), 
              outVariable, 
              std::vector<Index const*>({ index }),
              condition.get(),
              sortCondition.isDescending()
            ));

            condition.release();

            auto n = newNode.release();

            _plan->registerNode(n);
            _plan->replaceNode(enumerateCollectionNode, n);

            _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
            _modified = true;
            break;
          }
        }
      }

      return true; // always abort further searching here
    }

    bool handleIndexNode (IndexNode* indexNode) {
      if (_sortNode == nullptr) {
        return true;
      }

      if (indexNode->isInInnerLoop()) {
        // index node contained in an outer loop. must not optimize away the sort!
        return true;
      }

      auto const& indexes = indexNode->getIndexes();

      if (indexes.size() != 1) {
        // can only use this index node if it uses exactly one index or multiple indexes on exactly the same attributes
        auto cond = indexNode->condition();

        if (! cond->isSorted()) {
          // index conditions do not guarantee sortedness
          return true;
        }

        std::vector<std::vector<triagens::basics::AttributeName>> seen;

        for (auto& index : indexes) {
          if (index->sparse) {
            // cannot use a sparse index for sorting
            return true;
          }

          if (! seen.empty() && triagens::basics::AttributeName::isIdentical(index->fields, seen)) {
            // different attributes
            return true;
          }
        }

        // all indexes use the same attributes and index conditions guarantee sorted output
      }

      // if we get here, we either have one index or multiple indexes on the same attributes
      auto index = indexes[0];

      if (! index->isSorted()) {
        // can only use a sorted index
        return true;
      }

      if (index->sparse) {
        // cannot use a sparse index for sorting
        return true;
      }

      SortCondition sortCondition(_sorts, _variableDefinitions);

      if (! sortCondition.isEmpty() && 
          sortCondition.isOnlyAttributeAccess() &&
          sortCondition.isUnidirectional() && 
          sortCondition.isDescending() == indexNode->reverse()) {
        // we have found a sort condition, which is unidirectional and in the same
        // order as the IndexNode...
        // now check if the sort attributes match the ones of the index
        Variable const* outVariable = indexNode->outVariable();
        auto numCovered = sortCondition.coveredAttributes(outVariable, index->fields);

        if (numCovered == sortCondition.numAttributes()) {
          // sort condition is fully covered by index... now we can remove the sort node from the plan
          _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
          _modified = true;
        }
      }

      return true; // always abort after we found an IndexNode
    }

    bool enterSubquery (ExecutionNode*, ExecutionNode*) override final { 
      return false; 
    }

    bool before (ExecutionNode* en) override final {
      switch (en->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SUBQUERY:
        case EN::FILTER:
          return false;                           // skip. we don't care.

        case EN::CALCULATION: {
          auto outvars = en->getVariablesSetHere();
          TRI_ASSERT(outvars.size() == 1);

          _variableDefinitions.emplace(outvars[0]->id, static_cast<CalculationNode const*>(en)->expression()->node());
          return false;
        }

        case EN::SINGLETON:
        case EN::AGGREGATE:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
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
          if (! _sorts.empty() || _sortNode != nullptr) {
            return true; // a different SORT node. abort
          }
          _sortNode = static_cast<SortNode*>(en);
          for (auto& it : _sortNode->getElements()) {
            _sorts.emplace_back((it.first)->id, it.second);
          }
          return false;

        case EN::INDEX: 
          return handleIndexNode(static_cast<IndexNode*>(en));

        case EN::ENUMERATE_COLLECTION:
          return handleEnumerateCollectionNode(static_cast<EnumerateCollectionNode*>(en));
      }
      return true;
    }
};

int triagens::aql::useIndexForSortRule (Optimizer* opt, 
                                        ExecutionPlan* plan,
                                        Optimizer::Rule const* rule) {

  bool modified = false;
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::SORT, true);

  for (auto const& n : nodes) {
    auto sortNode = static_cast<SortNode*>(n);

    SortToIndexNode finder(plan);
    sortNode->walk(&finder);

    if (finder._modified) {
      modified = true;
    }
  }
    
  if (modified) {
    plan->findVarUsage();
  }
        
  opt->addPlan(plan, rule, modified, modified ? Optimizer::RuleLevel::pass5 : 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to remove filters which are covered by indexes
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeFiltersCoveredByIndexRule (Optimizer* opt,
                                                    ExecutionPlan* plan,
                                                    Optimizer::Rule const* rule) {
  std::unordered_set<ExecutionNode*> toUnlink;
  bool modified = false;
  std::vector<ExecutionNode*>&& nodes= plan->findNodesOfType(EN::FILTER, true); 
  
  for (auto const& node : nodes) {
    auto fn = static_cast<FilterNode const*>(node);
    // find the node with the filter expression
    auto inVar = fn->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);
          
    auto setter = plan->getVarSetBy(inVar[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }
        
    auto calculationNode = static_cast<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition 
    std::unique_ptr<Condition> condition(new Condition(plan->getAst()));
    condition->andCombine(conditionNode);
    condition->normalize(plan);

    if (condition->root() == nullptr) {
      continue;
    }

    size_t const n = condition->root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = static_cast<IndexNode const*>(current);

        // found an index node, now check if the expression is covered by the index
        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && ! indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            // single index. this is something that we can handle

            auto newNode = condition->removeIndexCondition(indexNode->outVariable(), indexCondition->root());

            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(setter);
              toUnlink.emplace(node);
              modified = true;
              handled = true;
            }
            else if (newNode != condition->root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              std::unique_ptr<Expression> expr(new Expression(plan->getAst(), newNode));
              CalculationNode* cn = new CalculationNode(plan, plan->nextId(), expr.get(), calculationNode->outVariable());
              expr.release();
              plan->registerNode(cn);
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled) {
        break;
      }

      if (! current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }
  }
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, modified);

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

static bool NextPermutationTuple (std::vector<size_t>& data,
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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::ENUMERATE_COLLECTION, true);

  std::unordered_set<ExecutionNode*> nodesSet;
  for (auto const& n : nodes) {
    TRI_ASSERT(nodesSet.find(n) == nodesSet.end());
    nodesSet.emplace(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto const& n : nodes) {
    if (nodesSet.find(n) != nodesSet.end()) {
      std::vector<ExecutionNode*> nn{ n };
      nodesSet.erase(n);

      // Now follow the dependencies as long as we see further such nodes:
      auto nwalker = n;

      while (true) {
        if (! nwalker->hasDependency()) {
          break;
        }

        auto dep = nwalker->getFirstDependency();

        if (dep->getType() != EN::ENUMERATE_COLLECTION) {
          break;
        }

        nwalker = dep;
        nn.emplace_back(nwalker);
        nodesSet.erase(nwalker);
      }

      if (nn.size() > 1) {
        // Move it into the permutation tuple:
        starts.emplace_back(permTuple.size());

        for (auto const& nnn : nn) {
          nodesToPermute.emplace_back(nnn);
          permTuple.emplace_back(permTuple.size());
        }
      }
    }
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  opt->addPlan(plan, rule, false);

  if (! starts.empty()) {
    NextPermutationTuple(permTuple, starts);  // will never return false

    do {
      // Clone the plan:
      auto newPlan = plan->clone();

      try {   // get rid of plan if any of this fails
        // Find the nodes in the new plan corresponding to the ones in the
        // old plan that we want to permute:
        std::vector<ExecutionNode*> newNodes;
        for (size_t j = 0; j < nodesToPermute.size(); j++) {
          newNodes.emplace_back(newPlan->getNodeById(nodesToPermute[j]->id()));
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
          auto const& parents = newNodes[lowBound]->getParents();

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
        if (! opt->addPlan(newPlan, rule, true)) {
          // have enough plans. stop permutations
          break;
        }
      }
      catch (...) {
        delete newPlan;
        throw;
      }

    } 
    while (NextPermutationTuple(permTuple, starts));
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

  if (triagens::arango::ServerState::instance()->isCoordinator()) {
    // we are a coordinator. now look in the plan for nodes of type
    // EnumerateCollectionNode and IndexNode
    std::vector<ExecutionNode::NodeType> const types = { 
      ExecutionNode::ENUMERATE_COLLECTION, 
      ExecutionNode::INDEX,
      ExecutionNode::INSERT,
      ExecutionNode::UPDATE,
      ExecutionNode::REPLACE,
      ExecutionNode::REMOVE
    }; 

    std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(types, true);

    for (auto& node: nodes) {
      // found a node we need to replace in the plan

      auto const& parents = node->getParents();
      auto const& deps = node->getDependencies();
      TRI_ASSERT(deps.size() == 1);
      bool const isRootNode = plan->isRoot(node);
      // don't do this if we are already distributing!
      if (deps[0]->getType() == ExecutionNode::REMOTE &&
          deps[0]->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE) {
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
      else if (nodeType == ExecutionNode::INDEX) {
        vocbase = static_cast<IndexNode*>(node)->vocbase();
        collection = static_cast<IndexNode*>(node)->collection();
      }
      else if (nodeType == ExecutionNode::INSERT ||
               nodeType == ExecutionNode::UPDATE ||
               nodeType == ExecutionNode::REPLACE ||
               nodeType == ExecutionNode::REMOVE ||
               nodeType == ExecutionNode::UPSERT) {
        vocbase = static_cast<ModificationNode*>(node)->vocbase();
        collection = static_cast<ModificationNode*>(node)->collection();
        if (nodeType == ExecutionNode::REMOVE ||
            nodeType == ExecutionNode::UPDATE) {
          // Note that in the REPLACE or UPSERT case we are not getting here, since
          // the distributeInClusterRule fires and a DistributionNode is
          // used.
          auto* modNode = static_cast<ModificationNode*>(node);
          modNode->getOptions().ignoreDocumentNotFound = true;
        }
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
      wasModified = true;
    }
  }
      
  if (wasModified) {
    plan->findVarUsage();
  }
   
  opt->addPlan(plan, rule, wasModified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only sent to one shard and not all as in scatterInCluster
///
/// it will change plans in place
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::distributeInClusterRule (Optimizer* opt,
                                            ExecutionPlan* plan,
                                            Optimizer::Rule const* rule) {
  bool wasModified = false;
        
  if (triagens::arango::ServerState::instance()->isCoordinator()) {
    // we are a coordinator, we replace the root if it is a modification node
    
    // only replace if it is the last node in the plan
    auto node = plan->root();
    TRI_ASSERT(node != nullptr);

    while (node != nullptr) {
      // loop until we find a modification node or the end of the plan
      auto nodeType = node->getType();

      if (nodeType == ExecutionNode::INSERT  ||
          nodeType == ExecutionNode::REMOVE  ||
          nodeType == ExecutionNode::UPDATE ||
          nodeType == ExecutionNode::REPLACE ||
          nodeType == ExecutionNode::UPSERT) {
        // found a node!
        break;
      }

      if (! node->hasDependency()) {
        // reached the end
        opt->addPlan(plan, rule, wasModified);

        return TRI_ERROR_NO_ERROR;
      }

      node = node->getFirstDependency();
    }

    TRI_ASSERT(node != nullptr);
    
    if (node == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
    }
    
    ExecutionNode* originalParent = nullptr;
    {
      if (node->hasParent()) {
        auto const& parents = node->getParents();
        originalParent = parents[0];
        TRI_ASSERT(originalParent != nullptr);
        TRI_ASSERT(node != plan->root());
      }
      else {
        TRI_ASSERT(node == plan->root());
      }
    }

    // when we get here, we have found a matching data-modification node!
    auto const nodeType = node->getType();
    
    TRI_ASSERT(nodeType == ExecutionNode::INSERT  ||
               nodeType == ExecutionNode::REMOVE  ||
               nodeType == ExecutionNode::UPDATE  ||
               nodeType == ExecutionNode::REPLACE ||
               nodeType == ExecutionNode::UPSERT);

    Collection const* collection = static_cast<ModificationNode*>(node)->collection();
   
    bool const defaultSharding = collection->usesDefaultSharding();

    if (nodeType == ExecutionNode::REMOVE ||
        nodeType == ExecutionNode::UPDATE) {
      if (! defaultSharding) {
        // We have to use a ScatterNode.
        opt->addPlan(plan, rule, wasModified);
        return TRI_ERROR_NO_ERROR;
      }
    }
    
    
    // In the INSERT and REPLACE cases we use a DistributeNode...

    TRI_ASSERT(node->hasDependency());
    auto const& deps = node->getDependencies();
    
    if (originalParent != nullptr) {
      originalParent->removeDependency(node);
      // unlink the node
      auto root = plan->root();
      plan->unlinkNode(node, true);
      plan->root(root, true); // fix root node
    }
    else {
      // unlink the node
      plan->unlinkNode(node, true);
      plan->root(deps[0], true); // fix root node
    }


    // extract database from plan node
    TRI_vocbase_t* vocbase = static_cast<ModificationNode*>(node)->vocbase();

    // insert a distribute node
    ExecutionNode* distNode = nullptr;
    Variable const* inputVariable;
    if (nodeType == ExecutionNode::INSERT ||
        nodeType == ExecutionNode::REMOVE) {
      TRI_ASSERT(node->getVariablesUsedHere().size() == 1);

      // in case of an INSERT, the DistributeNode is responsible for generating keys
      // if none present
      bool const createKeys = (nodeType == ExecutionNode::INSERT);
      inputVariable = node->getVariablesUsedHere()[0];
      distNode = new DistributeNode(plan, plan->nextId(), 
          vocbase, collection, inputVariable->id, createKeys);
    }
    else if (nodeType == ExecutionNode::REPLACE) {
      std::vector<Variable const*> v = node->getVariablesUsedHere();
      if (defaultSharding && v.size() > 1) {
        // We only look into _inKeyVariable
        inputVariable = v[1];
      }
      else {
        // We only look into _inDocVariable
        inputVariable = v[0];
      }
      distNode = new DistributeNode(plan, plan->nextId(), 
            vocbase, collection, inputVariable->id, false);
    }
    else if (nodeType == ExecutionNode::UPDATE) {
      std::vector<Variable const*> v = node->getVariablesUsedHere();
      if (v.size() > 1) {
        // If there is a key variable:
        inputVariable = v[1];
        // This is the _inKeyVariable! This works, since we use a ScatterNode
        // for non-default-sharding attributes.
      }
      else {
        // was only UPDATE <doc> IN <collection>
        inputVariable = v[0];
      }
      distNode = new DistributeNode(plan, plan->nextId(), 
          vocbase, collection, inputVariable->id, false);
    }
    else if (nodeType == ExecutionNode::UPSERT) {
      // an UPSERT nodes has two input variables!
      std::vector<Variable const*> const&& v = node->getVariablesUsedHere();
      TRI_ASSERT(v.size() >= 2);

      distNode = new DistributeNode(plan, plan->nextId(), 
          vocbase, collection, v[0]->id, v[2]->id, false);
    }
    else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
    }

    TRI_ASSERT(distNode != nullptr);

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
    ExecutionNode* gatherNode = new GatherNode(plan, plan->nextId(), vocbase, collection);
    plan->registerNode(gatherNode);
    gatherNode->addDependency(remoteNode);

    if (originalParent != nullptr) {
      // we did not replace the root node
      originalParent->addDependency(gatherNode);
    }
    else {
      // we replaced the root node, set a new root node
      plan->root(gatherNode, true);
    }
    wasModified = true;
  
    plan->findVarUsage();
  }
   
  opt->addPlan(plan, rule, wasModified);
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

  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::GATHER, true);

  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (! n->hasParent()) {
      continue;
    }

    auto parents = n->getParents();

    while (true) {
      bool stopSearching = false;
      auto inspectNode = parents[0];

      switch (inspectNode->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
          parents = inspectNode->getParents();
          continue;

        case EN::AGGREGATE:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::ILLEGAL:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::SORT:
        case EN::INDEX:
        case EN::ENUMERATE_COLLECTION:
          //do break
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
  
  opt->addPlan(plan, rule, modified);
  
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

  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::GATHER, true);
  
  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    auto gatherNode = static_cast<GatherNode*>(n);
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (! n->hasParent()) {
      continue;
    }

    auto parents = n->getParents();

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
        case EN::UPSERT:
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
        case EN::INDEX:
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
  
  opt->addPlan(plan, rule, modified);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeUnnecessaryRemoteScatterRule (Optimizer* opt, 
                                                       ExecutionPlan* plan, 
                                                       Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::REMOTE, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    // check if the remote node is preceeded by a scatter node and any number of
    // calculation and singleton nodes. if yes, remove remote and scatter
    if (! n->hasDependency()) {
      continue;
    }

    auto const dep = n->getFirstDependency();    
    if (dep->getType() != EN::SCATTER) {
      continue;
    }

    bool canOptimize = true;
    auto node = dep;
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
      toUnlink.emplace(n);
      toUnlink.emplace(dep);
    }
  }

  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, ! toUnlink.empty());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// WalkerWorker for undistributeRemoveAfterEnumColl
////////////////////////////////////////////////////////////////////////////////

class RemoveToEnumCollFinder final : public WalkerWorker<ExecutionNode> {
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
        _lastNode(nullptr) {
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
          _toUnlink.emplace(en);
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::DISTRIBUTE:
        case EN::SCATTER: {
          if (_scatter) { // met more than one scatter node
            break;        // abort . . . 
          }
          _scatter = true;
          _toUnlink.emplace(en);
          _lastNode = en;
          return false; // continue . . .
        }
        case EN::GATHER: {
          if (_gather) { // met more than one gather node
            break;       // abort . . . 
          }
          _gather = true;
          _toUnlink.emplace(en);
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
        case EN::UPSERT:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::ILLEGAL:
        case EN::LIMIT:           
        case EN::SORT:
        case EN::INDEX: {
          // if we meet any of the above, then we abort . . .
        }
    }
    _toUnlink.clear();
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief recognizes that a RemoveNode can be moved to the shards.
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::undistributeRemoveAfterEnumCollRule (Optimizer* opt, 
                                                        ExecutionPlan* plan, 
                                                        Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::REMOVE, true);
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    RemoveToEnumCollFinder finder(plan, toUnlink);
    n->walk(&finder);
  }

  bool modified = false;
  if (! toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    plan->findVarUsage();
    modified = true;
  }
  
  opt->addPlan(plan, rule, modified);

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
              || node->type == NODE_TYPE_OPERATOR_BINARY_GT
              || node->type == NODE_TYPE_OPERATOR_BINARY_IN))) {

      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      bool const isIn = (node->type == NODE_TYPE_OPERATOR_BINARY_IN && rhs->isArray());

      if (! isIn && lhs->isConstant()) {
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

      if (! isIn && 
          (lhs->type == NODE_TYPE_FCALL || 
           lhs->type == NODE_TYPE_FCALL_USER ||
           lhs->type == NODE_TYPE_REFERENCE)) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (! isIn &&
          (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
           lhs->type == NODE_TYPE_INDEXED_ACCESS)) {
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
          possibleNodes.emplace_back(lhs);
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
          possibleNodes.emplace_back(rhs);
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
    auto list = ast->createNodeArray();
    for (auto& x : valueNodes) {
      list->addMember(x);
    }

    // return a new IN operator node
    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN,
                                         commonNode->clone(ast),
                                         list);
  }

  bool canConvertExpression (AstNode const* node) {
    if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_EQ, commonNode, commonName)) {
      return canConvertExpressionWalker(node);
    }
    else if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_IN, commonNode, commonName)) {
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
        valueNodes.emplace_back(lhs);
        return true;
      } 
      
      if (canConvertExpressionWalker(lhs) && ! canConvertExpressionWalker(rhs)) {
        valueNodes.emplace_back(rhs);
        return true;
      } 
      // if canConvertExpressionWalker(lhs) and canConvertExpressionWalker(rhs), then one of
      // the equalities in the OR statement is of the form x == x
      // fall-through intentional
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (canConvertExpressionWalker(lhs) && ! canConvertExpressionWalker(rhs) && rhs->isArray()) {
        size_t const n = rhs->numMembers();

        for (size_t i = 0; i < n; ++i) {
          valueNodes.emplace_back(rhs->getMemberUnchecked(i));
        }
        return true;
      } 
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
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();
    
    auto cn = static_cast<CalculationNode*>(dep);
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    OrToInConverter converter;
    if (converter.canConvertExpression(cn->expression()->node())) {
      ExecutionNode* newNode = nullptr;
      auto inNode = converter.buildInExpression(plan->getAst());

      Expression* expr = new Expression(plan->getAst(), inNode);

      try {
        TRI_IF_FAILURE("OptimizerRules::replaceOrWithInRuleOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

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
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
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
  bool compareBounds (AstNodeType type, AstNode const* value, int lowhigh) { 
    int cmp = CompareAstNodes(bestValue, value, true);

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

    return false;
  }
};

int triagens::aql::removeRedundantOrRule (Optimizer* opt, 
                                          ExecutionPlan* plan, 
                                          Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();
    
    auto cn = static_cast<CalculationNode*>(dep);
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
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::removeDataModificationOutVariablesRule (Optimizer* opt, 
                                                           ExecutionPlan* plan, 
                                                           Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode::NodeType> const types = {
    EN::REMOVE,
    EN::INSERT,
    EN::UPDATE,
    EN::REPLACE,
    EN::UPSERT
  };

  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(types, true);
  
  for (auto const& n : nodes) {
    auto node = static_cast<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    auto varsUsedLater = n->getVarsUsedLater();
    if (varsUsedLater.find(node->getOutVariableOld()) == varsUsedLater.end()) {
      // "$OLD" is not used later
      node->clearOutVariableOld();
      modified = true;
    }
    
    if (varsUsedLater.find(node->getOutVariableNew()) == varsUsedLater.end()) {
      // "$NEW" is not used later
      node->clearOutVariableNew();
      modified = true;
    }
  }
  
  if (modified) {
    plan->findVarUsage();
  }
  
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief patch UPDATE statement on single collection that iterates over the
/// entire collection to operate in batches
////////////////////////////////////////////////////////////////////////////////

int triagens::aql::patchUpdateStatementsRule (Optimizer* opt, 
                                              ExecutionPlan* plan, 
                                              Optimizer::Rule const* rule) {
  bool modified = false;

  // not need to dive into subqueries here, as UPDATE needs to be on the top level
  std::vector<ExecutionNode*>&& nodes = plan->findNodesOfType(EN::UPDATE, false);
  
  for (auto const& n : nodes) {
    // we should only get through here a single time
    auto node = static_cast<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    auto& options = node->getOptions();
    if (! options.readCompleteInput) {
      // already ok
      continue;
    }

    auto const collection = node->collection();

    auto dep = n->getFirstDependency();

    while (dep != nullptr) {
      auto const type = dep->getType();

      if (type == EN::ENUMERATE_LIST || 
          type == EN::INDEX ||
          type == EN::SUBQUERY) {
        // not suitable
        modified = false;
        break;
      }

      if (type == EN::ENUMERATE_COLLECTION) {
        auto collectionNode = static_cast<EnumerateCollectionNode const*>(dep);

        if (collectionNode->collection() != collection) {
          // different collection, not suitable
          modified = false;
          break;
        }
        else {
          modified = true;
        }
      }

      dep = dep->getFirstDependency();
    }

    if (modified) {
      options.readCompleteInput = false;
    }
  }
  
  // always re-add the original plan, be it modified or not
  // only a flag in the plan will be modified
  opt->addPlan(plan, rule, modified);

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

