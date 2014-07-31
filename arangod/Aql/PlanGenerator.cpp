////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query plan generator
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

#include "Aql/PlanGenerator.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
  
// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

PlanGenerator::PlanGenerator () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

PlanGenerator::~PlanGenerator () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an initial execution plan from an abstract syntax tree
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromAst (Ast const* ast) {
  TRI_ASSERT(ast != nullptr);
  
  auto root = ast->root();
  TRI_ASSERT(root != nullptr);
  TRI_ASSERT(root->type == NODE_TYPE_ROOT);

  ExecutionPlan* plan = fromNode(ast, root);

  return plan;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds "previous" as dependency to "plan", returns "plan"
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::addDependency (ExecutionPlan* previous,
                                             ExecutionPlan* plan) {
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

ExecutionPlan* PlanGenerator::fromNodeFor (Ast const* ast,
                                           ExecutionPlan* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FOR);
  TRI_ASSERT(node->numMembers() == 2);

  auto variable = node->getMember(0);
  auto expression = node->getMember(1);

  // fetch 1st operand (out variable name)
  TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);
  
  ExecutionPlan* plan = nullptr;

  // peek at second operand
  if (expression->type == NODE_TYPE_COLLECTION) {
    // second operand is a collection
    char const* collectionName = expression->getStringValue();
    plan = new EnumerateCollectionPlan(ast->query()->vocbase(), std::string(collectionName), v->id, v->name); 
  }
  else {
    // generate a temporary variable
    auto out = ast->variables()->createTemporaryVariable();

    // generate a temporary calculation node
    CalculationPlan* calc;
    auto expr = new AqlExpression(expression);

    try {
      calc = new CalculationPlan(expr, out->id, out->name); 
    }
    catch (...) {
      // prevent memleak
      delete expr;
      throw;
    }

    TRI_ASSERT(calc != nullptr);

    try {
      plan = new EnumerateListPlan(out->id, out->name, v->id, v->name); 
      plan->addDependency(calc);
    }
    catch (...) {
      // prevent memleak
      delete calc;
    }
  }

  TRI_ASSERT(plan != nullptr);
  
  return addDependency(previous, plan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FILTER node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeFilter (Ast const* ast,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FILTER);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto variable = node->getMember(0);
  auto v = static_cast<Variable*>(variable->getData());
  
  auto plan = new FilterPlan(v->id, v->name);
  return addDependency(previous, plan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LET node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeLet (Ast const* ast,
                                           ExecutionPlan* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LET);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode const* variable = node->getMember(0);
  AstNode const* expression = node->getMember(1);

  auto v = static_cast<Variable*>(variable->getData());

  // TODO: node might be a subquery. this is currently NOT handled
  auto expr = new AqlExpression(const_cast<AstNode*>(expression));
  try {
    auto plan = new CalculationPlan(expr, v->id, v->name);
    return addDependency(previous, plan);
  }
  catch (...) {
    // prevent memleak
    delete expr;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST SORT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeSort (Ast const* ast,
                                            ExecutionPlan* previous,
                                            AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_SORT);
  TRI_ASSERT(node->numMembers() == 1);

  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST COLLECT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeCollect (Ast const* ast,
                                               ExecutionPlan* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_COLLECT);
  size_t const n = node->numMembers();

  TRI_ASSERT(n >= 1);
  
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LIMIT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeLimit (Ast const* ast,
                                             ExecutionPlan* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LIMIT);
  TRI_ASSERT(node->numMembers() == 2);

  auto offset = node->getMember(0);
  auto count  = node->getMember(1);

  TRI_ASSERT(offset->type == NODE_TYPE_VALUE);
  TRI_ASSERT(count->type == NODE_TYPE_VALUE);

  auto plan = new LimitPlan(static_cast<size_t>(offset->getIntValue()), static_cast<size_t>(count->getIntValue()));

  return addDependency(previous, plan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST RETURN node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeReturn (Ast const* ast,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_RETURN);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto variable = node->getMember(0);
  // TODO: the operand type of return is not necessarily a variable...
  auto v = static_cast<Variable*>(variable->getData());

  auto plan = new RootPlan(v->id, v->name);

  return addDependency(previous, plan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REMOVE node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeRemove (Ast const* ast,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REMOVE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST INSERT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeInsert (Ast const* ast,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_INSERT);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST UPDATE node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeUpdate (Ast const* ast,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_UPDATE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REPLACE node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeReplace (Ast const* ast,
                                               ExecutionPlan* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REPLACE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an abstract syntax tree node
////////////////////////////////////////////////////////////////////////////////
  
ExecutionPlan* PlanGenerator::fromNode (Ast const* ast,
                                        AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  ExecutionPlan* plan = new SingletonPlan();

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member == nullptr || member->type == NODE_TYPE_NOP) {
      continue;
    }

    switch (member->type) {
      case NODE_TYPE_FOR: {
        plan = fromNodeFor(ast, plan, member);
        break;
      }

      case NODE_TYPE_FILTER: {
        plan = fromNodeFilter(ast, plan, member);
        break;
      }

      case NODE_TYPE_LET: {
        plan = fromNodeLet(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_SORT: {
        plan = fromNodeSort(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_COLLECT: {
        plan = fromNodeCollect(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_LIMIT: {
        plan = fromNodeLimit(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_RETURN: {
        plan = fromNodeReturn(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_REMOVE: {
        plan = fromNodeRemove(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_INSERT: {
        plan = fromNodeInsert(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_UPDATE: {
        plan = fromNodeUpdate(ast, plan, member);
        break;
      }
      
      case NODE_TYPE_REPLACE: {
        plan = fromNodeReplace(ast, plan, member);
        break;
      }

      default: {
        // node type not implemented
        plan = nullptr;
        break;
      }
    }

    if (plan == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  return plan;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
