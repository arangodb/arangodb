////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser nodes
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

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                                                \
  TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);                \
  return NULL;

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro to create a new node or fail in case of OOM
////////////////////////////////////////////////////////////////////////////////

#define CREATE_NODE(type)                                                        \
  TRI_aql_node_t* node = (TRI_aql_node_t*) TRI_Allocate(sizeof(TRI_aql_node_t)); \
  if (!node) {                                                                   \
    ABORT_OOM                                                                    \
  }                                                                              \
                                                                                 \
  assert(context);                                                               \
  InitNode(context, node, type); 
  
////////////////////////////////////////////////////////////////////////////////
/// @brief add a sub node to a node
////////////////////////////////////////////////////////////////////////////////

#define SUB_NODE_MANDATORY(subNode)                                              \
  if (!subNode) {                                                                \
    ABORT_OOM                                                                    \
  }                                                                              \
                                                                                 \
  TRI_PushBackVectorPointer(&node->_subNodes, (void*) subNode);

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

static inline void InitNode (TRI_aql_context_t* const context,
                             TRI_aql_node_t* const node, 
                             const TRI_aql_node_type_e type) {
  node->_type = type;
  node->_next = NULL;
 
  TRI_InitVectorPointer(&node->_subNodes);
  TRI_RegisterNodeContextAql(context, node);
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
/// @brief create an AST main node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeMainAql (TRI_aql_context_t* const context) {
  CREATE_NODE(AQL_NODE_MAIN)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeForAql (TRI_aql_context_t* const context,
                                      const char* const name,
                                      const TRI_aql_node_t* const expression) {
  CREATE_NODE(AQL_NODE_FOR)

  if (!name) {
    ABORT_OOM
  }

  if (!TRI_IsValidVariableNameAql(name)) { 
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_VARIABLE_NAME_INVALID, name); 
    return NULL;
  }
  
  {
    TRI_aql_node_t* variable = TRI_CreateNodeVariableAql(context, name);
    SUB_NODE_MANDATORY(variable);
    SUB_NODE_MANDATORY(expression)
  }

  return node;
}

