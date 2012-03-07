////////////////////////////////////////////////////////////////////////////////
/// @brief query node declarations
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_NODE_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_NODE_H 1

#include <BasicsC/common.h>
#include <BasicsC/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node types enumeration
///
/// Each node in the AST has one of these types.
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QueryNodeUndefined = 0,

  TRI_QueryNodeContainerList,
  TRI_QueryNodeContainerOrderElement,
  TRI_QueryNodeContainerMemberAccess,
  TRI_QueryNodeContainerTernarySwitch,
  TRI_QueryNodeContainerCoordinatePair,

  TRI_QueryNodeJoinList,
  TRI_QueryNodeJoinInner,
  TRI_QueryNodeJoinLeft,
  TRI_QueryNodeJoinRight, 

  TRI_QueryNodeValueUndefined,
  TRI_QueryNodeValueNull,
  TRI_QueryNodeValueBool,
  TRI_QueryNodeValueString,
  TRI_QueryNodeValueNumberInt,
  TRI_QueryNodeValueNumberDouble,
  TRI_QueryNodeValueNumberDoubleString,
  TRI_QueryNodeValueArray,
  TRI_QueryNodeValueDocument,
  TRI_QueryNodeValueParameterNamed,
  TRI_QueryNodeValueIdentifier,
  TRI_QueryNodeValueNamedValue,
  TRI_QueryNodeValueCoordinate,
  TRI_QueryNodeValueOrderDirection,

  TRI_QueryNodeReferenceCollection,
  TRI_QueryNodeReferenceCollectionAlias,

  TRI_QueryNodeRestrictWithin,
  TRI_QueryNodeRestrictNear,

  TRI_QueryNodeUnaryOperatorPlus,
  TRI_QueryNodeUnaryOperatorMinus,
  TRI_QueryNodeUnaryOperatorNot,

  TRI_QueryNodeBinaryOperatorIn,
  TRI_QueryNodeBinaryOperatorAnd,
  TRI_QueryNodeBinaryOperatorOr,
  TRI_QueryNodeBinaryOperatorIdentical,
  TRI_QueryNodeBinaryOperatorUnidentical,
  TRI_QueryNodeBinaryOperatorEqual,
  TRI_QueryNodeBinaryOperatorUnequal,
  TRI_QueryNodeBinaryOperatorLess,
  TRI_QueryNodeBinaryOperatorGreater,
  TRI_QueryNodeBinaryOperatorLessEqual,
  TRI_QueryNodeBinaryOperatorGreaterEqual,
  TRI_QueryNodeBinaryOperatorAdd,
  TRI_QueryNodeBinaryOperatorSubtract,
  TRI_QueryNodeBinaryOperatorMultiply,
  TRI_QueryNodeBinaryOperatorDivide,
  TRI_QueryNodeBinaryOperatorModulus,

  TRI_QueryNodeControlFunctionCall,
  TRI_QueryNodeControlTernary,

  TRI_QueryNodeLast
} 
TRI_query_node_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node type groups enumeration
///
/// Classification of AST node types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QueryNodeGroupUndefined = 0,

  TRI_QueryNodeGroupContainer,
  TRI_QueryNodeGroupJoin,
  TRI_QueryNodeGroupValue,
  TRI_QueryNodeGroupReference,
  TRI_QueryNodeGroupRestrict,
  TRI_QueryNodeGroupBinaryOperator,
  TRI_QueryNodeGroupUnaryOperator,
  TRI_QueryNodeGroupControl
} 
TRI_query_node_type_group_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node structure
///
/// Each node in the AST has a certain type (@ref TRI_query_node_type_e), but as 
/// we are in C land we use the same container struct for all node types. 
/// We represent the different values a node can have (string, bool, double 
/// etc.) with a union of these primitive types.
/// Nodes also contain pointers to following nodes: lhs = left hand side, and
/// rhs = right hand side. These are null pointers if a node does not have 
/// children. For unary operators, only lhs is set, for binary operators, both
/// lhs and rhs must be set.
/// Nodes might also have their "next" pointer set to a follower-node. This is
/// a special hack used to mimic elements in a (non-hierarchical) list, and is
/// used to represent arrays and objects.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_node_s {
  TRI_query_node_type_e     _type;
  union {
    bool                    _boolValue;
    int64_t                 _intValue;
    double                  _doubleValue;
    char*                   _stringValue;
  } _value;
  struct TRI_query_node_s*  _lhs;   // pointer to left child (might be null)
  struct TRI_query_node_s*  _rhs;   // pointer to right child (might be null) 
  struct TRI_query_node_s*  _next;  // pointer to next node (used for lists, might be null)
}
TRI_query_node_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief get text/label for a node based on its type
////////////////////////////////////////////////////////////////////////////////

const char* TRI_QueryNodeGetName (const TRI_query_node_type_e); 

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type group of a node based on its type
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_type_group_e TRI_QueryNodeGetTypeGroup (const TRI_query_node_type_e); 

////////////////////////////////////////////////////////////////////////////////
/// @brief get the reverse of a relational operator
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_type_e TRI_QueryNodeGetReversedRelationalOperator (const TRI_query_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a unary operator
////////////////////////////////////////////////////////////////////////////////

char* TRI_QueryNodeGetUnaryOperatorString (const TRI_query_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a binary operator
////////////////////////////////////////////////////////////////////////////////

char* TRI_QueryNodeGetBinaryOperatorString (const TRI_query_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a unary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsUnaryOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a value node
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsValueNode (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a binary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsBinaryOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is the ternary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsTernaryOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsLogicalOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is an arithmetic operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsArithmeticOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a relational operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsRelationalOperator (const TRI_query_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is convertable into a bool value
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsBooleanizable (const TRI_query_node_t* const);

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
