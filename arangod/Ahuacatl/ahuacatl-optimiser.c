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

static TRI_aql_node_t* ProcessNode (TRI_aql_statement_walker_t* const, 
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
/// @brief return the current optimiser scope
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_optimiser_scope_t* CurrentScope (const TRI_vector_pointer_t* const scopes) {
  size_t n = scopes->_length;

  assert(n > 0);

  return TRI_AtVectorPointer(scopes, n - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add some access information to a for node
////////////////////////////////////////////////////////////////////////////////

static void PatchForNode (TRI_aql_context_t* const context, 
                          TRI_aql_node_t* const node,
                          TRI_aql_field_access_t* fieldAccess) {
  TRI_vector_pointer_t* previous;

  if (node == NULL || fieldAccess == NULL) {
    return;
  }

  assert(node->_type == TRI_AQL_NODE_FOR);

  previous = (TRI_vector_pointer_t*) node->_value._value._data; // might be NULL

  node->_value._value._data = (void*) TRI_AddAccessAql(context, previous, fieldAccess);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief backwards-patch for loops with the range access data we found
////////////////////////////////////////////////////////////////////////////////

static void PatchForLoops (TRI_aql_context_t* const context) {
  TRI_aql_optimiser_scope_t* currentScope = CurrentScope(&context->_optimiser._scopes);
  size_t n;

  if (!currentScope->_ranges) {
    return;
  }

  n = context->_optimiser._scopes._length;
  while (n-- > 0) {
    TRI_aql_optimiser_scope_t* scope = (TRI_aql_optimiser_scope_t*) TRI_AtVectorPointer(&context->_optimiser._scopes, n);
    char* prefix;
    size_t j, len;

    // reached the top level
    if (scope->_type == TRI_AQL_SCOPE_MAIN) {
      break;
    }
    
    // we're only interested in for loops
    if (scope->_type != TRI_AQL_SCOPE_FOR && scope->_type != TRI_AQL_SCOPE_FOR_NESTED) {
      continue;
    }

    // irrelevant for loop
    if (!scope->_variableName || !scope->_node) {
      continue;
    } 
    
    if (!scope->_ranges) {
      continue;
    } 
    
    // we found a for loop, inspect it
    prefix = TRI_Concatenate2String(scope->_variableName, ".");

    if (!prefix) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }
 
    // iterate over all possible field accesses we found in this scope
    len = scope->_ranges->_length;
    for (j = 0; j < len; ++j) {
      TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(scope->_ranges, j);

      // check if the range's variable name is the same as the for variable's name
      if (!TRI_IsPrefixString(fieldAccess->_fullName, prefix)) { 
        // names don't match
        continue;
      }

      // names match

      // merge the field access found into the already existing field accesses for the node
      PatchForNode(context, scope->_node, fieldAccess); 
      break;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);
    // TODO: we could bubble up some of the filters to higher level loops, but
    // we would need to find out which ones can safely be moved there first
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an optimiser scope
////////////////////////////////////////////////////////////////////////////////

static void FreeScope (TRI_aql_optimiser_scope_t* const scope) {
  if (scope->_variableName) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, scope->_variableName);
  }

  if (scope->_ranges) {
    TRI_FreeAccessesAql(scope->_ranges);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start an optimiser scope
////////////////////////////////////////////////////////////////////////////////

static void StartScope (TRI_aql_context_t* const context, 
                        const TRI_aql_scope_e requestedType,
                        const TRI_aql_node_t* const node,
                        const char* const variableName) {
  TRI_aql_optimiser_scope_t* scope; 
  TRI_aql_scope_e type = requestedType;

  if (requestedType == TRI_AQL_SCOPE_FOR) {
    TRI_aql_scope_e previousType = CurrentScope(&context->_optimiser._scopes)->_type;

    if (previousType == TRI_AQL_SCOPE_FOR || previousType == TRI_AQL_SCOPE_FOR_NESTED) {
      type = TRI_AQL_SCOPE_FOR_NESTED;
    }
  }

  scope = (TRI_aql_optimiser_scope_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_optimiser_scope_t), false);
  if (scope == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return;
  }

  scope->_type = type;
  scope->_ranges = NULL;
  scope->_node = NULL;
  scope->_variableName = NULL;

  if (variableName != NULL) {
    scope->_variableName = TRI_DuplicateString(variableName);
    if (scope->_variableName == NULL) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }
  }
  
  if (type == TRI_AQL_SCOPE_FOR || type == TRI_AQL_SCOPE_FOR_NESTED) {
    // store the node pointer for later for loop patching
    scope->_node = (TRI_aql_node_t*) node;
  }

  if (context->_optimiser._scopes._length > 0) {
    // copy ranges of parent scope into current one
    scope->_ranges = TRI_CloneAccessesAql(context, CurrentScope(&context->_optimiser._scopes)->_ranges);
  }
  
  // finally, add the new scope 
  TRI_PushBackVectorPointer(&context->_optimiser._scopes, scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end an optimiser scope
////////////////////////////////////////////////////////////////////////////////

static void EndScope (TRI_aql_context_t* const context, const bool isReturn) {
  TRI_aql_optimiser_scope_t* scope = CurrentScope(&context->_optimiser._scopes);
    
  if (isReturn && (scope->_type == TRI_AQL_SCOPE_MAIN || scope->_type == TRI_AQL_SCOPE_SUBQUERY)) {
    // dont't close these scopes with a return as they are closed by other functions
    return;
  }

  // we are closing at least one scope
  while (true) {
    TRI_aql_scope_e type = scope->_type;

    FreeScope(scope);

    TRI_RemoveVectorPointer(&context->_optimiser._scopes, context->_optimiser._scopes._length - 1);

    // break if we reached the top level for loop
    if (type != TRI_AQL_SCOPE_FOR_NESTED) {
      break;
    }
  
    // next iteration
    scope = CurrentScope(&context->_optimiser._scopes);
  }
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

static TRI_aql_node_t* OptimiseSort (TRI_aql_context_t* const context,
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
/// @brief optimise a filter expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFilter (TRI_aql_context_t* const context,
                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_optimiser_scope_t* scope = CurrentScope(&context->_optimiser._scopes);
 
TRY_LOOP:  
  if (!expression) {
    return node;
  }
  
  if (!TRI_IsConstantValueNodeAql(expression)) {
    TRI_vector_pointer_t* oldRanges;
    TRI_vector_pointer_t* newRanges;
    bool changed = false;

    oldRanges = scope->_ranges;
    newRanges = TRI_OptimiseRangesAql(context, expression, &changed, oldRanges);
    
    if (newRanges) {
      if (oldRanges) {
        TRI_FreeAccessesAql(oldRanges);
      }
      scope->_ranges = newRanges;
    }
    
    if (changed) {
      // expression code was changed, re-optimise it
      node->_members._buffer[0] = ProcessNode((void*) context, expression);
      expression = TRI_AQL_NODE_MEMBER(node, 0);

      // something changed, now try again
      goto TRY_LOOP;
    }

    return node;
  }

  if (TRI_GetBooleanNodeValueAql(expression)) {
    // filter expression is always true => remove it
    TRI_AQL_LOG("optimised away constant (true) filter");

    return TRI_GetDummyNopNodeAql();
  }
  else {
    // filter expression is always false => patch surrounding scope
    TRI_AQL_LOG("optimised away scope"); 

    return TRI_GetDummyReturnEmptyNodeAql();
    /* TODO: validate this!!
    if (scope->_node) {
      TRI_aql_field_access_t* impossible;

      impossible = TRI_CreateImpossibleAccessAql(context);
      if (impossible == NULL) {
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return node;
      } 

      PatchForNode(context, scope->_node, impossible);
      TRI_FreeAccessAql(impossible);
      
      TRI_AQL_LOG("optimised away scope"); 
    }
    */
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a reference expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseReference (TRI_aql_context_t* const context,
                                          TRI_aql_node_t* node) {
  // TODO: find variable in current symbol table
  // follow references until variable declaration is found
  // if variable value at source is constant, copy the constant into here as well
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
/// @brief optimise nodes recursively
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseNode (TRI_aql_context_t* const context, 
                                     TRI_aql_node_t* node) {
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
      return OptimiseReference(context, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseStatement (TRI_aql_context_t* const context, 
                                          TRI_aql_node_t* node) {
  assert(node);

  // node optimisations
  switch (node->_type) {
    case TRI_AQL_NODE_SORT:
      return OptimiseSort(context, node);
    case TRI_AQL_NODE_FILTER:
      return OptimiseFilter(context, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a node recursively
///
/// this is the callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessNode (TRI_aql_statement_walker_t* const walker,
                                    TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) walker->_data;

  return OptimiseNode(context, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a statement
///
/// this is the callback function used by the statement walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ProcessStatement (TRI_aql_statement_walker_t* const walker,
                                         TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) walker->_data;
  TRI_aql_node_t* result = node;

  if (node) {
    // scope handling
    if (node->_type == TRI_AQL_NODE_FOR) {
      TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);

      StartScope(context, TRI_AQL_SCOPE_FOR, node, TRI_AQL_NODE_STRING(nameNode));
    }

    result = OptimiseStatement(context, node);

    // scope handling
    if (node->_type == TRI_AQL_NODE_RETURN) {
      PatchForLoops(context);
      EndScope(context, true);
    }
  }

  return result;
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
/// @brief optimise the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseAql (TRI_aql_context_t* const context) {
  TRI_aql_statement_walker_t* walker;

  walker = TRI_CreateStatementWalkerAql((void*) context, 
                                        true,
                                        &ProcessNode, 
                                        NULL, 
                                        &ProcessStatement);
  if (walker == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }

  StartScope(context, TRI_AQL_SCOPE_MAIN, NULL, NULL);

  TRI_WalkStatementsAql(walker, context->_statements); 
  
  EndScope(context, false);

  TRI_FreeStatementWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
