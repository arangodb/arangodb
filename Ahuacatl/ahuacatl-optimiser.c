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
#include "Ahuacatl/ahuacatl-codegen-js.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-functions.h"

#include "V8/v8-execution.h"

#undef RANGE_OPTIMIZER  

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (void*, TRI_aql_node_t*);

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
    
    // we found a for loop, inspect it
    prefix = TRI_Concatenate2String(scope->_variableName, ".");

    if (!prefix) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }

    len = scope->_ranges->_length;
    for (j = 0; j < len; ++j) {
      TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(scope->_ranges, j);
      TRI_aql_field_access_t* previous;
      int compareResult;

      // check if the range's variable name is the same as the for variable's name
      if (!TRI_IsPrefixString(fieldAccess->_fieldName, prefix)) { 
        // names don't match
        continue;
      }

      // names match

      // check if current or previous range are better
      previous = (TRI_aql_field_access_t*) scope->_node->_value._value._data; 
      compareResult = TRI_PickAccessAql(previous, fieldAccess);
      
      if (compareResult == 1) {
        // clone access and copy it into node
        if (previous) {
          TRI_FreeAccessAql(previous);
        }
        scope->_node->_value._value._data = (void*) TRI_CloneAccessAql(context, fieldAccess);
      }
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, prefix);
    // TODO: we could bubble up some of the filters to higher level loops, but
    // we would need to find out which ones can safely be moved there first
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start an optimiser scope
////////////////////////////////////////////////////////////////////////////////

