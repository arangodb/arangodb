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

#include "ast-node.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get text/label for a node based on its type
////////////////////////////////////////////////////////////////////////////////

const char *QLAstNodeGetName (const QL_ast_node_type_e type) {
  switch (type) {
    case QLNodeContainerList:
      return "list container";
    case QLNodeContainerOrderElement:
      return "order element container";

    case QLNodeJoinList:
      return "join: list";
    case QLNodeJoinInner:
      return "join: inner";
    case QLNodeJoinLeft:
      return "join: left";
    case QLNodeJoinRight:
      return "join: right";

    case QLNodeValueUndefined:
      return "value: undefined";
    case QLNodeValueNull:
      return "value: null";
    case QLNodeValueBool:
      return "value: bool";
    case QLNodeValueString:
      return "value: string";
    case QLNodeValueNumberInt:
      return "value: number (int)";
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
      return "value: number (double)";
    case QLNodeValueArray:
      return "value: array";
    case QLNodeValueDocument:
      return "value: document";
    case QLNodeValueParameterNumeric:
      return "value: numeric parameter";
    case QLNodeValueParameterNamed:
      return "value: named parameter";
    case QLNodeValueIdentifier:
      return "value: identifier";
    case QLNodeValueNamedValue:
      return "value: named value";
    case QLNodeValueOrderDirection:
      return "value: orderdirection";

    case QLNodeReferenceAttribute:
      return "reference: attribute";
    case QLNodeReferenceCollection:
      return "reference: collection";

    case QLNodeUnaryOperatorPlus:
      return "unary: plus";
    case QLNodeUnaryOperatorMinus:
      return "unary: minus";
    case QLNodeUnaryOperatorNot:
      return "unary: not";

    case QLNodeBinaryOperatorIn:
      return "binary: in";
    case QLNodeBinaryOperatorAnd:
      return "binary: and";
    case QLNodeBinaryOperatorOr:
      return "binary: or";
    case QLNodeBinaryOperatorIdentical:
      return "binary: identical";
    case QLNodeBinaryOperatorUnidentical:
      return "binary: unidentical";
    case QLNodeBinaryOperatorEqual:
      return "binary: equal";
    case QLNodeBinaryOperatorUnequal:
      return "binary: unequal";
    case QLNodeBinaryOperatorLess:
      return "binary: less";
    case QLNodeBinaryOperatorGreater:
      return "binary: greater";
    case QLNodeBinaryOperatorLessEqual:
      return "binary: lessequal";
    case QLNodeBinaryOperatorGreaterEqual:
      return "binary: greaterequal";
    case QLNodeBinaryOperatorAdd:
      return "binary: add";
    case QLNodeBinaryOperatorSubtract:
      return "binary: subtract";
    case QLNodeBinaryOperatorMultiply:
      return "binary: multiply";
    case QLNodeBinaryOperatorDivide:
      return "binary: divide";
    case QLNodeBinaryOperatorModulus:
      return "binary: modulus";
    
    case QLNodeControlFunctionCall:
      return "function call";

    default:
      return "unknown";
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the type group of a node based on its type
////////////////////////////////////////////////////////////////////////////////

QL_ast_node_type_group_e QLAstNodeGetTypeGroup (const QL_ast_node_type_e type) {
  switch (type) {
    case QLNodeContainerList:
    case QLNodeContainerOrderElement:
      return QLNodeGroupContainer;
    
    case QLNodeJoinList:
    case QLNodeJoinInner:
    case QLNodeJoinLeft:
    case QLNodeJoinRight:
      return QLNodeGroupJoin;

    case QLNodeValueUndefined:
    case QLNodeValueNull:
    case QLNodeValueBool:
    case QLNodeValueString:
    case QLNodeValueNumberInt:
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
    case QLNodeValueArray:
    case QLNodeValueDocument:
    case QLNodeValueParameterNumeric:
    case QLNodeValueParameterNamed:
    case QLNodeValueIdentifier:
    case QLNodeValueOrderDirection:
      return QLNodeGroupValue;

    case QLNodeReferenceAttribute:
    case QLNodeReferenceCollection:
      return QLNodeGroupReference;

    case QLNodeUnaryOperatorPlus:
    case QLNodeUnaryOperatorMinus:
    case QLNodeUnaryOperatorNot:
      return QLNodeGroupUnaryOperator;

    case QLNodeBinaryOperatorIn:
    case QLNodeBinaryOperatorAnd:
    case QLNodeBinaryOperatorOr:
    case QLNodeBinaryOperatorIdentical:
    case QLNodeBinaryOperatorUnidentical:
    case QLNodeBinaryOperatorEqual:
    case QLNodeBinaryOperatorUnequal:
    case QLNodeBinaryOperatorLess:
    case QLNodeBinaryOperatorGreater:
    case QLNodeBinaryOperatorLessEqual:
    case QLNodeBinaryOperatorGreaterEqual:
    case QLNodeBinaryOperatorAdd:
    case QLNodeBinaryOperatorSubtract:
    case QLNodeBinaryOperatorMultiply:
    case QLNodeBinaryOperatorDivide:
    case QLNodeBinaryOperatorModulus:
      return QLNodeGroupBinaryOperator;

    case QLNodeControlFunctionCall:
      return QLNodeGroupControl;

    default:
      return QLNodeGroupUndefined;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a unary operator
////////////////////////////////////////////////////////////////////////////////

char *QLAstNodeGetUnaryOperatorString (const QL_ast_node_type_e type) {
  switch (type) {
    case QLNodeUnaryOperatorPlus:
      return "+";
    case QLNodeUnaryOperatorMinus:
      return "-";
    case QLNodeUnaryOperatorNot:
      return "!";
    default:
      return "";
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the label string for a binary operator
////////////////////////////////////////////////////////////////////////////////

char *QLAstNodeGetBinaryOperatorString (const QL_ast_node_type_e type) {
  switch (type) {
    case QLNodeBinaryOperatorAnd: 
      return "&&";
    case QLNodeBinaryOperatorOr: 
      return "||";
    case QLNodeBinaryOperatorIdentical: 
      return "===";
    case QLNodeBinaryOperatorUnidentical: 
      return "!==";
    case QLNodeBinaryOperatorEqual: 
      return "==";
    case QLNodeBinaryOperatorUnequal: 
      return "!=";
    case QLNodeBinaryOperatorLess: 
      return "<";
    case QLNodeBinaryOperatorGreater: 
      return ">";
    case QLNodeBinaryOperatorLessEqual:
      return "<=";
    case QLNodeBinaryOperatorGreaterEqual:
      return ">=";
    case QLNodeBinaryOperatorAdd:
      return "+";
    case QLNodeBinaryOperatorSubtract:
      return "-";
    case QLNodeBinaryOperatorMultiply:
      return "*";
    case QLNodeBinaryOperatorDivide:
      return "/";
    case QLNodeBinaryOperatorModulus:
      return "%";
    default:
      return "";
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsArithmeticOperator (QL_ast_node_t *node) {
  switch (node->_type) {
    case QLNodeBinaryOperatorAdd:
    case QLNodeBinaryOperatorSubtract:
    case QLNodeBinaryOperatorMultiply:
    case QLNodeBinaryOperatorDivide:
    case QLNodeBinaryOperatorModulus:
      return true;
    default: 
      return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a logical operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsLogicalOperator (QL_ast_node_t *node) {
  switch (node->_type) {
    case QLNodeBinaryOperatorAnd:
    case QLNodeBinaryOperatorOr:
      return true;
    default: 
      return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a relational operator
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsRelationalOperator (QL_ast_node_t *node) {
  switch (node->_type) {
    case QLNodeBinaryOperatorIdentical:
    case QLNodeBinaryOperatorUnidentical:
    case QLNodeBinaryOperatorEqual:
    case QLNodeBinaryOperatorUnequal:
    case QLNodeBinaryOperatorLess:
    case QLNodeBinaryOperatorGreater:
    case QLNodeBinaryOperatorLessEqual:
    case QLNodeBinaryOperatorGreaterEqual:
      return true;
    default: 
      return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return whether a node is a constant
////////////////////////////////////////////////////////////////////////////////

bool QLAstNodeIsBooleanizable (QL_ast_node_t *node) {
  switch (node->_type) {
    case QLNodeValueBool:
    // case QLNodeValueString: // TODO
    case QLNodeValueNumberInt:
    case QLNodeValueNumberDouble:
    case QLNodeValueNumberDoubleString:
    case QLNodeValueNull:
    // case QLNodeValueArray: // TODO
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

