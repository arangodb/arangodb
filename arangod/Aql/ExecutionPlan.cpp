////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution plan
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the plan
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan::ExecutionPlan () 
  : _nodes(),
    _root(nullptr) {

  _nodes.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the plan, frees all assigned nodes
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan::~ExecutionPlan () {
  for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
    delete (*it);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an AST
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* ExecutionPlan::instanciateFromAst (Ast const* ast) {
  TRI_ASSERT(ast != nullptr);

  auto root = ast->root();
  TRI_ASSERT(root != nullptr);
  TRI_ASSERT(root->type == NODE_TYPE_ROOT);

  auto plan = new ExecutionPlan();

  try {
    plan->_root = plan->fromNode(ast, root);
    
    std::cout << "ESTIMATED COST = €" << plan->getCost() << "\n";
    std::cout << plan->_root->toJson().toString() << "\n";

    return plan;
  }
  catch (...) {
    delete plan;
    throw;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a node to the plan, will delete node if addition fails
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::addNode (ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);

  try {
    _nodes.push_back(node);
    return node;
  }
  catch (...) {
    delete node;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a calculation node for an arbitrary expression
////////////////////////////////////////////////////////////////////////////////

CalculationNode* ExecutionPlan::createTemporaryCalculation (Ast const* ast,
                                                            AstNode const* expression) {
  // generate a temporary variable
  auto out = ast->variables()->createTemporaryVariable();
  TRI_ASSERT(out != nullptr);

  // generate a temporary calculation node
  auto expr = new Expression(ast->query()->executor(), const_cast<AstNode*>(expression));

  try {
    auto en = new CalculationNode(expr, out);

    addNode(reinterpret_cast<ExecutionNode*>(en));
    return en;
  }
  catch (...) {
    // prevent memleak
    delete expr;
    throw;
    // no need to delete "out" as this is automatically freed by the variables management
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds "previous" as dependency to "plan", returns "plan"
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::addDependency (ExecutionNode* previous,
                                             ExecutionNode* plan) {
  TRI_ASSERT(previous != nullptr);
  TRI_ASSERT(plan != nullptr);

  try {
    plan->addDependency(previous);
    return plan;
  }
  catch (...) {
    // prevent memleak
    delete plan;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FOR node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeFor (Ast const* ast,
                                           ExecutionNode* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FOR);
  TRI_ASSERT(node->numMembers() == 2);

  auto variable = node->getMember(0);
  auto expression = node->getMember(1);

  // fetch 1st operand (out variable name)
  TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);
  
  ExecutionNode* en = nullptr;

  // peek at second operand
  if (expression->type == NODE_TYPE_COLLECTION) {
    // second operand is a collection
    char const* collectionName = expression->getStringValue();
    auto collections = ast->query()->collections();
    auto collection = collections->get(collectionName);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    en = addNode(new EnumerateCollectionNode(ast->query()->vocbase(), collection, v));
  }
  else if (expression->type == NODE_TYPE_REFERENCE) {
    // second operand is already a variable
    auto inVariable = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(inVariable != nullptr);
    en = addNode(new EnumerateListNode(inVariable, v));
  }
  else {
    // second operand is some misc. expression
    auto calc = createTemporaryCalculation(ast, expression);

    calc->addDependency(previous);
    en = addNode(new EnumerateListNode(calc->outVariable(), v));
    previous = calc;
  }

  TRI_ASSERT(en != nullptr);
  
  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FILTER node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeFilter (Ast const* ast,
                                              ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FILTER);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto expression = node->getMember(0);

  ExecutionNode* en = nullptr;
  
  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new FilterNode(v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);

    calc->addDependency(previous);
    en = addNode(new FilterNode(calc->outVariable()));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LET node
/// this also includes handling of subqueries (as subqueries can only occur 
/// inside LET nodes)
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeLet (Ast const* ast,
                                           ExecutionNode* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LET);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode const* variable = node->getMember(0);
  AstNode const* expression = node->getMember(1);

  auto v = static_cast<Variable*>(variable->getData());
  
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_SUBQUERY) {
    // operand is a subquery...
    auto subquery = fromNode(ast, expression);

    if (subquery == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    en = addNode(new SubqueryNode(subquery, v));
  }
  else {
    // operand is some misc expression, including references to other variables
    auto expr = new Expression(ast->query()->executor(), const_cast<AstNode*>(expression));

    try {
      en = addNode(new CalculationNode(expr, v));
    }
    catch (...) {
      // prevent memleak
      delete expr;
      throw;
    }
  }
    
  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST SORT node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeSort (Ast const* ast,
                                            ExecutionNode* previous,
                                            AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_SORT);
  TRI_ASSERT(node->numMembers() == 1);

  auto list = node->getMember(0);
  TRI_ASSERT(list->type == NODE_TYPE_LIST);

  std::vector<std::pair<Variable const*, bool>> elements;
  std::vector<CalculationNode*> temp;

  try {
    size_t const n = list->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto element = list->getMember(i);
      TRI_ASSERT(element != nullptr);
      TRI_ASSERT(element->type == NODE_TYPE_SORT_ELEMENT);
      TRI_ASSERT(element->numMembers() == 1);

      auto expression = element->getMember(0);

      if (expression->type == NODE_TYPE_REFERENCE) {
        // sort operand is a variable
        auto v = static_cast<Variable*>(expression->getData());
        TRI_ASSERT(v != nullptr);
        elements.push_back(std::make_pair(v, element->getBoolValue()));
      }
      else {
        // sort operand is some misc expression
        auto calc = createTemporaryCalculation(ast, expression);
        temp.push_back(calc);
        elements.push_back(std::make_pair(calc->outVariable(), element->getBoolValue()));
      }
    }
  }
  catch (...) {
    // prevent memleak
    for (auto it = temp.begin(); it != temp.end(); ++it) {
      delete (*it);
    }
    throw;
  }

  TRI_ASSERT(! elements.empty());

  // properly link the temporary calculations in the plan
  for (auto it = temp.begin(); it != temp.end(); ++it) {
    (*it)->addDependency(previous);
    previous = (*it);
  }

  auto en = addNode(new SortNode(elements));

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST COLLECT node
/// note that also a sort plan node will be added in front of the collect plan
/// node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeCollect (Ast const* ast,
                                               ExecutionNode* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_COLLECT);
  size_t const n = node->numMembers();

  TRI_ASSERT(n >= 1);

  auto list = node->getMember(0);
  size_t const numVars = list->numMembers();
  
  std::vector<std::pair<Variable const*, bool>> sortElements;

  std::vector<std::pair<Variable const*, Variable const*>> aggregateVariables;
  aggregateVariables.reserve(numVars);
  for (size_t i = 0; i < numVars; ++i) {
    auto assigner = list->getMember(i);

    if (assigner == nullptr) {
      continue;
    }

    TRI_ASSERT(assigner->type == NODE_TYPE_ASSIGN);
    auto out = assigner->getMember(0);
    TRI_ASSERT(out != nullptr);
    auto v = static_cast<Variable*>(out->getData());
    TRI_ASSERT(v != nullptr);
   
    auto expression = assigner->getMember(1);
      
    if (expression->type == NODE_TYPE_REFERENCE) {
      // operand is a variable
      auto e = static_cast<Variable*>(expression->getData());
      aggregateVariables.push_back(std::make_pair(v, e));
      sortElements.push_back(std::make_pair(e, true));
    }
    else {
      // operand is some misc expression
      auto calc = createTemporaryCalculation(ast, expression);

      calc->addDependency(previous);
      previous = calc;

      aggregateVariables.push_back(std::make_pair(v, calc->outVariable()));
      sortElements.push_back(std::make_pair(calc->outVariable(), true));
    }
  }

  // inject a sort node for all expressions / variables that we just picked up...
  auto sort = addNode(new SortNode(sortElements));
  sort->addDependency(previous);
  previous = sort;

  // handle out variable
  Variable* outVariable = nullptr;

  if (n == 2) {
    // collect with an output variable!
    auto v = node->getMember(1);
    outVariable = static_cast<Variable*>(v->getData());
  }

  auto en = addNode(new AggregateNode(aggregateVariables, outVariable, ast->variables()->variables(false)));

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LIMIT node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeLimit (Ast const* ast,
                                             ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LIMIT);
  TRI_ASSERT(node->numMembers() == 2);

  auto offset = node->getMember(0);
  auto count  = node->getMember(1);

  TRI_ASSERT(offset->type == NODE_TYPE_VALUE);
  TRI_ASSERT(count->type == NODE_TYPE_VALUE);

  auto en = addNode(new LimitNode(static_cast<size_t>(offset->getIntValue()), static_cast<size_t>(count->getIntValue())));

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST RETURN node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeReturn (Ast const* ast,
                                              ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_RETURN);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto expression = node->getMember(0);

  ExecutionNode* en = nullptr;
  
  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new ReturnNode(v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = addNode(new ReturnNode(calc->outVariable()));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REMOVE node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeRemove (Ast const* ast,
                                              ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REMOVE);
  TRI_ASSERT(node->numMembers() == 2);
  
  char const* collectionName = node->getMember(0)->getStringValue();
  auto collections = ast->query()->collections();
  auto collection = collections->get(collectionName);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto expression = node->getMember(1);
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new RemoveNode(ast->query()->vocbase(), collection, v, nullptr));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = addNode(new RemoveNode(ast->query()->vocbase(), collection, calc->outVariable(), nullptr));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST INSERT node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeInsert (Ast const* ast,
                                              ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_INSERT);
  TRI_ASSERT(node->numMembers() == 2);
  
  auto collection = node->getMember(0);
  auto expression = node->getMember(1);

  // collection, expression
  char const* collectionName = collection->getStringValue();
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new InsertNode(ast->query()->vocbase(), std::string(collectionName), v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = addNode(new InsertNode(ast->query()->vocbase(), std::string(collectionName), calc->outVariable()));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST UPDATE node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeUpdate (Ast const* ast,
                                              ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_UPDATE);
  TRI_ASSERT(node->numMembers() >= 2);
  
  auto collection = node->getMember(0);
  auto expression = node->getMember(1);

  // collection, expression
  char const* collectionName = collection->getStringValue();
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new UpdateNode(ast->query()->vocbase(), std::string(collectionName), v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = addNode(new UpdateNode(ast->query()->vocbase(), std::string(collectionName), calc->outVariable()));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REPLACE node
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromNodeReplace (Ast const* ast,
                                               ExecutionNode* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REPLACE);
  TRI_ASSERT(node->numMembers() >= 2);
  
  auto collection = node->getMember(0);
  auto expression = node->getMember(1);

  // collection, expression
  char const* collectionName = collection->getStringValue();
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = addNode(new ReplaceNode(ast->query()->vocbase(), std::string(collectionName), v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = addNode(new ReplaceNode(ast->query()->vocbase(), std::string(collectionName), calc->outVariable()));
    previous = calc;
  }

  return addDependency(previous, en);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an abstract syntax tree node
////////////////////////////////////////////////////////////////////////////////
  
ExecutionNode* ExecutionPlan::fromNode (Ast const* ast,
                                        AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  ExecutionNode* en = addNode(new SingletonNode());

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member == nullptr || member->type == NODE_TYPE_NOP) {
      continue;
    }

    switch (member->type) {
      case NODE_TYPE_FOR: {
        en = fromNodeFor(ast, en, member);
        break;
      }

      case NODE_TYPE_FILTER: {
        en = fromNodeFilter(ast, en, member);
        break;
      }

      case NODE_TYPE_LET: {
        en = fromNodeLet(ast, en, member);
        break;
      }
    
      case NODE_TYPE_SORT: {
        en = fromNodeSort(ast, en, member);
        break;
      }
    
      case NODE_TYPE_COLLECT: {
        en = fromNodeCollect(ast, en, member);
        break;
      }
      
      case NODE_TYPE_LIMIT: {
        en = fromNodeLimit(ast, en, member);
        break;
      }
    
      case NODE_TYPE_RETURN: {
        en = fromNodeReturn(ast, en, member);
        break;
      }
    
      case NODE_TYPE_REMOVE: {
        en = fromNodeRemove(ast, en, member);
        break;
      }
    
      case NODE_TYPE_INSERT: {
        en = fromNodeInsert(ast, en, member);
        break;
      }
    
      case NODE_TYPE_UPDATE: {
        en = fromNodeUpdate(ast, en, member);
        break;
      }
    
      case NODE_TYPE_REPLACE: {
        en = fromNodeReplace(ast, en, member);
        break;
      }

      default: {
        // node type not implemented
        en = nullptr;
        break;
      }
    }

    if (en == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  return en;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
