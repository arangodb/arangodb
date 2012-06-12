////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, optimiser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-optimiser.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"

#include "V8/v8-execution.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a node recursively
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseNode (TRI_aql_statement_walker_t* const, 
                                     TRI_aql_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const, 
                                         TRI_aql_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief pick an index for the ranges found
////////////////////////////////////////////////////////////////////////////////

static void AttachCollectionHint (TRI_aql_context_t* const context,
                                  TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_vector_pointer_t* availableIndexes;
  TRI_aql_collection_hint_t* hint;
  TRI_aql_index_t* idx;
  TRI_aql_collection_t* collection;
  char* collectionName;

  collectionName = TRI_AQL_NODE_STRING(nameNode);
  assert(collectionName);

  hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(node);

  if (hint == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  if (hint->_ranges == NULL) {
    // no ranges found to be used as indexes

    return;
  }

  collection = TRI_GetCollectionAql(context, collectionName);
  if (collection == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  hint->_collection = collection;

  availableIndexes = &(((TRI_sim_collection_t*) collection->_collection->_collection)->_indexes);

  if (availableIndexes == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  idx = TRI_DetermineIndexAql(context, 
                              availableIndexes, 
                              collectionName, 
                              hint->_ranges);

  hint->_index = idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief annotate a node with context information
///
/// this is a callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* AnnotateNode (TRI_aql_statement_walker_t* const walker,
                                     TRI_aql_node_t* node) {
  TRI_aql_context_t* context;
  
  if (node->_type != TRI_AQL_NODE_COLLECTION) {
    return node;
  }
  
  context = (TRI_aql_context_t*) walker->_data;

  AttachCollectionHint(context, node);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a relational operation
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* RelationCode (const char* const name, 
                                          const TRI_aql_node_t* const lhs,
                                          const TRI_aql_node_t* const rhs) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);

  if (!lhs || !rhs) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){return AHUACATL_RELATIONAL_") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
    
  if (!TRI_NodeJavascriptAql(buffer, lhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
      
  if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (!TRI_NodeJavascriptAql(buffer, rhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, ");})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* FcallCode (const char* const name, 
                                       const TRI_aql_node_t* const args) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  size_t i;
  size_t n;

  if (!buffer) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){return AHUACATL_FCALL(") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, ",[") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];
    if (i > 0) {
      if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
        TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
        return NULL;
      }
    }

    if (!TRI_NodeJavascriptAql(buffer, arg)) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
      return NULL;
    }
  }

  if (TRI_AppendStringStringBuffer(buffer, "]);})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFcall (TRI_aql_context_t* const context,
                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* args = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_function_t* function;
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  size_t i;
  size_t n;

  function = (TRI_aql_function_t*) TRI_AQL_NODE_DATA(node);
  assert(function);

  // check if function is deterministic
  if (!function->_isDeterministic) {
    return node;
  }

  // check if function call arguments are deterministic
  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];

    if (!arg || !TRI_IsConstantValueNodeAql(arg)) {
      return node;
    }
  }
 
  // all arguments are constants
  // create the function code
  code = FcallCode(function->_internalName, args);
  if (!code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (!execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (!json) {
    // cannot optimise the function call due to an internal error

    // TODO: check whether we can validate the arguments here already and return an error early
    // TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, "function optimisation");
    return node;
  }

  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  TRI_AQL_LOG("optimised function call");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a sort expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseSort (TRI_aql_statement_walker_t* const walker,
                                     TRI_aql_node_t* node) {
  TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(node, 0);
  size_t i, n;
    
  if (!list) {
    return node;
  }

  i = 0;
  n = list->_members._length;
  while (i < n) {
    // sort element
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(element, 0);

    // check if the sort element is constant
    if (!expression || !TRI_IsConstantValueNodeAql(expression)) {
      ++i;
      continue;
    }

    // sort element is constant so it can be removed
    TRI_RemoveVectorPointer(&list->_members, i);
    --n;

    TRI_AQL_LOG("optimised away sort element");
  }

  if (n == 0) {
    // no members left => sort removed
    TRI_AQL_LOG("optimised away sort");

    return TRI_GetDummyNopNodeAql();
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a constant filter expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseConstantFilter (TRI_aql_node_t* const node) {
  if (TRI_GetBooleanNodeValueAql(node)) {
    // filter expression is always true => remove it
    TRI_AQL_LOG("optimised away constant (true) filter");

    return TRI_GetDummyNopNodeAql();
  }

  // filter expression is always false => invalidate surrounding scope(s)
  TRI_AQL_LOG("optimised away scope"); 

  return TRI_GetDummyReturnEmptyNodeAql();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a filter expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFilter (TRI_aql_statement_walker_t* const walker,
                                       TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) walker->_data;
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 0);
 
  while (true) {
    TRI_vector_pointer_t* oldRanges;
    TRI_vector_pointer_t* newRanges;
    bool changed;

    if (!expression) {
      return node;
    }

    if (TRI_IsConstantValueNodeAql(expression)) {
      // filter expression is a constant value
      return OptimiseConstantFilter(expression);
    }

    // filter expression is non-constant
    oldRanges = TRI_GetCurrentRangesStatementWalkerAql(walker);
    
    changed = false;
    newRanges = TRI_OptimiseRangesAql(context, expression, &changed, oldRanges);
    
    if (newRanges) {
      TRI_SetCurrentRangesStatementWalkerAql(walker, newRanges);
    }
    
    if (!changed) {
      break;
    }

    // expression code was changed, set pointer to new value re-optimise it
    node->_members._buffer[0] = OptimiseNode(walker, expression);
    expression = TRI_AQL_NODE_MEMBER(node, 0);

    // next iteration
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a reference expression
///
/// this looks up the source node that defines the variable and checks if the
/// variable has a constant value. if yes, then the reference is replaced with
/// the constant value
/// e.g. in the query "let a = 1 for x in a ...", the latter a would be replaced
/// by the value "1".
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseReference (TRI_aql_statement_walker_t* const walker,
                                          TRI_aql_node_t* node) {
  TRI_aql_variable_t* variable;
  TRI_aql_node_t* definingNode;
  char* variableName = (char*) TRI_AQL_NODE_STRING(node);

  assert(variableName);
  variable = TRI_GetVariableStatementWalkerAql(walker, variableName);

  if (variable == NULL) {
    return node;
  }

  definingNode = variable->_definingNode;
  if (definingNode == NULL) {
    return node;
  }

  if (definingNode->_type == TRI_AQL_NODE_LET) {
    // variable is defined via a let
    TRI_aql_node_t* expressionNode;

    expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
    if (expressionNode && TRI_IsConstantValueNodeAql(expressionNode)) {
      // the source variable is constant, so we can replace the reference with 
      // the source's value
      return expressionNode;
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryArithmeticOperation (TRI_aql_context_t* const context,
                                                         TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_UNARY_PLUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_UNARY_MINUS);

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsNumericValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_PLUS) {
    // + number => number
    node = operand;
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_MINUS) {
    // - number => eval!
    node = TRI_CreateNodeValueDoubleAql(context, - TRI_GetNumericNodeValueAql(operand));
    if (node == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryLogicalOperation (TRI_aql_context_t* const context,
                                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_UNARY_NOT);

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    // node is not a constant value
    return node;
  }

  if (!TRI_IsBooleanValueNodeAql(operand)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  // ! (bool value) => evaluate and replace with result
  node = TRI_CreateNodeValueBoolAql(context, ! TRI_GetBooleanNodeValueAql(operand));
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  TRI_AQL_LOG("optimised away unary logical operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryLogicalOperation (TRI_aql_context_t* const context,
                                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  bool lhsValue;

  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);

  if (isEligibleLhs && !TRI_IsBooleanValueNodeAql(lhs)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (isEligibleRhs && !TRI_IsBooleanValueNodeAql(rhs)) {
    // value type is not boolean => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (!isEligibleLhs || !isEligibleRhs) {
    // node is not a constant value
    return node;
  }

  lhsValue = TRI_GetBooleanNodeValueAql(lhs);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR);

  TRI_AQL_LOG("optimised away binary logical operation");

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
    if (lhsValue) {
      // if (true && rhs) => rhs
      return rhs;
    }

    // if (false && rhs) => false
    return lhs;
  } 
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR) {
    if (lhsValue) {
      // if (true || rhs) => true
      return lhs;
    }

    // if (false || rhs) => rhs
    return rhs;
  }

  assert(false);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a relational operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryRelationalOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  char* func;
  
  if (!lhs || !TRI_IsConstantValueNodeAql(lhs) || !rhs || !TRI_IsConstantValueNodeAql(rhs)) {
    return node;
  } 
  
  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_EQ) {
    func = "EQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_NE) {
    func = "UNEQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GT) {
    func = "GREATER";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GE) {
    func = "GREATER_EQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LT) {
    func = "LESS";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LE) {
    func = "LESS_EQUAL";
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_IN) {
    func = "IN";
  }
  else {
    // not what we expected, however, simply continue
    return node;
  }
  
  code = RelationCode(func, lhs, rhs); 
  if (!code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (!execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (!json) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, NULL);
    return NULL;
  }
  
  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }
    
  TRI_AQL_LOG("optimised away binary relational operation");

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryArithmeticOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  double value;
  
  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);
  
  if (isEligibleLhs && !TRI_IsNumericValueNodeAql(lhs)) {
    // node is not a numeric value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  
  if (isEligibleRhs && !TRI_IsNumericValueNodeAql(rhs)) {
    // node is not a numeric value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }
  
  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_BINARY_PLUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MINUS ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_TIMES ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_DIV ||
         node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MOD);

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_PLUS) {
    value = TRI_GetNumericNodeValueAql(lhs) + TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MINUS) {
    value = TRI_GetNumericNodeValueAql(lhs) - TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_TIMES) {
    value = TRI_GetNumericNodeValueAql(lhs) * TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_DIV) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // division by zero
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISION_BY_ZERO, NULL);
      return node;
    }
    value = TRI_GetNumericNodeValueAql(lhs) / TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_MOD) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      // division by zero
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISION_BY_ZERO, NULL);
      return node;
    }
    value = fmod(TRI_GetNumericNodeValueAql(lhs), TRI_GetNumericNodeValueAql(rhs));
  }
  else {
    value = 0.0;
  }
  
  node = TRI_CreateNodeValueDoubleAql(context, value);

  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  TRI_AQL_LOG("optimised away binary arithmetic operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise the ternary operation
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseTernaryOperation (TRI_aql_context_t* const context,
                                                 TRI_aql_node_t* node) {
  TRI_aql_node_t* condition = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* truePart = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_aql_node_t* falsePart = TRI_AQL_NODE_MEMBER(node, 2);
  
  assert(node->_type == TRI_AQL_NODE_OPERATOR_TERNARY);

  if (!condition || !TRI_IsConstantValueNodeAql(condition)) {
    // node is not a constant value
    return node;
  }

  if (!TRI_IsBooleanValueNodeAql(condition)) {
    // node is not a boolean value => error
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  if (!truePart || !falsePart) {
    // true or false parts not defined
    //  should not happen but we must not continue in this case
    return node;
  }
    
  TRI_AQL_LOG("optimised away ternary operation");
  
  // evaluate condition
  if (TRI_GetBooleanNodeValueAql(condition)) {
    // condition is true, replace with truePart
    return truePart;
  }
  
  // condition is true, replace with falsePart
  return falsePart;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a specific node
///
/// This is a callback function called by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseNode (TRI_aql_statement_walker_t* const walker, 
                                     TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) walker->_data;

  assert(node);

  // node optimisations
  switch (node->_type) {
    case TRI_AQL_NODE_OPERATOR_UNARY_PLUS:
    case TRI_AQL_NODE_OPERATOR_UNARY_MINUS:
      return OptimiseUnaryArithmeticOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_UNARY_NOT:
      return OptimiseUnaryLogicalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_AND:
    case TRI_AQL_NODE_OPERATOR_BINARY_OR:
      return OptimiseBinaryLogicalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_EQ:
    case TRI_AQL_NODE_OPERATOR_BINARY_NE:
    case TRI_AQL_NODE_OPERATOR_BINARY_LT:
    case TRI_AQL_NODE_OPERATOR_BINARY_LE:
    case TRI_AQL_NODE_OPERATOR_BINARY_GT:
    case TRI_AQL_NODE_OPERATOR_BINARY_GE:
    case TRI_AQL_NODE_OPERATOR_BINARY_IN:
      return OptimiseBinaryRelationalOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_BINARY_PLUS:
    case TRI_AQL_NODE_OPERATOR_BINARY_MINUS:
    case TRI_AQL_NODE_OPERATOR_BINARY_TIMES:
    case TRI_AQL_NODE_OPERATOR_BINARY_DIV:
    case TRI_AQL_NODE_OPERATOR_BINARY_MOD:
      return OptimiseBinaryArithmeticOperation(context, node);
    case TRI_AQL_NODE_OPERATOR_TERNARY:
      return OptimiseTernaryOperation(context, node);
    case TRI_AQL_NODE_FCALL:
      return OptimiseFcall(context, node);
    case TRI_AQL_NODE_REFERENCE:
      return OptimiseReference(walker, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseStatement (TRI_aql_statement_walker_t* const walker, 
                                          TRI_aql_node_t* node) {
  assert(walker);
  assert(node);
   
  // node optimisations
  switch (node->_type) {
    case TRI_AQL_NODE_SORT:
      return OptimiseSort(walker, node);
    case TRI_AQL_NODE_FILTER:
      return OptimiseFilter(walker, node);
    default: {
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief patch variables with range information
////////////////////////////////////////////////////////////////////////////////

static void PatchVariables (TRI_aql_statement_walker_t* const walker) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) walker->_data;
  TRI_vector_pointer_t* ranges;
  size_t i, n;

  ranges = TRI_GetCurrentRangesStatementWalkerAql(walker);
  if (ranges == NULL) {
    // no ranges defined, exit early
    return;
  }

  // iterate over all ranges found
  n = ranges->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess;
    TRI_aql_variable_t* variable;
    TRI_aql_node_t* definingNode;
    TRI_aql_node_t* expressionNode;
    char* variableName;

    fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(ranges, i);
    assert(fieldAccess);
    assert(fieldAccess->_fullName);
    assert(fieldAccess->_variableNameLength > 0);

    variableName = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fullName, fieldAccess->_variableNameLength);
    if (variableName == NULL) {
      // out of memory!
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }

    variable = TRI_GetVariableStatementWalkerAql(walker, variableName);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, variableName);

    if (variable == NULL) {
      continue;
    }

    // note: we must not modify outer variables of subqueries 

    // get the node that defines the variable
    definingNode = variable->_definingNode;

    assert(definingNode != NULL);

    expressionNode = NULL;
    switch (definingNode->_type) {
      case TRI_AQL_NODE_LET:
        expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
        break;
      case TRI_AQL_NODE_FOR:
        expressionNode = TRI_AQL_NODE_MEMBER(definingNode, 1);
        break;
      default: {
      }
    }

    if (expressionNode != NULL) {
      if (expressionNode->_type == TRI_AQL_NODE_COLLECTION) {
        TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) (TRI_AQL_NODE_DATA(expressionNode));

        // set new value
        hint->_ranges = TRI_AddAccessAql(context, hint->_ranges, fieldAccess);
      }
    }
  } 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
///
/// this is a callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const walker,
                                         TRI_aql_node_t* node) {
  if (node) {
    // this may change the node pointer
    node = OptimiseStatement(walker, node);

    // patch variables with range infos
    if (node->_type == TRI_AQL_NODE_SCOPE_END) {
      PatchVariables(walker);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise the AST
////////////////////////////////////////////////////////////////////////////////

static bool OptimiseAst (TRI_aql_context_t* const context) {
  TRI_aql_statement_walker_t* walker;

  walker = TRI_CreateStatementWalkerAql((void*) context, 
                                        true,
                                        &OptimiseNode,
                                        NULL, 
                                        &ProcessStatement);
  if (walker == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }

  TRI_WalkStatementsAql(walker, context->_statements); 

  TRI_FreeStatementWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which indexes to use in the query
////////////////////////////////////////////////////////////////////////////////

static bool DetermineIndexes (TRI_aql_context_t* const context) {
  TRI_aql_statement_walker_t* walker;

  walker = TRI_CreateStatementWalkerAql((void*) context, 
                                        false,
                                        &AnnotateNode, 
                                        NULL, 
                                        NULL);
  if (walker == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }

  TRI_WalkStatementsAql(walker, context->_statements); 

  TRI_FreeStatementWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseAql (TRI_aql_context_t* const context) {
  if (!OptimiseAst(context)) {
    return false;
  }

  if (!DetermineIndexes(context)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
