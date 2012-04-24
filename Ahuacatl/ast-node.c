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

#include "Ahuacatl/ast-node.h"
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

#define ABORT_OOM \
  TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL); \
  return NULL;

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

static inline void InitNode (TRI_aql_parse_context_t* const context,
                             TRI_aql_node_t* const node, 
                             const TRI_aql_node_type_e type) {
  node->_type = type;
  node->_next = NULL;
  node->free = NULL;
 
  TRI_RegisterNodeParseContextAql(context, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a list node
////////////////////////////////////////////////////////////////////////////////

static void FreeNodeList (TRI_aql_node_t* const node) {
  TRI_aql_node_list_t* _node = (TRI_aql_node_list_t*) node;

  TRI_DestroyVectorPointer(&_node->_values);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an array node
////////////////////////////////////////////////////////////////////////////////

static void FreeNodeArray (TRI_aql_node_t* const node) {
  TRI_aql_node_array_t* _node = (TRI_aql_node_array_t*) node;

  TRI_DestroyAssociativePointer(&_node->_values);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hash array element 
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashArrayElement (TRI_associative_pointer_t* array, 
                                  void const* element) {
  TRI_aql_node_array_element_t* e = (TRI_aql_node_array_element_t*) element;

  return TRI_FnvHashString(e->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine array element equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualArrayElement (TRI_associative_pointer_t* array, 
                               void const* key, 
                               void const* element) {
  TRI_aql_node_array_element_t* e = (TRI_aql_node_array_element_t*) element;

  return TRI_EqualString(key, e->_name);
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
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeForAql (TRI_aql_parse_context_t* const context,
                                      const char* const name,
                                      const TRI_aql_node_t* const expression) {
  TRI_aql_node_for_t* node;
  TRI_aql_node_t* variableNode;
  
  assert(context);
  
  if (!name || !expression) {
    ABORT_OOM
  }

  if (!TRI_IsValidVariableNameAql(name)) { 
    TRI_SetErrorAql(context, TRI_ERROR_QUERY_VARIABLE_NAME_INVALID, name); 
    return NULL;
  }
  
  node = (TRI_aql_node_for_t*) TRI_Allocate(sizeof(TRI_aql_node_for_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_FOR);

  variableNode = TRI_CreateNodeVariableAql(context, name);
  if (!variableNode) {
    return NULL;
  }

  node->_variable = variableNode;
  node->_expression = (TRI_aql_node_t*) expression;

  return (TRI_aql_node_t*) node;
}
/*
////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLetAql (TRI_aql_parse_context_t* const context,
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

TRI_aql_node_t* TRI_CreateNodeFilterAql (TRI_aql_parse_context_t* const context,
                                         const TRI_aql_node_t* const expression) {
  TRI_aql_node_filter_t* node;
  
  assert(context);
  
  if (!expression) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_filter_t*) TRI_Allocate(sizeof(TRI_aql_node_filter_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_FILTER);

  node->_expression = (TRI_aql_node_t*) expression;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST return node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReturnAql (TRI_aql_parse_context_t* const context,
                                         const TRI_aql_node_t* const expression) {
  TRI_aql_node_return_t* node;
  
  assert(context);
  
  if (!expression) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_return_t*) TRI_Allocate(sizeof(TRI_aql_node_return_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_RETURN);

  node->_expression = (TRI_aql_node_t*) expression;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeCollectAql (TRI_aql_parse_context_t* const context,
                                          const TRI_aql_node_t* const list,
                                          const char* const name) {
  TRI_aql_node_collect_t* node;
  TRI_aql_node_t* variableNode;
  
  assert(context);
  
  if (!list) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_collect_t*) TRI_Allocate(sizeof(TRI_aql_node_collect_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_COLLECT);
  
  node->_list = (TRI_aql_node_t*) list;

  node->_into = NULL;
  if (name) {
    variableNode = TRI_CreateNodeVariableAql(context, name);
    if (!variableNode) {
      return NULL;
    }
    node->_into = variableNode;
  }

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortAql (TRI_aql_parse_context_t* const context,
                                       const TRI_aql_node_t* const list) {
  TRI_aql_node_sort_t* node;
  
  assert(context);
  
  if (!list) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_sort_t*) TRI_Allocate(sizeof(TRI_aql_node_sort_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_SORT);

  node->_list = (TRI_aql_node_t*) list;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortElementAql (TRI_aql_parse_context_t* const context,
                                              const TRI_aql_node_t* const expression, 
                                              const bool ascending) {
  TRI_aql_node_sort_element_t* node;
  
  assert(context);
  
  if (!expression) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_sort_element_t*) TRI_Allocate(sizeof(TRI_aql_node_sort_element_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_SORT_ELEMENT);
  
  node->_expression = (TRI_aql_node_t*) expression;
  node->_ascending = ascending;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST limit node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLimitAql (TRI_aql_parse_context_t* const context,
                                        const int64_t offset, 
                                        const int64_t count) {
  TRI_aql_node_limit_t* node;
  
  assert(context);
  
  node = (TRI_aql_node_limit_t*) TRI_Allocate(sizeof(TRI_aql_node_limit_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_LIMIT);
  
  node->_offset = offset;
  node->_count = count;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST assign node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAssignAql (TRI_aql_parse_context_t* const context,
                                         const char* const name,
                                         const TRI_aql_node_t* const expression) {
  TRI_aql_node_assign_t* node;
  TRI_aql_node_t* variableNode;
  
  assert(context);
  
  if (!name || !expression) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_assign_t*) TRI_Allocate(sizeof(TRI_aql_node_assign_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_ASSIGN);

  variableNode = TRI_CreateNodeVariableAql(context, name);
  if (!variableNode) {
    return NULL;
  }

  node->_variable = variableNode;
  node->_expression = (TRI_aql_node_t*) expression;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeVariableAql (TRI_aql_parse_context_t* const context,
                                           const char* const name) {
  TRI_aql_node_variable_t* node;
  
  assert(context);

  if (!name) {
    ABORT_OOM
  }
  
  if (!TRI_AddVariableParseContextAql(context, name)) {
    // duplicate variable name 
    TRI_SetErrorAql(context, TRI_ERROR_QUERY_VARIABLE_REDECLARED, name); 
    return NULL;
  }
  
  node = (TRI_aql_node_variable_t*) TRI_Allocate(sizeof(TRI_aql_node_variable_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VARIABLE);
  
  node->_name = (char*) name;

  if (!node->_name) {
    ABORT_OOM
  }

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReferenceAql (TRI_aql_parse_context_t* const context,
                                            const char* const name,
                                            const bool isCollection) {
  TRI_aql_node_reference_t* node;
  
  assert(context);

  if (!name) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_reference_t*) TRI_Allocate(sizeof(TRI_aql_node_reference_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_REFERENCE);
  
  node->_name = (char*) name;
  node->_isCollection = isCollection;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAql (TRI_aql_parse_context_t* const context,
                                            const char* const name) {
  TRI_aql_node_attribute_t* node;
  
  assert(context);

  if (!name) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_attribute_t*) TRI_Allocate(sizeof(TRI_aql_node_attribute_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_ATTRIBUTE);
  
  node->_name = (char*) name;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST parameter node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeParameterAql (TRI_aql_parse_context_t* const context,
                                            const char* const name) {
  TRI_aql_node_parameter_t* node;
  
  assert(context);

  if (!name) {
    ABORT_OOM
  }

  // save name of bind parameter for later
  TRI_InsertKeyAssociativePointer(&context->_parameterNames, name, (void*) name, true);

  node = (TRI_aql_node_parameter_t*) TRI_Allocate(sizeof(TRI_aql_node_parameter_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_PARAMETER);
  
  node->_name = (char*) name;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryPlusAql (TRI_aql_parse_context_t* const context,
                                                    const TRI_aql_node_t* const operand) {
  TRI_aql_node_operator_unary_t* node;
  
  assert(context);

  if (!operand) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_unary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_unary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_UNARY_PLUS);
  
  node->_operand = (TRI_aql_node_t*) operand;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryMinusAql (TRI_aql_parse_context_t* const context,
                                                     const TRI_aql_node_t* const operand) {
  TRI_aql_node_operator_unary_t* node;
  
  assert(context);

  if (!operand) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_unary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_unary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_UNARY_MINUS);
  
  node->_operand = (TRI_aql_node_t*) operand;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary not node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryNotAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const operand) {
  TRI_aql_node_operator_unary_t* node;
  
  assert(context);

  if (!operand) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_unary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_unary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_UNARY_NOT);
  
  node->_operand = (TRI_aql_node_t*) operand;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary and node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryAndAql (TRI_aql_parse_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_AND);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary or node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryOrAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_OR);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary eq node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryEqAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_EQ);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ne node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryNeAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_NE);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary lt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLtAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_LT);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary le node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLeAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_LE);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary gt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGtAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_GT);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ge node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGeAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_GE);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary in node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryInAql (TRI_aql_parse_context_t* const context,
                                                   const TRI_aql_node_t* const lhs,
                                                   const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_IN);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryPlusAql (TRI_aql_parse_context_t* const context,
                                                     const TRI_aql_node_t* const lhs,
                                                     const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_PLUS);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryMinusAql (TRI_aql_parse_context_t* const context,
                                                      const TRI_aql_node_t* const lhs,
                                                      const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_MINUS);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary times node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryTimesAql (TRI_aql_parse_context_t* const context,
                                                      const TRI_aql_node_t* const lhs,
                                                      const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_TIMES);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary div node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryDivAql (TRI_aql_parse_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_DIV);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary mod node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryModAql (TRI_aql_parse_context_t* const context,
                                                    const TRI_aql_node_t* const lhs,
                                                    const TRI_aql_node_t* const rhs) {
  TRI_aql_node_operator_binary_t* node;
  
  assert(context);

  if (!lhs || !rhs) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_binary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_binary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_BINARY_MOD);
  
  node->_lhs = (TRI_aql_node_t*) lhs;
  node->_rhs = (TRI_aql_node_t*) rhs;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST ternary operator node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorTernaryAql (TRI_aql_parse_context_t* const context,
                                                  const TRI_aql_node_t* const condition,
                                                  const TRI_aql_node_t* const truePart,
                                                  const TRI_aql_node_t* const falsePart) {
  TRI_aql_node_operator_ternary_t* node;
  
  assert(context);

  if (!condition || !truePart || !falsePart) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_operator_ternary_t*) TRI_Allocate(sizeof(TRI_aql_node_operator_ternary_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_OPERATOR_TERNARY);
  
  node->_condition = (TRI_aql_node_t*) condition;
  node->_truePart = (TRI_aql_node_t*) truePart;
  node->_falsePart = (TRI_aql_node_t*) falsePart;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST subquery node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSubqueryAql (TRI_aql_parse_context_t* const context,
                                           const TRI_aql_node_t* const query) {
  TRI_aql_node_subquery_t* node;
  
  assert(context);

  if (!query) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_subquery_t*) TRI_Allocate(sizeof(TRI_aql_node_subquery_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_SUBQUERY);
  
  node->_query = (TRI_aql_node_t*) query;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAccessAql (TRI_aql_parse_context_t* const context, 
                                                  const TRI_aql_node_t* const accessed,
                                                  const char* const name) { 
  TRI_aql_node_attribute_access_t* node;
  
  assert(context);

  if (!accessed || !name) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_attribute_access_t*) TRI_Allocate(sizeof(TRI_aql_node_attribute_access_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_ATTRIBUTE_ACCESS);
  
  node->_accessed = (TRI_aql_node_t*) accessed;
  node->_name = (char*) name;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST index access node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeIndexedAql (TRI_aql_parse_context_t* const context,
                                          const TRI_aql_node_t* const accessed, 
                                          const TRI_aql_node_t* const indexValue) {
  TRI_aql_node_indexed_t* node;
  
  assert(context);

  if (!accessed || !indexValue) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_indexed_t*) TRI_Allocate(sizeof(TRI_aql_node_indexed_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_INDEXED);
  
  node->_accessed = (TRI_aql_node_t*) accessed;
  node->_index = (TRI_aql_node_t*) indexValue;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expand node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeExpandAql (TRI_aql_parse_context_t* const context,
                                         const TRI_aql_node_t* const expanded,
                                         const TRI_aql_node_t* const expansion) {
  TRI_aql_node_expand_t* node;
  
  assert(context);

  if (!expanded || !expansion) {
    ABORT_OOM
  }
  
  node = (TRI_aql_node_expand_t*) TRI_Allocate(sizeof(TRI_aql_node_expand_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_EXPAND);
  
  node->_expanded = (TRI_aql_node_t*) expanded;
  node->_expansion = (TRI_aql_node_t*) expansion;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST null value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueNullAql (TRI_aql_parse_context_t* const context) {
  TRI_aql_node_value_t* node;
  
  assert(context);

  node = (TRI_aql_node_value_t*) TRI_Allocate(sizeof(TRI_aql_node_value_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VALUE);

  node->_value._type = AQL_TYPE_NULL;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueIntAql (TRI_aql_parse_context_t* const context,
                                           const int64_t value) { 
  TRI_aql_node_value_t* node;
  
  assert(context);

  node = (TRI_aql_node_value_t*) TRI_Allocate(sizeof(TRI_aql_node_value_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VALUE);

  node->_value._type = AQL_TYPE_INT;
  node->_value._value._int = value;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST double value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueDoubleAql (TRI_aql_parse_context_t* const context,
                                              const double value) { 
  TRI_aql_node_value_t* node;
  
  assert(context);

  node = (TRI_aql_node_value_t*) TRI_Allocate(sizeof(TRI_aql_node_value_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VALUE);

  node->_value._type = AQL_TYPE_DOUBLE;
  node->_value._value._double = value;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueBoolAql (TRI_aql_parse_context_t* const context,
                                            const bool value) { 
  TRI_aql_node_value_t* node;
  
  assert(context);

  node = (TRI_aql_node_value_t*) TRI_Allocate(sizeof(TRI_aql_node_value_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VALUE);

  node->_value._type = AQL_TYPE_BOOL;
  node->_value._value._bool = value;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST string value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueStringAql (TRI_aql_parse_context_t* const context,
                                              const char* const value) { 
  TRI_aql_node_value_t* node;
  
  assert(context);

  if (!value) {
    ABORT_OOM
  }

  node = (TRI_aql_node_value_t*) TRI_Allocate(sizeof(TRI_aql_node_value_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_VALUE);

  node->_value._type = AQL_TYPE_STRING;
  node->_value._value._string = (char*) value;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeListAql (TRI_aql_parse_context_t* const context) {
  TRI_aql_node_list_t* node;

  assert(context);

  node = (TRI_aql_node_list_t*) TRI_Allocate(sizeof(TRI_aql_node_list_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_LIST);

  TRI_InitVectorPointer(&node->_values);
  node->_base.free = &FreeNodeList;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayAql (TRI_aql_parse_context_t* const context) {
  TRI_aql_node_array_t* node;

  assert(context);

  node = (TRI_aql_node_array_t*) TRI_Allocate(sizeof(TRI_aql_node_array_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_ARRAY);
  
  TRI_InitAssociativePointer(&node->_values,
                             &TRI_HashStringKeyAssociativePointer,
                             &HashArrayElement, 
                             &EqualArrayElement,
                             0);

  node->_base.free = &FreeNodeArray;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayElementAql (TRI_aql_parse_context_t* const context,
                                               const char* const name,
                                               const TRI_aql_node_t* const value) {
  TRI_aql_node_array_element_t* node;

  assert(context);

  if (!value) {
    ABORT_OOM
  }

  node = (TRI_aql_node_array_element_t*) TRI_Allocate(sizeof(TRI_aql_node_array_element_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_ARRAY_ELEMENT);
 
  node->_name = (char*) name;
  node->_value = (TRI_aql_node_t*) value;
  
  if (!node->_name) {
    ABORT_OOM
  }   

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST function call node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeFcallAql (TRI_aql_parse_context_t* const context,
                                        const char* const name,
                                        const TRI_aql_node_t* const parameters) {
  TRI_aql_node_fcall_t* node;
  TRI_aql_function_t* function;
  TRI_associative_pointer_t* functions;
  char* upperName;

  assert(context);
  assert(context->_vocbase);


  if (!name || !parameters) {
    ABORT_OOM
  }

  functions = context->_vocbase->_functionsAql;
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
    TRI_SetErrorAql(context, TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN, name);
    return NULL;
  }

  node = (TRI_aql_node_fcall_t*) TRI_Allocate(sizeof(TRI_aql_node_fcall_t));

  if (!node) {
    ABORT_OOM
  }

  InitNode(context, (TRI_aql_node_t*) node, AQL_NODE_FCALL);

  node->_name = function->_internalName;
  node->_parameters = (TRI_aql_node_t*) parameters;

  return (TRI_aql_node_t*) node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushListAql (TRI_aql_parse_context_t* const context, 
                      const TRI_aql_node_t* const value) {
  TRI_aql_node_list_t* list = (TRI_aql_node_list_t*) TRI_PeekStackAql(context);

  assert(context);
  assert(list);
  
  if (!value) {
    ABORT_OOM
  }

  TRI_PushBackVectorPointer(&list->_values, (void*) value);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of an array
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushArrayAql (TRI_aql_parse_context_t* const context, 
                       const char* const name,
                       const TRI_aql_node_t* const value) {
  TRI_aql_node_array_t* array = (TRI_aql_node_array_t*) TRI_PeekStackAql(context);
  TRI_aql_node_array_element_t* element;

  assert(context);
  assert(array);
  
  if (!value) {
    ABORT_OOM
  }

  element = (TRI_aql_node_array_element_t*) TRI_CreateNodeArrayElementAql(context, name, value);
  if (!element) {
    ABORT_OOM
  }
  
  if (TRI_InsertKeyAssociativePointer(&array->_values, element->_name, (void*) element, false)) {
    // duplicate element in a document/array, e.g. { "name" : "John", "name" : "Jim" }
    TRI_SetErrorAql(context, TRI_ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED, name); 
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