static void StartScope (TRI_aql_context_t* const context, 
                        const TRI_aql_codegen_scope_e requestedType,
                        const TRI_aql_node_t* const node,
                        const char* const variableName) {
  TRI_aql_optimiser_scope_t* scope; 
  TRI_aql_codegen_scope_e type = requestedType;

  if (requestedType == TRI_AQL_SCOPE_FOR) {
    TRI_aql_codegen_scope_e previousType = CurrentScope(&context->_optimiser._scopes)->_type;

    if (previousType == TRI_AQL_SCOPE_FOR || previousType == TRI_AQL_SCOPE_FOR_NESTED) {
      type = TRI_AQL_SCOPE_FOR_NESTED;
    }
  }

  scope = (TRI_aql_optimiser_scope_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_optimiser_scope_t), false);
  if (!scope) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return;
  }

  scope->_type = type;
  scope->_ranges = NULL;
  scope->_node = NULL;
  scope->_variableName = NULL;

  if (variableName != NULL) {
    scope->_variableName = TRI_DuplicateString(variableName);
    if (!scope->_variableName) {
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
    scope->_ranges = TRI_CloneRangesAql(context, CurrentScope(&context->_optimiser._scopes)->_ranges);
  }
   
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
    TRI_aql_codegen_scope_e type = scope->_type;
    
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
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, NULL);
    return NULL;
  }

  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  LOG_TRACE("optimised function call");

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

    LOG_TRACE("optimised away sort element");
  }

  if (n == 0) {
    // no members left => sort removed
    LOG_TRACE("optimised away sort");

    return NULL;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a filter expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFilter (TRI_aql_context_t* const context,
                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 0);
   
  if (!expression) {
    return node;
  }
  
  if (!TRI_IsConstantValueNodeAql(expression)) {
#ifdef RANGE_OPTIMIZER  
    TRI_aql_optimiser_scope_t* scope = CurrentScope(&context->_optimiser._scopes);
    TRI_vector_pointer_t* ranges;
    bool changed = false;

    ranges = TRI_OptimiseRangesAql(context, expression, &changed, scope->_ranges);
    
    if (ranges) {
      scope->_ranges = ranges;
    }
    
    if (changed) {
      // expression code was changed, re-optimise it
      node->_members._buffer[0] = ModifyNode((void*) context, expression);
      expression = TRI_AQL_NODE_MEMBER(node, 0);

      // try again if it is constant
      if (TRI_IsConstantValueNodeAql(expression)) {
        if (TRI_GetBooleanNodeValueAql(expression)) {
          // filter expression is always true => remove it
          LOG_TRACE("optimised away constant filter");

          return NULL;
        }
      }
    }
#endif
    return node;
  }

  if (TRI_GetBooleanNodeValueAql(expression)) {
    // filter expression is always true => remove it
    LOG_TRACE("optimised away constant filter");

    return NULL;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryArithmeticOperation (TRI_aql_context_t* const context,
                                                         TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsNumericValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  assert(node->_type == AQL_NODE_OPERATOR_UNARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_UNARY_MINUS);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_PLUS) {
    // + number => number
    node = operand;
  }
  else if (node->_type == AQL_NODE_OPERATOR_UNARY_MINUS) {
    // - number => eval!
    node = TRI_CreateNodeValueDoubleAql(context, - TRI_GetNumericNodeValueAql(operand));
    if (!node) {
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

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsBooleanValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_UNARY_NOT);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_NOT) {
    // ! bool => evaluate
    node = TRI_CreateNodeValueBoolAql(context, ! TRI_GetBooleanNodeValueAql(operand));
    if (!node) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }

    LOG_TRACE("optimised away unary logical operation");
  }

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
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (isEligibleRhs && !TRI_IsBooleanValueNodeAql(rhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }

  lhsValue = TRI_GetBooleanNodeValueAql(lhs);
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_AND ||
         node->_type == AQL_NODE_OPERATOR_BINARY_OR);

  LOG_TRACE("optimised away binary logical operation");

  if (node->_type == AQL_NODE_OPERATOR_BINARY_AND) {
    if (lhsValue) {
      // if (true && rhs) => rhs
      return rhs;
    }

    // if (false && rhs) => false
    return lhs;
  } 
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_OR) {
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
  
  if (node->_type == AQL_NODE_OPERATOR_BINARY_EQ) {
    func = "EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_NE) {
    func = "UNEQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_GT) {
    func = "GREATER";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_GE) {
    func = "GREATER_EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_LT) {
    func = "LESS";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_LE) {
    func = "LESS_EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_IN) {
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
    
  LOG_TRACE("optimised away binary relational operation");

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
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  
  if (isEligibleRhs && !TRI_IsNumericValueNodeAql(rhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }
  
  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MINUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_TIMES ||
         node->_type == AQL_NODE_OPERATOR_BINARY_DIV ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MOD);

  if (node->_type == AQL_NODE_OPERATOR_BINARY_PLUS) {
    value = TRI_GetNumericNodeValueAql(lhs) + TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MINUS) {
    value = TRI_GetNumericNodeValueAql(lhs) - TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_TIMES) {
    value = TRI_GetNumericNodeValueAql(lhs) * TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_DIV) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISON_BY_ZERO, NULL);
      return node;
    }
    value = TRI_GetNumericNodeValueAql(lhs) / TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MOD) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISON_BY_ZERO, NULL);
      return node;
    }
    value = fmod(TRI_GetNumericNodeValueAql(lhs), TRI_GetNumericNodeValueAql(rhs));
  }
  
  node = TRI_CreateNodeValueDoubleAql(context, value);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  LOG_TRACE("optimised away binary arithmetic operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise nodes recursively
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseNode (TRI_aql_context_t* const context, 
                                     TRI_aql_node_t* node) {
  assert(node);

  // node optimisations
  switch (node->_type) {
    case AQL_NODE_OPERATOR_UNARY_PLUS:
    case AQL_NODE_OPERATOR_UNARY_MINUS:
      return OptimiseUnaryArithmeticOperation(context, node);
    case AQL_NODE_OPERATOR_UNARY_NOT:
      return OptimiseUnaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_AND:
    case AQL_NODE_OPERATOR_BINARY_OR:
      return OptimiseBinaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_EQ:
    case AQL_NODE_OPERATOR_BINARY_NE:
    case AQL_NODE_OPERATOR_BINARY_LT:
    case AQL_NODE_OPERATOR_BINARY_LE:
    case AQL_NODE_OPERATOR_BINARY_GT:
    case AQL_NODE_OPERATOR_BINARY_GE:
    case AQL_NODE_OPERATOR_BINARY_IN:
      return OptimiseBinaryRelationalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_PLUS:
    case AQL_NODE_OPERATOR_BINARY_MINUS:
    case AQL_NODE_OPERATOR_BINARY_TIMES:
    case AQL_NODE_OPERATOR_BINARY_DIV:
    case AQL_NODE_OPERATOR_BINARY_MOD:
      return OptimiseBinaryArithmeticOperation(context, node);
    case AQL_NODE_SORT:
      return OptimiseSort(context, node);
    case AQL_NODE_FILTER:
      return OptimiseFilter(context, node);
    case AQL_NODE_FCALL:
      return OptimiseFcall(context, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise nodes recursively
///
/// this is the callback function used by the tree walker
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (void* data, TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) data;
  TRI_aql_node_t* result = node;

#ifdef RANGE_OPTIMIZER  
  if (node) {
    // scope handling
    if (node->_type == AQL_NODE_FOR) {
      TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);

      StartScope(context, TRI_AQL_SCOPE_FOR, node, TRI_AQL_NODE_STRING(nameNode));
    }
    else if (node->_type == AQL_NODE_SUBQUERY) {
      StartScope(context, TRI_AQL_SCOPE_FUNCTION, NULL, NULL);
    }
#endif

    result = OptimiseNode(context, node);

#ifdef RANGE_OPTIMIZER  
    // scope handling
    if (node->_type == AQL_NODE_RETURN) {
      PatchForLoops(context);
      EndScope(context, true);
    }  
    else if (node->_type == AQL_NODE_SUBQUERY) {
      EndScope(context, false);
    }
  }
#endif

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
/// @brief optimise the AST
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_OptimiseAql (TRI_aql_context_t* const context,
                                 TRI_aql_node_t* node) {
  TRI_aql_modify_tree_walker_t* walker;
 
  walker = TRI_CreateModifyTreeWalkerAql((void*) context, &ModifyNode);
  if (!walker) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

#ifdef RANGE_OPTIMIZER  
  StartScope(context, TRI_AQL_SCOPE_MAIN, NULL, NULL);
#endif

  node = TRI_ModifyWalkTreeAql(walker, node); 
  
#ifdef RANGE_OPTIMIZER  
  EndScope(context, false);
#endif

  TRI_FreeModifyTreeWalkerAql(walker);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
