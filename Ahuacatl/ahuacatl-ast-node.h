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

#ifndef TRIAGENS_DURHAM_AHUACATL_AST_NODE_H
#define TRIAGENS_DURHAM_AHUACATL_AST_NODE_H 1

#include <BasicsC/common.h>
#include <BasicsC/strings.h>
#include <BasicsC/hashes.h>
#include <BasicsC/vector.h>
#include <BasicsC/associative.h>

#include "Ahuacatl/ahuacatl-context.h"

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
/// @brief indexes of variable subnodes and their meanings
////////////////////////////////////////////////////////////////////////////////

#define TRI_AQL_IDX_FOR_VARIABLE                                              0
#define TRI_AQL_IDX_FOR_EXPRESSION                                            1
#define TRI_AQL_IDX_ASSIGN_VARIABLE                                           0
#define TRI_AQL_IDX_ASSIGN_EXPRESSION                                         1
#define TRI_AQL_IDX_FILTER_EXPRESSION                                         0
#define TRI_AQL_IDX_COLLECT_LIST                                              0
#define TRI_AQL_IDX_COLLECT_INTO                                              1
#define TRI_AQL_IDX_RETURN_EXPRESSION                                         0
#define TRI_AQL_IDX_SORT_LIST                                                 0
#define TRI_AQL_IDX_SORT_ELEMENT_EXPRESSION                                   0
#define TRI_AQL_IDX_ATTRIBUTE_ACCESS_ACCESSED                                 0
#define TRI_AQL_IDX_INDEXED_ACCESSED                                          0
#define TRI_AQL_IDX_INDEXED_INDEX                                             1
#define TRI_AQL_IDX_EXPAND_EXPANDED                                           0
#define TRI_AQL_IDX_EXPAND_EXPANSION                                          1
#define TRI_AQL_IDX_UNARY_OPERAND                                             0
#define TRI_AQL_IDX_BINARY_LHS                                                0
#define TRI_AQL_IDX_BINARY_RHS                                                1
#define TRI_AQL_IDX_TERNARY_CONDITION                                         0
#define TRI_AQL_IDX_TERNARY_TRUEPART                                          1
#define TRI_AQL_IDX_TERNARY_FALSEPART                                         2
#define TRI_AQL_IDX_SUBQUERY_QUERY                                            0
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

#define TRI_AQL_NODE_SUBNODE(node, n) (TRI_aql_node_t*) node->_subNodes._buffer[n]

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
  AQL_NODE_UNDEFINED = 0,
  AQL_NODE_MAIN, 
  AQL_NODE_FOR,
  AQL_NODE_LET,
  AQL_NODE_FILTER,
  AQL_NODE_RETURN,
  AQL_NODE_COLLECT,
  AQL_NODE_SORT,
  AQL_NODE_SORT_ELEMENT,
  AQL_NODE_LIMIT,
  AQL_NODE_VARIABLE,
  AQL_NODE_ASSIGN,
  AQL_NODE_OPERATOR_UNARY_PLUS,
  AQL_NODE_OPERATOR_UNARY_MINUS,
  AQL_NODE_OPERATOR_UNARY_NOT,
  AQL_NODE_OPERATOR_BINARY_AND,
  AQL_NODE_OPERATOR_BINARY_OR,
  AQL_NODE_OPERATOR_BINARY_PLUS,
  AQL_NODE_OPERATOR_BINARY_MINUS,
  AQL_NODE_OPERATOR_BINARY_TIMES,
  AQL_NODE_OPERATOR_BINARY_DIV,
  AQL_NODE_OPERATOR_BINARY_MOD,
  AQL_NODE_OPERATOR_BINARY_EQ,
  AQL_NODE_OPERATOR_BINARY_NE,
  AQL_NODE_OPERATOR_BINARY_LT,
  AQL_NODE_OPERATOR_BINARY_LE,
  AQL_NODE_OPERATOR_BINARY_GT,
  AQL_NODE_OPERATOR_BINARY_GE,
  AQL_NODE_OPERATOR_BINARY_IN,
  AQL_NODE_OPERATOR_TERNARY,
  AQL_NODE_SUBQUERY,
  AQL_NODE_ATTRIBUTE_ACCESS,
  AQL_NODE_INDEXED,
  AQL_NODE_EXPAND,
  AQL_NODE_VALUE,
  AQL_NODE_LIST,
  AQL_NODE_ARRAY,
  AQL_NODE_ARRAY_ELEMENT,
  AQL_NODE_COLLECTION,
  AQL_NODE_REFERENCE,
  AQL_NODE_ATTRIBUTE,
  AQL_NODE_PARAMETER,
  AQL_NODE_FCALL
}
TRI_aql_node_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST value types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  AQL_TYPE_FAIL = 0,
  AQL_TYPE_NULL,
  AQL_TYPE_INT,
  AQL_TYPE_DOUBLE,
  AQL_TYPE_BOOL,
  AQL_TYPE_STRING
}
TRI_aql_value_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for an AQL value
////////////////////////////////////////////////////////////////////////////////
  
