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
#include "Aql/WalkerWorker.h"
#include "Basics/JsonHelper.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using namespace triagens::basics;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the plan
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan::ExecutionPlan () 
  : _ids(),
    _root(nullptr),
    _nextId(0) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the plan, frees all assigned nodes
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan::~ExecutionPlan () {
  for (auto x : _ids){
    delete x.second;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an AST
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* ExecutionPlan::instanciateFromAst (Ast* ast) {
  TRI_ASSERT(ast != nullptr);

  auto root = ast->root();
  TRI_ASSERT(root != nullptr);
  TRI_ASSERT(root->type == NODE_TYPE_ROOT);

  auto plan = new ExecutionPlan();

  try {
    plan->_root = plan->fromNode(ast, root);
    plan->findVarUsage();
// just for debugging
/*
    auto JsonPlan = plan->_root->toJson();
    auto JsonString = JsonPlan.toString();
    std::cout << JsonString << "\n";
    auto otherPlan = ExecutionPlan::instanciateFromJson (ast,
                                                         JsonPlan);
    auto otherJsonString = otherPlan->_root->toJson().toString();
    std::cout << otherJsonString << "\n";
    TRI_ASSERT(otherJsonString == JsonString);
    return otherPlan;
*/
//
    return plan;
  }
  catch (...) {
    delete plan;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from JSON
////////////////////////////////////////////////////////////////////////////////

ExecutionPlan* ExecutionPlan::instanciateFromJson (Ast* ast,
                                                   triagens::basics::Json const& json) {
  auto plan = new ExecutionPlan();

  try {
    plan->_root = plan->fromJson(ast, json);
    plan->findVarUsage();
    return plan;
  }
  catch (...) {
    delete plan;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a node by its id
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::getNodeById (size_t id) const {
  auto it = _ids.find(id);
  
  if (it != _ids.end()) {
    // node found
    return (*it).second;
  }

  // node unknown
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "node not found");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create modification options from an AST node
////////////////////////////////////////////////////////////////////////////////

ModificationOptions ExecutionPlan::createOptions (AstNode const* node) {
  ModificationOptions options;

  // parse the modification options we got
  if (node != nullptr && 
      node->type == NODE_TYPE_ARRAY) {
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMember(i);

      if (member != nullptr && 
          member->type == NODE_TYPE_ARRAY_ELEMENT) {
        auto name = member->getStringValue();
        auto value = member->getMember(0);

        TRI_ASSERT(value->isConstant());

        if (strcmp(name, "waitForSync") == 0) {
          options.waitForSync = value->toBoolean();
        }
        else if (strcmp(name, "ignoreErrors") == 0) {
          options.ignoreErrors = value->toBoolean();
        }
        else if (strcmp(name, "keepNull") == 0) {
          // nullMeansRemove is the opposite of keepNull
          options.nullMeansRemove = (! value->toBoolean());
        }
      }
    }
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a node with the plan, will delete node if addition fails
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::registerNode (ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->id() > 0);

  try {
    _ids.insert(std::make_pair(node->id(), node));
  }
  catch (...) {
    delete node;
    throw;
  }
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister a node to the plan
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::unregisterNode (ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->id() > 0);

  auto it = _ids.find(node->id());
  TRI_ASSERT(it != _ids.end());
  TRI_ASSERT(it->second == node);

  _ids.erase(it);
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
    auto en = new CalculationNode(nextId(), expr, out);

    registerNode(reinterpret_cast<ExecutionNode*>(en));
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no collection for EnumerateCollection");
    }
    en = registerNode(new EnumerateCollectionNode(nextId(), ast->query()->vocbase(), collection, v));
  }
  else if (expression->type == NODE_TYPE_REFERENCE) {
    // second operand is already a variable
    auto inVariable = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(inVariable != nullptr);
    en = registerNode(new EnumerateListNode(nextId(), inVariable, v));
  }
  else {
    // second operand is some misc. expression
    auto calc = createTemporaryCalculation(ast, expression);

    calc->addDependency(previous);
    en = registerNode(new EnumerateListNode(nextId(), calc->outVariable(), v));
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
    en = registerNode(new FilterNode(nextId(), v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);

    calc->addDependency(previous);
    en = registerNode(new FilterNode(nextId(), calc->outVariable()));
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

    en = registerNode(new SubqueryNode(nextId(), subquery, v));
  }
  else {
    // operand is some misc expression, including references to other variables
    auto expr = new Expression(ast->query()->executor(), const_cast<AstNode*>(expression));

    try {
      en = registerNode(new CalculationNode(nextId(), expr, v));
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

  auto en = registerNode(new SortNode(nextId(), elements));

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
  auto sort = registerNode(new SortNode(nextId(), sortElements));
  sort->addDependency(previous);
  previous = sort;

  // handle out variable
  Variable* outVariable = nullptr;

  if (n == 2) {
    // collect with an output variable!
    auto v = node->getMember(1);
    outVariable = static_cast<Variable*>(v->getData());
  }

  auto en = registerNode(new AggregateNode(nextId(), aggregateVariables, outVariable, ast->variables()->variables(false)));

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

  auto en = registerNode(new LimitNode(nextId(), static_cast<size_t>(offset->getIntValue()), static_cast<size_t>(count->getIntValue())));

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
    en = registerNode(new ReturnNode(nextId(), v));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = registerNode(new ReturnNode(nextId(), calc->outVariable()));
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
  TRI_ASSERT(node->numMembers() == 3);
  
  auto options = createOptions(node->getMember(0));
  char const* collectionName = node->getMember(1)->getStringValue();
  auto collections = ast->query()->collections();
  auto collection = collections->get(collectionName);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no collection for RemoveNode");
  }

  auto expression = node->getMember(2);
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new RemoveNode(nextId(), ast->query()->vocbase(), collection, options, v, nullptr));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = registerNode(new RemoveNode(nextId(), ast->query()->vocbase(), collection, options, calc->outVariable(), nullptr));
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
  TRI_ASSERT(node->numMembers() == 3);
  
  auto options = createOptions(node->getMember(0));
  char const* collectionName = node->getMember(1)->getStringValue();
  auto collections = ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto expression = node->getMember(2);
  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new InsertNode(nextId(), ast->query()->vocbase(), collection, options, v, nullptr));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, expression);
    calc->addDependency(previous);
    en = registerNode(new InsertNode(nextId(), ast->query()->vocbase(), collection, options, calc->outVariable(), nullptr));
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
  TRI_ASSERT(node->numMembers() >= 3);
  
  auto options = createOptions(node->getMember(0));
  char const* collectionName = node->getMember(1)->getStringValue();
  auto collections = ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto docExpression = node->getMember(2);
  auto keyExpression = node->getOptionalMember(3);
  Variable const* keyVariable = nullptr;
  ExecutionNode* en = nullptr;

  if (keyExpression != nullptr) {
    if (keyExpression->type == NODE_TYPE_REFERENCE) {
      // key operand is already a variable
      auto v = static_cast<Variable*>(keyExpression->getData());
      TRI_ASSERT(v != nullptr);
      keyVariable = v;
    }
    else {
      // key operand is some misc expression
      auto calc = createTemporaryCalculation(ast, keyExpression);
      calc->addDependency(previous);
      previous = calc;
      keyVariable = calc->outVariable();
    }
  }

  if (docExpression->type == NODE_TYPE_REFERENCE) {
    // document operand is already a variable
    auto v = static_cast<Variable*>(docExpression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new UpdateNode(nextId(), ast->query()->vocbase(), collection, options, v, keyVariable, nullptr));
  }
  else {
    // document operand is some misc expression
    auto calc = createTemporaryCalculation(ast, docExpression);
    calc->addDependency(previous);
    en = registerNode(new UpdateNode(nextId(), ast->query()->vocbase(), collection, options, calc->outVariable(), keyVariable, nullptr));
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
  TRI_ASSERT(node->numMembers() >= 3);
  
  auto options = createOptions(node->getMember(0));
  char const* collectionName = node->getMember(1)->getStringValue();
  auto collections = ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto docExpression = node->getMember(2);
  auto keyExpression = node->getOptionalMember(3);
  Variable const* keyVariable = nullptr;
  ExecutionNode* en = nullptr;
  
  if (keyExpression != nullptr) {
    if (keyExpression->type == NODE_TYPE_REFERENCE) {
      // key operand is already a variable
      auto v = static_cast<Variable*>(keyExpression->getData());
      TRI_ASSERT(v != nullptr);
      keyVariable = v;
    }
    else {
      // key operand is some misc expression
      auto calc = createTemporaryCalculation(ast, keyExpression);
      calc->addDependency(previous);
      previous = calc;
      keyVariable = calc->outVariable();
    }
  }
  
  if (docExpression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(docExpression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new ReplaceNode(nextId(), ast->query()->vocbase(), collection, options, v, keyVariable, nullptr));
  }
  else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(ast, docExpression);
    calc->addDependency(previous);
    en = registerNode(new ReplaceNode(nextId(), ast->query()->vocbase(), collection, options, calc->outVariable(), keyVariable, nullptr));
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

  ExecutionNode* en = registerNode(new SingletonNode(nextId()));

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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "type not handled");
    }
  }

  return en;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find nodes of a certain type
////////////////////////////////////////////////////////////////////////////////

class NodeFinder : public WalkerWorker<ExecutionNode> {

    ExecutionNode::NodeType _lookingFor;

    std::vector<ExecutionNode*>& _out;

    bool _enterSubqueries;

  public:
    NodeFinder (ExecutionNode::NodeType lookingFor,
                std::vector<ExecutionNode*>& out,
                bool enterSubqueries) 
      : _lookingFor(lookingFor), _out(out), _enterSubqueries(enterSubqueries) {
    };

    void before (ExecutionNode* en) {
      if (en->getType() == _lookingFor) {
        _out.push_back(en);
      }
    }

    bool enterSubquery (ExecutionNode* super, ExecutionNode* sub) {
      return _enterSubqueries;
    }
};

std::vector<ExecutionNode*> ExecutionPlan::findNodesOfType (
                                  ExecutionNode::NodeType type,
                                  bool enterSubqueries) {

  std::vector<ExecutionNode*> result;
  NodeFinder finder(type, result, enterSubqueries);
  root()->walk(&finder);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct for findVarUsage
////////////////////////////////////////////////////////////////////////////////

struct VarUsageFinder : public WalkerWorker<ExecutionNode> {

    std::unordered_set<Variable const*> _usedLater;
    std::unordered_set<Variable const*> _valid;
    std::unordered_map<VariableId, ExecutionNode*> _varSetBy;

    VarUsageFinder () {
    }

    ~VarUsageFinder () {
    }

    void before (ExecutionNode* en) {
      en->invalidateVarUsage();
      en->setVarsUsedLater(_usedLater);
      // Add variables used here to _usedLater:
      auto&& usedHere = en->getVariablesUsedHere();
      for (auto v : usedHere) {
        _usedLater.insert(v);
      }
    }

    void after (ExecutionNode* en) {
      // Add variables set here to _valid:
      auto&& setHere = en->getVariablesSetHere();
      for (auto v : setHere) {
        _valid.insert(v);
        _varSetBy.insert(std::make_pair(v->id, en));
      }
      en->setVarsValid(_valid);
      en->setVarUsageValid();
    }

    bool enterSubquery (ExecutionNode* super, ExecutionNode* sub) {
      VarUsageFinder subfinder;
      subfinder._valid = _valid;  // need a copy for the subquery!
      sub->walk(&subfinder);
      
      // we've fully processed the subquery
      return false;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief determine and set _varsUsedLater in all nodes
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::findVarUsage () {
  VarUsageFinder finder;
  root()->walk(&finder);
  _varSetBy = finder._varSetBy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinkNodes, note that this does not delete the removed
/// nodes and that one cannot remove the root node of the plan.
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::unlinkNodes (std::unordered_set<ExecutionNode*>& toRemove) {
  for (auto x : toRemove) {
    unlinkNode(x);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinkNode, note that this does not delete the removed
/// node and that one cannot remove the root node of the plan.
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::unlinkNode (ExecutionNode* node) {
  auto&& parents = node->getParents();
  if (parents.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Cannot unlink root node of plan.");
  }
  else if (parents.size() > 1) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
        "Cannot remove node with more than one parent.");
  }
  else {
    auto&& dep = node->getDependencies();
    parents[0]->removeDependency(node);
    for (auto x : dep) {
      parents[0]->addDependency(x);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaceNode, note that <newNode> must be registered with the plan
/// before this method is called, also this does not delete the old
/// node and that one cannot replace the root node of the plan.
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::replaceNode (ExecutionNode* oldNode, 
                                 ExecutionNode* newNode) { 
  TRI_ASSERT(oldNode->id() != newNode->id());
  TRI_ASSERT(newNode->getDependencies().empty());
  TRI_ASSERT(oldNode != _root);

  std::vector<ExecutionNode*> deps = oldNode->getDependencies();
  
  for (auto x : deps) {
    newNode->addDependency(x);
  }
  
  auto&& oldNodeParents = oldNode->getParents();
  for (auto oldNodeParent : oldNodeParents) {
    if(! oldNodeParent->replaceDependency(oldNode, newNode)){
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                  "Could not replace dependencies of an old node.");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert <newNode> before <oldNode>. <newNode> must be registered with 
/// the plan before this method is called
////////////////////////////////////////////////////////////////////////////////

void ExecutionPlan::insertDependency (ExecutionNode* oldNode, 
                                      ExecutionNode* newNode) {
  TRI_ASSERT(oldNode->id() != newNode->id());
  TRI_ASSERT(newNode->getDependencies().size() == 1);
  TRI_ASSERT(oldNode != _root);

  auto&& oldDeps = oldNode->getDependencies();
  if (! oldNode->replaceDependency(oldDeps[0], newNode)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                "Could not replace dependencies of an old node.");
  }

  newNode->removeDependencies();
  newNode->addDependency(oldDeps[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone the plan by recursively cloning starting from the root
////////////////////////////////////////////////////////////////////////////////

class CloneNodeAdder : public WalkerWorker<ExecutionNode> {
    ExecutionPlan* _plan;
  
  public:

    CloneNodeAdder (ExecutionPlan* plan) : _plan(plan){}
    
    ~CloneNodeAdder (){}

    void before (ExecutionNode* node){
      _plan->registerNode(node);
    }
};

ExecutionPlan* ExecutionPlan::clone (){
  auto plan = new ExecutionPlan();
  try {
    plan->_root = _root->clone();
    plan->_nextId = _nextId;
    CloneNodeAdder adder(plan);
    plan->_root->walk(&adder);
    plan->findVarUsage();
    return plan;
  }
  catch (...) {
    delete plan;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a plan from the JSON provided
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ExecutionPlan::fromJson (Ast* ast,
                                        Json const& json) {
  ExecutionNode* ret = nullptr;
  Json nodes = json.get("nodes");

  if (! nodes.isList()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nodes is not a list");
  }

  // first, re-create all nodes from the JSON, using the node ids
  // no dependency links will be set up in this step
  auto const size = nodes.size();

  for (size_t i = 0; i < size; i++) {
    Json oneJsonNode = nodes.at(i);

    if (! oneJsonNode.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json node is not an array");
    }
    
    ret = ExecutionNode::fromJsonFactory(ast,
                                         oneJsonNode);

    registerNode(ret);

    TRI_ASSERT(ret != nullptr);

    if (ret->getType() == triagens::aql::ExecutionNode::SUBQUERY) {
      // found a subquery node. now do magick here
      Json subquery = oneJsonNode.get("subquery");
      // create the subquery nodes from the "subquery" sub-node
      auto subqueryNode = fromJson(ast, subquery);
    
      // register the just created subquery 
      static_cast<SubqueryNode*>(ret)->setSubquery(subqueryNode); 
    }
  }

  // all nodes have been created. now add the dependencies

  for (size_t i = 0; i < size; i++) {
    Json oneJsonNode = nodes.at(i);

    if (! oneJsonNode.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json node is not an array");
    }
   
    // read the node's own id 
    auto thisId = JsonHelper::getNumericValue<size_t>(oneJsonNode.json(), "id", 0);
    auto thisNode = getNodeById(thisId);

    // now re-link the dependencies
    Json dependencies = oneJsonNode.get("dependencies");
    if (JsonHelper::isList(dependencies.json())) {
      size_t const nDependencies = dependencies.size();

      for (size_t j = 0; j < nDependencies; j ++) {
        if (JsonHelper::isNumber(dependencies.at(j).json())) {
          auto depId = JsonHelper::getNumericValue<size_t>(dependencies.at(j).json(), 0);
          thisNode->addDependency(getNodeById(depId));
        }
      }
    }
  }

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