/*
////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLetAql (TRI_aql_context_t* const context,
                                      const char* const name,
                                      const TRI_aql_node_t* const expression) {
  TRI_aql_node_let_t* node;
  TRI_aql_node_t* variableNode;
  
  assert(context);
  
  if (!name || !expression) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_let_t*) TRI_Allocate(sizeof(TRI_aql_node_let_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_LET);

  variableNode = TRI_CreateNodeVariableAql(context, name);
  if (!variableNode) {
    return NULL;
  }

  node->_variable = variableNode;
  node->_expression = (TRI_aql_node_t*) expression;

  return (TRI_aql_node_t*) node;
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST filter node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeFilterAql (TRI_aql_context_t* const context,
                                         const TRI_aql_node_t* const expression) {
  CREATE_NODE(AQL_NODE_FILTER) 
  
  SUB_NODE_MANDATORY(expression)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST return node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReturnAql (TRI_aql_context_t* const context,
                                         const TRI_aql_node_t* const expression) {
  CREATE_NODE(AQL_NODE_RETURN) 
  
  SUB_NODE_MANDATORY(expression)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeCollectAql (TRI_aql_context_t* const context,
                                          const TRI_aql_node_t* const list,
                                          const char* const name) {
  CREATE_NODE(AQL_NODE_COLLECT)

  SUB_NODE_MANDATORY(list)
  if (name) {
    TRI_aql_node_t* variable = TRI_CreateNodeVariableAql(context, name);
    SUB_NODE_MANDATORY(variable)
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortAql (TRI_aql_context_t* const context,
                                       const TRI_aql_node_t* const list) {
  CREATE_NODE(AQL_NODE_SORT)
  
  SUB_NODE_MANDATORY(list)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortElementAql (TRI_aql_context_t* const context,
                                              const TRI_aql_node_t* const expression, 
                                              const bool ascending) {
  CREATE_NODE(AQL_NODE_SORT_ELEMENT)

  SUB_NODE_MANDATORY(expression)
  TRI_AQL_NODE_BOOL(node) = ascending;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST limit node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLimitAql (TRI_aql_context_t* const context,
                                        const TRI_aql_node_t* const offset, 
                                        const TRI_aql_node_t* const count) {
  CREATE_NODE(AQL_NODE_LIMIT)

  SUB_NODE_MANDATORY(offset)
  SUB_NODE_MANDATORY(count)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST assign node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAssignAql (TRI_aql_context_t* const context,
                                         const char* const name,
                                         const TRI_aql_node_t* const expression) {
  CREATE_NODE(AQL_NODE_ASSIGN)
 
  {
    TRI_aql_node_t* variable = TRI_CreateNodeVariableAql(context, name);
    SUB_NODE_MANDATORY(variable)
    SUB_NODE_MANDATORY(expression)
  }
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeVariableAql (TRI_aql_context_t* const context,
                                           const char* const name) {
  CREATE_NODE(AQL_NODE_VARIABLE)
   
  if (!name) {
    ABORT_OOM
  }
  
  if (!TRI_AddVariableContextAql(context, name)) {
    // duplicate variable name 
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_VARIABLE_REDECLARED, name); 
    return NULL;
  }
  
  TRI_AQL_NODE_STRING(node) = (char*) name;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeCollectionAql (TRI_aql_context_t* const context,
                                             const char* const name) {
  CREATE_NODE(AQL_NODE_COLLECTION)

  if (!name) {
    ABORT_OOM
  }
  
  TRI_AQL_NODE_STRING(node) = (char*) name;

  // duplicates are not a problem here, we simply ignore them
  TRI_InsertKeyAssociativePointer(&context->_collectionNames, name, (void*) name, false);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReferenceAql (TRI_aql_context_t* const context,
                                            const char* const name) {
  CREATE_NODE(AQL_NODE_REFERENCE)

  if (!name) {
    ABORT_OOM
  }
  
  TRI_AQL_NODE_STRING(node) = (char*) name;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAql (TRI_aql_context_t* const context,
                                            const char* const name) {
  CREATE_NODE(AQL_NODE_ATTRIBUTE)

  if (!name) {
    ABORT_OOM
  }
  
  TRI_AQL_NODE_STRING(node) = (char*) name;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST parameter node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeParameterAql (TRI_aql_context_t* const context,
                                            const char* const name) {
  CREATE_NODE(AQL_NODE_PARAMETER)

  if (!name) {
    ABORT_OOM
  }

  // save name of bind parameter for later
  TRI_InsertKeyAssociativePointer(&context->_parameterNames, name, (void*) name, true);

  TRI_AQL_NODE_STRING(node) = (char*) name;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryPlusAql (TRI_aql_context_t* const context,
                                                    const TRI_aql_node_t* const operand) {
  CREATE_NODE(AQL_NODE_OPERATOR_UNARY_PLUS)
 
  SUB_NODE_MANDATORY(operand)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryMinusAql (TRI_aql_context_t* const context,
                                                     const TRI_aql_node_t* const operand) {
  CREATE_NODE(AQL_NODE_OPERATOR_UNARY_MINUS)
  
  SUB_NODE_MANDATORY(operand)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary not node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryNotAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const operand) {
  CREATE_NODE(AQL_NODE_OPERATOR_UNARY_NOT)
  
  SUB_NODE_MANDATORY(operand)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary and node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryAndAql (TRI_aql_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_AND)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary or node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryOrAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_OR)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary eq node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryEqAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_EQ)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ne node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryNeAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_NE)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary lt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLtAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_LT)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary le node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLeAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_LE)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary gt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGtAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_GT)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ge node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGeAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_GE)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary in node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryInAql (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_IN)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryPlusAql (TRI_aql_context_t* const context,
                                                     const TRI_aql_node_t* const lhs,
                                                     const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_PLUS)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryMinusAql (TRI_aql_context_t* const context,
                                                      const TRI_aql_node_t* const lhs,
                                                      const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_MINUS)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary times node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryTimesAql (TRI_aql_context_t* const context,
                                                      const TRI_aql_node_t* const lhs,
                                                      const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_TIMES)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary div node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryDivAql (TRI_aql_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_DIV)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary mod node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryModAql (TRI_aql_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  CREATE_NODE(AQL_NODE_OPERATOR_BINARY_MOD)
  
  SUB_NODE_MANDATORY(lhs)
  SUB_NODE_MANDATORY(rhs)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST ternary operator node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorTernaryAql (TRI_aql_context_t* const context,
                                                  const TRI_aql_node_t* const condition,
                                                  const TRI_aql_node_t* const truePart,
                                                  const TRI_aql_node_t* const falsePart) {
  CREATE_NODE(AQL_NODE_OPERATOR_TERNARY)
  
  SUB_NODE_MANDATORY(condition)
  SUB_NODE_MANDATORY(truePart)
  SUB_NODE_MANDATORY(falsePart)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST subquery node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSubqueryAql (TRI_aql_context_t* const context,
                                           const TRI_aql_node_t* const query) {
  CREATE_NODE(AQL_NODE_SUBQUERY)
  
  SUB_NODE_MANDATORY(query)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAccessAql (TRI_aql_context_t* const context, 
                                                  const TRI_aql_node_t* const accessed,
                                                  const char* const name) { 
  CREATE_NODE(AQL_NODE_ATTRIBUTE_ACCESS)

  if (!name) {
    ABORT_OOM
  }

  SUB_NODE_MANDATORY(accessed)
  TRI_AQL_NODE_STRING(node) = (char*) name;  

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST index access node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeIndexedAql (TRI_aql_context_t* const context,
                                          const TRI_aql_node_t* const accessed, 
                                          const TRI_aql_node_t* const indexValue) {
  CREATE_NODE(AQL_NODE_INDEXED)

  SUB_NODE_MANDATORY(accessed)
  SUB_NODE_MANDATORY(indexValue)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expand node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeExpandAql (TRI_aql_context_t* const context,
                                         const TRI_aql_node_t* const expanded,
                                         const TRI_aql_node_t* const expansion) {
  CREATE_NODE(AQL_NODE_EXPAND)

  SUB_NODE_MANDATORY(expanded)
  SUB_NODE_MANDATORY(expansion)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST null value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueNullAql (TRI_aql_context_t* const context) {
  CREATE_NODE(AQL_NODE_VALUE)

  TRI_AQL_NODE_TYPE(node) = AQL_TYPE_NULL;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueIntAql (TRI_aql_context_t* const context,
                                           const int64_t value) { 
  CREATE_NODE(AQL_NODE_VALUE)

  TRI_AQL_NODE_TYPE(node) = AQL_TYPE_INT;
  TRI_AQL_NODE_INT(node) = value;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST double value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueDoubleAql (TRI_aql_context_t* const context,
                                              const double value) { 
  CREATE_NODE(AQL_NODE_VALUE)

  TRI_AQL_NODE_TYPE(node) = AQL_TYPE_DOUBLE;
  TRI_AQL_NODE_DOUBLE(node) = value;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueBoolAql (TRI_aql_context_t* const context,
                                            const bool value) { 
  CREATE_NODE(AQL_NODE_VALUE)

  TRI_AQL_NODE_TYPE(node) = AQL_TYPE_BOOL;
  TRI_AQL_NODE_BOOL(node) = value;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST string value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueStringAql (TRI_aql_context_t* const context,
                                              const char* const value) { 
  CREATE_NODE(AQL_NODE_VALUE)

  if (!value) {
    ABORT_OOM
  }
  
  TRI_AQL_NODE_TYPE(node) = AQL_TYPE_STRING;
  TRI_AQL_NODE_STRING(node) = (char*) value;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeListAql (TRI_aql_context_t* const context) {
  CREATE_NODE(AQL_NODE_LIST)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayAql (TRI_aql_context_t* const context) {
  CREATE_NODE(AQL_NODE_ARRAY)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayElementAql (TRI_aql_context_t* const context,
                                               const char* const name,
                                               const TRI_aql_node_t* const value) {
  CREATE_NODE(AQL_NODE_ARRAY_ELEMENT)

  if (!name) {
    ABORT_OOM
  }

  TRI_AQL_NODE_STRING(node) = (char*) name;
  SUB_NODE_MANDATORY(value)

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST function call node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeFcallAql (TRI_aql_context_t* const context,
                                        const char* const name,
                                        const TRI_aql_node_t* const parameters) {
  CREATE_NODE(AQL_NODE_FCALL)

  if (!name) {
    ABORT_OOM
  }
  
  {
    TRI_aql_function_t* function;
    TRI_associative_pointer_t* functions;
    char* upperName;

    assert(context->_vocbase);
    functions = context->_vocbase->_functions;
    assert(functions);

    // normalize the name by upper-casing it
    upperName = TRI_UpperAsciiString(name);
    if (!upperName) {
      ABORT_OOM
    }

    function = (TRI_aql_function_t*) TRI_LookupByKeyAssociativePointer(functions, (void*) upperName);
    TRI_Free(upperName);

    if (!function) {
      // function name is unknown
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN, name);
      return NULL;
    }

    SUB_NODE_MANDATORY(parameters);
    TRI_AQL_NODE_STRING(node) = function->_internalName;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushListAql (TRI_aql_context_t* const context, 
                      const TRI_aql_node_t* const value) {
  TRI_aql_node_t* node = TRI_PeekStackAql(context);

  assert(node);
 
  SUB_NODE_MANDATORY(value); 

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of an array
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushArrayAql (TRI_aql_context_t* const context, 
                       const char* const name,
                       const TRI_aql_node_t* const value) {
  TRI_aql_node_t* node = TRI_PeekStackAql(context);
  TRI_aql_node_t* element;

  assert(node);
  
  element = TRI_CreateNodeArrayElementAql(context, name, value);
  if (!element) {
    ABORT_OOM
  }

  SUB_NODE_MANDATORY(element);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a constant
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsConstantValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);

  if (node->_type != AQL_NODE_VALUE && 
      node->_type != AQL_NODE_LIST &&
      node->_type != AQL_NODE_ARRAY) {
    return false;
  }

  if (node->_type == AQL_NODE_LIST || node->_type == AQL_NODE_ARRAY) {
    size_t i;
    size_t n = node->_subNodes._length;

    for (i = 0; i < n; ++i) {
      TRI_aql_node_t* subNode = (TRI_aql_node_t*) node->_subNodes._buffer[i];

      if (!TRI_IsConstantValueNodeAql(subNode)) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is numeric
////////////////////////////////////////////////////////////////////////////////

inline bool TRI_IsNumericValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);

  if (node->_type != AQL_NODE_VALUE) {
    return false;
  }

  return (node->_value._type == AQL_TYPE_INT ||
          node->_value._type == AQL_TYPE_DOUBLE); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is boolean
////////////////////////////////////////////////////////////////////////////////

inline bool TRI_IsBooleanValueNodeAql (const TRI_aql_node_t* const node) {
  assert(node);
  
  return (node->_type == AQL_NODE_VALUE && node->_value._type == AQL_TYPE_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the boolean value of a node
////////////////////////////////////////////////////////////////////////////////

inline bool TRI_GetBooleanNodeValueAql (const TRI_aql_node_t* const node) {
  assert(node);
  assert(node->_type == AQL_NODE_VALUE);
    
  return TRI_AQL_NODE_BOOL(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of a node
////////////////////////////////////////////////////////////////////////////////

double TRI_GetNumericNodeValueAql (const TRI_aql_node_t* const node) {
  assert(node);
  assert(node->_type == AQL_NODE_VALUE);

  if (node->_value._type == AQL_TYPE_INT) {
    return (double) TRI_AQL_NODE_INT(node);
  }
  if (node->_value._type == AQL_TYPE_DOUBLE) {
    return TRI_AQL_NODE_DOUBLE(node);
  }
  if (node->_value._type == AQL_TYPE_BOOL) {
    return TRI_AQL_NODE_BOOL(node) ? 1.0 : 0.0;
  }

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
