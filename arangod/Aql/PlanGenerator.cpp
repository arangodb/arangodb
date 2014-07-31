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

ExecutionPlan* PlanGenerator::fromAst (Query* query,
                                       Ast const* ast) {
  TRI_ASSERT(ast != nullptr);
  
  auto root = ast->root();
  TRI_ASSERT(root != nullptr);
  TRI_ASSERT(root->type == NODE_TYPE_ROOT);

  ExecutionPlan* plan = fromNode(query, root);

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

ExecutionPlan* PlanGenerator::fromNodeFor (Query* query,
                                           ExecutionPlan* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FOR);
  TRI_ASSERT(node->numMembers() == 2);

  auto variable = node->getMember(0);
  auto expression = node->getMember(1);
  
  ExecutionPlan* plan = nullptr;

  if (expression->type == NODE_TYPE_COLLECTION) {
    char const* collectionName = expression->getStringValue();
    plan = new EnumerateCollectionPlan(query->vocbase(), std::string(collectionName), 0, "0"); // FIXME
  }
  else if (expression->type == NODE_TYPE_REFERENCE) {
    auto v = static_cast<Variable*>(variable->getData());
    plan = new EnumerateListPlan(v->id, v->name, 0, "0"); // FIXME
  }
  else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  
  return addDependency(previous, plan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FILTER node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeFilter (Query* query,
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

ExecutionPlan* PlanGenerator::fromNodeLet (Query* query,
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

ExecutionPlan* PlanGenerator::fromNodeSort (Query* query,
                                            ExecutionPlan* previous,
                                            AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_SORT);
  TRI_ASSERT(node->numMembers() == 1);

  // AstNode const* list = node->getMember(0);
  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST COLLECT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeCollect (Query* query,
                                               ExecutionPlan* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_COLLECT);
  size_t const n = node->numMembers();

  TRI_ASSERT(n >= 1);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LIMIT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeLimit (Query* query,
                                             ExecutionPlan* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LIMIT);
  TRI_ASSERT(node->numMembers() == 2);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST RETURN node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeReturn (Query* query,
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

ExecutionPlan* PlanGenerator::fromNodeRemove (Query* query,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REMOVE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST INSERT node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeInsert (Query* query,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_INSERT);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST UPDATE node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeUpdate (Query* query,
                                              ExecutionPlan* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_UPDATE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REPLACE node
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* PlanGenerator::fromNodeReplace (Query* query,
                                               ExecutionPlan* previous,
                                               AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REPLACE);

  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an abstract syntax tree node
////////////////////////////////////////////////////////////////////////////////
  
ExecutionPlan* PlanGenerator::fromNode (Query* query,
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
        plan = fromNodeFor(query, plan, member);
        break;
      }

      case NODE_TYPE_FILTER: {
        plan = fromNodeFilter(query, plan, member);
        break;
      }

      case NODE_TYPE_LET: {
        plan = fromNodeLet(query, plan, member);
        break;
      }
      
      case NODE_TYPE_SORT: {
        plan = fromNodeSort(query, plan, member);
        break;
      }
      
      case NODE_TYPE_COLLECT: {
        plan = fromNodeCollect(query, plan, member);
        break;
      }
      
      case NODE_TYPE_LIMIT: {
        plan = fromNodeLimit(query, plan, member);
        break;
      }
      
      case NODE_TYPE_RETURN: {
        plan = fromNodeReturn(query, plan, member);
        break;
      }
      
      case NODE_TYPE_REMOVE: {
        plan = fromNodeRemove(query, plan, member);
        break;
      }
      
      case NODE_TYPE_INSERT: {
        plan = fromNodeInsert(query, plan, member);
        break;
      }
      
      case NODE_TYPE_UPDATE: {
        plan = fromNodeUpdate(query, plan, member);
        break;
      }
      
      case NODE_TYPE_REPLACE: {
        plan = fromNodeReplace(query, plan, member);
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
