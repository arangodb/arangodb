////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser nodes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_AHUACATL_AHUACATL_NODE_H
#define TRIAGENS_AHUACATL_AHUACATL_NODE_H 1

#include "BasicsC/common.h"
#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief indexes of node members and their meanings
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_IDX_FOR_VARIABLE                                              0
#define TRI_AQL_IDX_FOR_EXPRESSION                                            1
#define TRI_AQL_IDX_LET_VARIABLE                                              0
#define TRI_AQL_IDX_LET_EXPRESSION                                            1
#define TRI_AQL_IDX_FILTER_EXPRESSION                                         0
#define TRI_AQL_IDX_COLLECT_LIST                                              0
#define TRI_AQL_IDX_COLLECT_INTO                                              1
#define TRI_AQL_IDX_RETURN_EXPRESSION                                         0
#define TRI_AQL_IDX_SORT_LIST                                                 0
#define TRI_AQL_IDX_SORT_ELEMENT_EXPRESSION                                   0
#define TRI_AQL_IDX_ATTRIBUTE_ACCESS_ACCESSED                                 0
#define TRI_AQL_IDX_INDEXED_ACCESSED                                          0
#define TRI_AQL_IDX_INDEXED_INDEX                                             1
#define TRI_AQL_IDX_EXPAND_VARIABLE1                                          0
#define TRI_AQL_IDX_EXPAND_VARIABLE2                                          1
#define TRI_AQL_IDX_EXPAND_EXPANDED                                           2
#define TRI_AQL_IDX_EXPAND_EXPANSION                                          3
#define TRI_AQL_IDX_ASSIGN_VARIABLE                                           0
#define TRI_AQL_IDX_ASSIGN_EXPRESSION                                         1
#define TRI_AQL_IDX_UNARY_OPERAND                                             0
#define TRI_AQL_IDX_BINARY_LHS                                                0
#define TRI_AQL_IDX_BINARY_RHS                                                1
#define TRI_AQL_IDX_TERNARY_CONDITION                                         0
#define TRI_AQL_IDX_TERNARY_TRUEPART                                          1
#define TRI_AQL_IDX_TERNARY_FALSEPART                                         2
#define TRI_AQL_IDX_SUBQUERY_VARIABLE                                         0
#define TRI_AQL_IDX_FCALL_PARAMETERS                                          0
#define TRI_AQL_IDX_ARRAY_ELEMENT_SUBNODE                                     0
#define TRI_AQL_IDX_ARRAY_VALUES                                              0

////////////////////////////////////////////////////////////////////////////////
/// @brief access the int value of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_INT(node) node->_value._value._int

////////////////////////////////////////////////////////////////////////////////
/// @brief access the double value of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_DOUBLE(node) node->_value._value._double

////////////////////////////////////////////////////////////////////////////////
/// @brief access the bool value of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_BOOL(node) node->_value._value._bool

////////////////////////////////////////////////////////////////////////////////
/// @brief access the string value of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_STRING(node) node->_value._value._string

////////////////////////////////////////////////////////////////////////////////
/// @brief access the data value of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_DATA(node) node->_value._value._data

////////////////////////////////////////////////////////////////////////////////
/// @brief access the value type of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_TYPE(node) node->_value._type

////////////////////////////////////////////////////////////////////////////////
/// @brief access the nth subnode of a node
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_NODE_MEMBER(node, n) (TRI_aql_node_t*) node->_members._buffer[n]

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_NODE_NOP = 0,
  TRI_AQL_NODE_RETURN_EMPTY,
  TRI_AQL_NODE_SCOPE_START,
  TRI_AQL_NODE_SCOPE_END,
  TRI_AQL_NODE_FOR,
  TRI_AQL_NODE_LET,
  TRI_AQL_NODE_FILTER,
  TRI_AQL_NODE_RETURN,
  TRI_AQL_NODE_COLLECT,
  TRI_AQL_NODE_SORT,
  TRI_AQL_NODE_SORT_ELEMENT,
  TRI_AQL_NODE_LIMIT,
  TRI_AQL_NODE_VARIABLE,
  TRI_AQL_NODE_ASSIGN,
  TRI_AQL_NODE_OPERATOR_UNARY_PLUS,
  TRI_AQL_NODE_OPERATOR_UNARY_MINUS,
  TRI_AQL_NODE_OPERATOR_UNARY_NOT,
  TRI_AQL_NODE_OPERATOR_BINARY_AND,
  TRI_AQL_NODE_OPERATOR_BINARY_OR,
  TRI_AQL_NODE_OPERATOR_BINARY_PLUS,
  TRI_AQL_NODE_OPERATOR_BINARY_MINUS,
  TRI_AQL_NODE_OPERATOR_BINARY_TIMES,
  TRI_AQL_NODE_OPERATOR_BINARY_DIV,
  TRI_AQL_NODE_OPERATOR_BINARY_MOD,
  TRI_AQL_NODE_OPERATOR_BINARY_EQ,
  TRI_AQL_NODE_OPERATOR_BINARY_NE,
  TRI_AQL_NODE_OPERATOR_BINARY_LT,
  TRI_AQL_NODE_OPERATOR_BINARY_LE,
  TRI_AQL_NODE_OPERATOR_BINARY_GT,
  TRI_AQL_NODE_OPERATOR_BINARY_GE,
  TRI_AQL_NODE_OPERATOR_BINARY_IN,
  TRI_AQL_NODE_OPERATOR_TERNARY,
  TRI_AQL_NODE_SUBQUERY,
  TRI_AQL_NODE_ATTRIBUTE_ACCESS,
  TRI_AQL_NODE_INDEXED,
  TRI_AQL_NODE_EXPAND,
  TRI_AQL_NODE_VALUE,
  TRI_AQL_NODE_LIST,
  TRI_AQL_NODE_ARRAY,
  TRI_AQL_NODE_ARRAY_ELEMENT,
  TRI_AQL_NODE_COLLECTION,
  TRI_AQL_NODE_REFERENCE,
  TRI_AQL_NODE_ATTRIBUTE,
  TRI_AQL_NODE_PARAMETER,
  TRI_AQL_NODE_FCALL
}
TRI_aql_node_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST value types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_TYPE_FAIL = 0,
  TRI_AQL_TYPE_NULL,
  TRI_AQL_TYPE_INT,
  TRI_AQL_TYPE_DOUBLE,
  TRI_AQL_TYPE_BOOL,
  TRI_AQL_TYPE_STRING
}
TRI_aql_value_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for an AQL value
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_value_s {
  union {
    int64_t _int;
    double _double;
    bool _bool;
    char* _string;
    void* _data;
  } _value;
  TRI_aql_value_type_e _type;
}
TRI_aql_value_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief an abstract AST node type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_node_s {
  TRI_vector_pointer_t _members;
  TRI_aql_node_type_e _type;
  TRI_aql_value_t _value;
}
TRI_aql_node_t;

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

bool TRI_IsTopLevelTypeAql (const TRI_aql_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the node type group
////////////////////////////////////////////////////////////////////////////////

const char* TRI_NodeGroupAql (const TRI_aql_node_t* const, const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the "nice" name of an AST node
////////////////////////////////////////////////////////////////////////////////

const char* TRI_NodeNameAql (const TRI_aql_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief return true if a node is a list node
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsListNodeAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a constant
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsConstantValueNodeAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is numeric
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsNumericValueNodeAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a node value is boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsBooleanValueNodeAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
