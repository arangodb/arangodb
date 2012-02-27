////////////////////////////////////////////////////////////////////////////////
/// @brief AST node declarations
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

#ifndef TRIAGENS_DURHAM_QL_ASTNODE
#define TRIAGENS_DURHAM_QL_ASTNODE

#include <BasicsC/common.h>
#include <BasicsC/vector.h>


#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node types enumeration
///
/// Each node in the AST has one of these types.
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  QLNodeUndefined = 0,

  QLNodeContainerList,
  QLNodeContainerOrderElement,
  QLNodeContainerMemberAccess,
  QLNodeContainerTernarySwitch,
  QLNodeContainerCoordinatePair,

  QLNodeJoinList,
  QLNodeJoinInner,
  QLNodeJoinLeft,
  QLNodeJoinRight, 

  QLNodeValueUndefined,
  QLNodeValueNull,
  QLNodeValueBool,
  QLNodeValueString,
  QLNodeValueNumberInt,
  QLNodeValueNumberDouble,
  QLNodeValueNumberDoubleString,
  QLNodeValueArray,
  QLNodeValueDocument,
  QLNodeValueParameterNumeric,
  QLNodeValueParameterNamed,
  QLNodeValueIdentifier,
  QLNodeValueNamedValue,
  QLNodeValueCoordinate,
  QLNodeValueOrderDirection,

  QLNodeReferenceCollection,
  QLNodeReferenceCollectionAlias,

  QLNodeRestrictWithin,
  QLNodeRestrictNear,

  QLNodeUnaryOperatorPlus,
  QLNodeUnaryOperatorMinus,
  QLNodeUnaryOperatorNot,

  QLNodeBinaryOperatorIn,
  QLNodeBinaryOperatorAnd,
  QLNodeBinaryOperatorOr,
  QLNodeBinaryOperatorIdentical,
  QLNodeBinaryOperatorUnidentical,
  QLNodeBinaryOperatorEqual,
  QLNodeBinaryOperatorUnequal,
  QLNodeBinaryOperatorLess,
  QLNodeBinaryOperatorGreater,
  QLNodeBinaryOperatorLessEqual,
  QLNodeBinaryOperatorGreaterEqual,
  QLNodeBinaryOperatorAdd,
  QLNodeBinaryOperatorSubtract,
  QLNodeBinaryOperatorMultiply,
  QLNodeBinaryOperatorDivide,
  QLNodeBinaryOperatorModulus,

  QLNodeControlFunctionCall,
  QLNodeControlTernary,

  QLNodeLast
} 
QL_ast_node_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node type groups enumeration
///
/// Classification of AST node types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  QLNodeGroupUndefined = 0,

  QLNodeGroupContainer,
  QLNodeGroupJoin,
  QLNodeGroupValue,
  QLNodeGroupReference,
  QLNodeGroupRestrict,
  QLNodeGroupBinaryOperator,
  QLNodeGroupUnaryOperator,
  QLNodeGroupControl
} 
QL_ast_node_type_group_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node structure
///
/// Each node in the AST has a certain type (@ref QL_ast_node_type_e), but as 
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

typedef struct QL_ast_node_s {
  QL_ast_node_type_e _type;
  union {
    bool             _boolValue;
    int64_t          _intValue;
    double           _doubleValue;
    char             *_stringValue;
  } _value;
  int32_t            _line;
  int32_t            _column;
  void               *_lhs;          // pointer to left child (might be null)
  void               *_rhs;          // pointer to right child (might be null) 
  void               *_next;         // pointer to next node (used for lists, might be null)
}
QL_ast_node_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief get text/label for a node based on its type
////////////////////////////////////////////////////////////////////////////////

const char* QLAstNodeGetName (const QL_ast_node_type_e); 

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type group of a node based on its type
////////////////////////////////////////////////////////////////////////////////

QL_ast_node_type_group_e QLAstNodeGetTypeGroup (const QL_ast_node_type_e); 

////////////////////////////////////////////////////////////////////////////////
/// @brief get the reverse of a relational operator
////////////////////////////////////////////////////////////////////////////////

QL_ast_node_type_e QLAstNodeGetReversedRelationalOperator (const QL_ast_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a unary operator
////////////////////////////////////////////////////////////////////////////////

char* QLAstNodeGetUnaryOperatorString (const QL_ast_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a binary operator
////////////////////////////////////////////////////////////////////////////////

char* QLAstNodeGetBinaryOperatorString (const QL_ast_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a unary operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsUnaryOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a value node
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsValueNode (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a binary operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsBinaryOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is the ternary operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsTernaryOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsLogicalOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is an arithmetic operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsArithmeticOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a relational operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsRelationalOperator (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is convertable into a bool value
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsBooleanizable (const QL_ast_node_t*);

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