typedef struct TRI_aql_value_s {
  TRI_aql_value_type_e _type;
  union {
    int64_t _int;
    double _double;
    bool _bool;
    char* _string;
    void* _data;
  } _value;
} 
TRI_aql_value_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief an abstract AST node type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_node_s {
  TRI_aql_node_type_e _type;
  TRI_aql_value_t _value;
  TRI_vector_pointer_t _subNodes;
  struct TRI_aql_node_s* _next;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST main node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeMainAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeForAql (TRI_aql_context_t* const,
                                      const char* const,
                                      const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLetAql (TRI_aql_context_t* const,
                                      const char* const,
                                      const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST filter node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeFilterAql (TRI_aql_context_t* const, 
                                         const TRI_aql_node_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST return node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReturnAql (TRI_aql_context_t* const,
                                         const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeCollectAql (TRI_aql_context_t* const,
                                          const TRI_aql_node_t* const list,
                                          const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortAql (TRI_aql_context_t* const,
                                       const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSortElementAql (TRI_aql_context_t* const,
                                              const TRI_aql_node_t* const, 
                                              const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST limit node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeLimitAql (TRI_aql_context_t* const,
                                        const TRI_aql_node_t* const,
                                        const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST assign node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAssignAql (TRI_aql_context_t* const,
                                         const char* const,
                                         const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST parameter node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeParameterAql (TRI_aql_context_t* const,
                                            const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeVariableAql (TRI_aql_context_t* const,
                                           const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeCollectionAql (TRI_aql_context_t* const,
                                             const char* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeReferenceAql (TRI_aql_context_t* const,
                                            const char* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAql (TRI_aql_context_t* const,
                                            const char* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryPlusAql (TRI_aql_context_t* const,
                                                    const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryMinusAql (TRI_aql_context_t* const,
                                                     const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary not node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorUnaryNotAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary and node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryAndAql (TRI_aql_context_t* const,
                                                    const TRI_aql_node_t* const, 
                                                    const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary or node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryOrAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary plus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryPlusAql (TRI_aql_context_t* const,
                                                     const TRI_aql_node_t* const, 
                                                     const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary minus node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryMinusAql (TRI_aql_context_t* const,
                                                      const TRI_aql_node_t* const, 
                                                      const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary times node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryTimesAql (TRI_aql_context_t* const,
                                                      const TRI_aql_node_t* const, 
                                                      const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary div node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryDivAql (TRI_aql_context_t* const,
                                                    const TRI_aql_node_t* const, 
                                                    const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary mod node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryModAql (TRI_aql_context_t* const,
                                                    const TRI_aql_node_t* const, 
                                                    const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary eq node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryEqAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ne node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryNeAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary lt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLtAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary le node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryLeAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary gt node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGtAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary ge node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryGeAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary in node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorBinaryInAql (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const, 
                                                   const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST ternary operator node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeOperatorTernaryAql (TRI_aql_context_t* const,
                                                  const TRI_aql_node_t* const,
                                                  const TRI_aql_node_t* const,
                                                  const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST subquery node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeSubqueryAql (TRI_aql_context_t* const,
                                           const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeAttributeAccessAql (TRI_aql_context_t* const,
                                                  const TRI_aql_node_t* const,
                                                  const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST index access node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeIndexedAql (TRI_aql_context_t* const,
                                          const TRI_aql_node_t* const,
                                          const TRI_aql_node_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expand node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeExpandAql (TRI_aql_context_t* const,
                                         const TRI_aql_node_t* const,
                                         const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST null value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueNullAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueIntAql (TRI_aql_context_t* const,
                                           const int64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST double value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueDoubleAql (TRI_aql_context_t* const,
                                              const double);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueBoolAql (TRI_aql_context_t* const,
                                            const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST string value node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeValueStringAql (TRI_aql_context_t* const,
                                              const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeListAql (TRI_aql_context_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayAql (TRI_aql_context_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST function call node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeFcallAql (TRI_aql_context_t* const,
                                        const char* const,
                                        const TRI_aql_node_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_CreateNodeArrayElementAql (TRI_aql_context_t* const,
                                               const char* const,
                                               const TRI_aql_node_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushListAql (TRI_aql_context_t* const, 
                      const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a value to the end of an array
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushArrayAql (TRI_aql_context_t* const, 
                       const char* const,
                       const TRI_aql_node_t* const);

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
/// @brief get the numeric value of a node
////////////////////////////////////////////////////////////////////////////////

double TRI_GetNumericNodeValueAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the boolean value of a node
////////////////////////////////////////////////////////////////////////////////

bool TRI_GetBooleanNodeValueAql (const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
