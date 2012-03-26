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

#include "VocBase/query-node.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get text/label for a node based on its type
////////////////////////////////////////////////////////////////////////////////

const char* TRI_QueryNodeGetName (const TRI_query_node_type_e type) {
  switch (type) {
    case TRI_QueryNodeContainerList:
      return "list container";
    case TRI_QueryNodeContainerOrderElement:
      return "order element container";
    case TRI_QueryNodeContainerMemberAccess:
      return "member access";
    case TRI_QueryNodeContainerTernarySwitch:
      return "ternary operator switch";
    case TRI_QueryNodeContainerCoordinatePair:
      return "coordinate pair";

    case TRI_QueryNodeJoinList:
      return "join: list";
    case TRI_QueryNodeJoinInner:
      return "join: inner";
    case TRI_QueryNodeJoinLeft:
      return "join: left";
    case TRI_QueryNodeJoinRight:
      return "join: right";

    case TRI_QueryNodeValueUndefined:
      return "value: undefined";
    case TRI_QueryNodeValueNull:
      return "value: null";
    case TRI_QueryNodeValueBool:
      return "value: bool";
    case TRI_QueryNodeValueString:
      return "value: string";
    case TRI_QueryNodeValueNumberInt:
      return "value: number (int)";
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
      return "value: number (double)";
    case TRI_QueryNodeValueArray:
      return "value: array";
    case TRI_QueryNodeValueDocument:
      return "value: document";
    case TRI_QueryNodeValueParameterNamed:
      return "value: named parameter";
    case TRI_QueryNodeValueIdentifier:
      return "value: identifier";
    case TRI_QueryNodeValueNamedValue:
      return "value: named value";
    case TRI_QueryNodeValueCoordinate:
      return "value: geo coordinate";
    case TRI_QueryNodeValueOrderDirection:
      return "value: orderdirection";

    case TRI_QueryNodeReferenceCollection:
      return "reference: collection";
    case TRI_QueryNodeReferenceCollectionAlias:
      return "reference: collection alias";

    case TRI_QueryNodeRestrictWithin:
      return "within restrictor";
    case TRI_QueryNodeRestrictNear:
      return "near restrictor";

    case TRI_QueryNodeUnaryOperatorPlus:
      return "unary: plus";
    case TRI_QueryNodeUnaryOperatorMinus:
      return "unary: minus";
    case TRI_QueryNodeUnaryOperatorNot:
      return "unary: not";

    case TRI_QueryNodeBinaryOperatorIn:
      return "binary: in";
    case TRI_QueryNodeBinaryOperatorAnd:
      return "binary: and";
    case TRI_QueryNodeBinaryOperatorOr:
      return "binary: or";
    case TRI_QueryNodeBinaryOperatorEqual:
      return "binary: equal";
    case TRI_QueryNodeBinaryOperatorUnequal:
      return "binary: unequal";
    case TRI_QueryNodeBinaryOperatorLess:
      return "binary: less";
    case TRI_QueryNodeBinaryOperatorGreater:
      return "binary: greater";
    case TRI_QueryNodeBinaryOperatorLessEqual:
      return "binary: lessequal";
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
      return "binary: greaterequal";
    case TRI_QueryNodeBinaryOperatorAdd:
      return "binary: add";
    case TRI_QueryNodeBinaryOperatorSubtract:
      return "binary: subtract";
    case TRI_QueryNodeBinaryOperatorMultiply:
      return "binary: multiply";
    case TRI_QueryNodeBinaryOperatorDivide:
      return "binary: divide";
    case TRI_QueryNodeBinaryOperatorModulus:
      return "binary: modulus";
    
    case TRI_QueryNodeControlFunctionCall:
      return "function call";
    case TRI_QueryNodeControlTernary:
      return "ternary operator";

    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type group of a node based on its type
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_type_group_e TRI_QueryNodeGetTypeGroup (const TRI_query_node_type_e type) {
  switch (type) {
    case TRI_QueryNodeContainerList:
    case TRI_QueryNodeContainerOrderElement:
    case TRI_QueryNodeContainerMemberAccess:
    case TRI_QueryNodeContainerTernarySwitch:
    case TRI_QueryNodeContainerCoordinatePair:
      return TRI_QueryNodeGroupContainer;
    
    case TRI_QueryNodeJoinList:
    case TRI_QueryNodeJoinInner:
    case TRI_QueryNodeJoinLeft:
    case TRI_QueryNodeJoinRight:
      return TRI_QueryNodeGroupJoin;

    case TRI_QueryNodeValueUndefined:
    case TRI_QueryNodeValueNull:
    case TRI_QueryNodeValueBool:
    case TRI_QueryNodeValueString:
    case TRI_QueryNodeValueNumberInt:
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueArray:
    case TRI_QueryNodeValueDocument:
    case TRI_QueryNodeValueParameterNamed:
    case TRI_QueryNodeValueIdentifier:
    case TRI_QueryNodeValueNamedValue:
    case TRI_QueryNodeValueCoordinate:
    case TRI_QueryNodeValueOrderDirection:
      return TRI_QueryNodeGroupValue;

    case TRI_QueryNodeReferenceCollection:
    case TRI_QueryNodeReferenceCollectionAlias:
      return TRI_QueryNodeGroupReference;
    
    case TRI_QueryNodeRestrictWithin:
    case TRI_QueryNodeRestrictNear:
      return TRI_QueryNodeGroupRestrict;

    case TRI_QueryNodeUnaryOperatorPlus:
    case TRI_QueryNodeUnaryOperatorMinus:
    case TRI_QueryNodeUnaryOperatorNot:
      return TRI_QueryNodeGroupUnaryOperator;

    case TRI_QueryNodeBinaryOperatorIn:
    case TRI_QueryNodeBinaryOperatorAnd:
    case TRI_QueryNodeBinaryOperatorOr:
    case TRI_QueryNodeBinaryOperatorEqual:
    case TRI_QueryNodeBinaryOperatorUnequal:
    case TRI_QueryNodeBinaryOperatorLess:
    case TRI_QueryNodeBinaryOperatorGreater:
    case TRI_QueryNodeBinaryOperatorLessEqual:
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
    case TRI_QueryNodeBinaryOperatorAdd:
    case TRI_QueryNodeBinaryOperatorSubtract:
    case TRI_QueryNodeBinaryOperatorMultiply:
    case TRI_QueryNodeBinaryOperatorDivide:
    case TRI_QueryNodeBinaryOperatorModulus:
      return TRI_QueryNodeGroupBinaryOperator;

    case TRI_QueryNodeControlFunctionCall:
    case TRI_QueryNodeControlTernary:
      return TRI_QueryNodeGroupControl;

    default:
      return TRI_QueryNodeGroupUndefined;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the reverse of a relational operator
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_type_e TRI_QueryNodeGetReversedRelationalOperator (const TRI_query_node_type_e type) {
  switch (type) {
    case TRI_QueryNodeBinaryOperatorEqual:
      return TRI_QueryNodeBinaryOperatorUnequal;
    case TRI_QueryNodeBinaryOperatorUnequal:
      return TRI_QueryNodeBinaryOperatorEqual;
    case TRI_QueryNodeBinaryOperatorLess:
      return TRI_QueryNodeBinaryOperatorGreaterEqual;
    case TRI_QueryNodeBinaryOperatorLessEqual:
      return TRI_QueryNodeBinaryOperatorGreater;
    case TRI_QueryNodeBinaryOperatorGreater:
      return TRI_QueryNodeBinaryOperatorLessEqual;
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
      return TRI_QueryNodeBinaryOperatorLess;
    default:
      assert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a unary operator
////////////////////////////////////////////////////////////////////////////////

/*
char* TRI_QueryNodeGetUnaryOperatorString (const TRI_query_node_type_e type) {
  switch (type) {
    case TRI_QueryNodeUnaryOperatorPlus:
      return "+";
    case TRI_QueryNodeUnaryOperatorMinus:
      return "-";
    case TRI_QueryNodeUnaryOperatorNot:
      return "!";
    default:
      return "";
  }
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a binary operator
////////////////////////////////////////////////////////////////////////////////

/*
char* TRI_QueryNodeGetBinaryOperatorString (const TRI_query_node_type_e type) {
  switch (type) {
    case TRI_QueryNodeBinaryOperatorAnd: 
      return "&&";
    case TRI_QueryNodeBinaryOperatorOr: 
      return "||";
    case TRI_QueryNodeBinaryOperatorIdentical: 
      return "===";
    case TRI_QueryNodeBinaryOperatorUnidentical: 
      return "!==";
    case TRI_QueryNodeBinaryOperatorEqual: 
      return "==";
    case TRI_QueryNodeBinaryOperatorUnequal: 
      return "!=";
    case TRI_QueryNodeBinaryOperatorLess: 
      return "<";
    case TRI_QueryNodeBinaryOperatorGreater: 
      return ">";
    case TRI_QueryNodeBinaryOperatorLessEqual:
      return "<=";
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
      return ">=";
    case TRI_QueryNodeBinaryOperatorAdd:
      return "+";
    case TRI_QueryNodeBinaryOperatorSubtract:
      return "-";
    case TRI_QueryNodeBinaryOperatorMultiply:
      return "*";
    case TRI_QueryNodeBinaryOperatorDivide:
      return "/";
    case TRI_QueryNodeBinaryOperatorModulus:
      return "%";
    case TRI_QueryNodeBinaryOperatorIn:
      return " in ";
    default:
      return "";
  }
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a value node
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsValueNode (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeValueUndefined:
    case TRI_QueryNodeValueNull:
    case TRI_QueryNodeValueBool:
    case TRI_QueryNodeValueString:
    case TRI_QueryNodeValueNumberInt:
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueArray:
    case TRI_QueryNodeValueDocument:
    case TRI_QueryNodeValueParameterNamed:
    case TRI_QueryNodeValueIdentifier:
    case TRI_QueryNodeValueNamedValue:
    case TRI_QueryNodeValueOrderDirection:
      return true;
    default:
      return false;
  } 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsArithmeticOperator (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeBinaryOperatorAdd:
    case TRI_QueryNodeBinaryOperatorSubtract:
    case TRI_QueryNodeBinaryOperatorMultiply:
    case TRI_QueryNodeBinaryOperatorDivide:
    case TRI_QueryNodeBinaryOperatorModulus:
      return true;
    default: 
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a unary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsUnaryOperator (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeUnaryOperatorPlus:
    case TRI_QueryNodeUnaryOperatorMinus:
    case TRI_QueryNodeUnaryOperatorNot:
      return true;
    default: 
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a binary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsBinaryOperator (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeBinaryOperatorIn:
    case TRI_QueryNodeBinaryOperatorAnd:
    case TRI_QueryNodeBinaryOperatorOr:
    case TRI_QueryNodeBinaryOperatorEqual:
    case TRI_QueryNodeBinaryOperatorUnequal:
    case TRI_QueryNodeBinaryOperatorLess:
    case TRI_QueryNodeBinaryOperatorGreater:
    case TRI_QueryNodeBinaryOperatorLessEqual:
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
    case TRI_QueryNodeBinaryOperatorAdd:
    case TRI_QueryNodeBinaryOperatorSubtract:
    case TRI_QueryNodeBinaryOperatorMultiply:
    case TRI_QueryNodeBinaryOperatorDivide:
    case TRI_QueryNodeBinaryOperatorModulus: 
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is the ternary operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsTernaryOperator (const TRI_query_node_t* const node) {
  return (node->_type == TRI_QueryNodeControlTernary);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsLogicalOperator (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeBinaryOperatorAnd:
    case TRI_QueryNodeBinaryOperatorOr:
    case TRI_QueryNodeUnaryOperatorNot:
      return true;
    default: 
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a relational operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsRelationalOperator (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeBinaryOperatorEqual:
    case TRI_QueryNodeBinaryOperatorUnequal:
    case TRI_QueryNodeBinaryOperatorLess:
    case TRI_QueryNodeBinaryOperatorGreater:
    case TRI_QueryNodeBinaryOperatorLessEqual:
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
      return true;
    default: 
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a constant
////////////////////////////////////////////////////////////////////////////////

bool TRI_QueryNodeIsBooleanizable (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeValueBool:
    // case TRI_QueryNodeValueString: // TODO
    case TRI_QueryNodeValueNumberInt:
    case TRI_QueryNodeValueNumberDouble:
    case TRI_QueryNodeValueNumberDoubleString:
    case TRI_QueryNodeValueNull:
    // case TRI_QueryNodeValueArray: // TODO
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

