////////////////////////////////////////////////////////////////////////////////
/// @brief AST to javascript-string conversion functions
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


#include "javascripter.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a string buffer for the results
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t *QLJavascripterInit (void) {
  TRI_string_buffer_t *buffer = (TRI_string_buffer_t *) TRI_Allocate(sizeof(TRI_string_buffer_t));

  if (buffer == 0) {
    return 0;
  }
  TRI_InitStringBuffer(buffer);
  return buffer;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief free the result string buffer
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterFree (TRI_string_buffer_t *buffer) {
  if (buffer == 0) {
    return;
  }

  TRI_FreeStringBuffer(buffer);
  TRI_Free(buffer);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Walk a horizontal list of elements and print them
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterWalkList (TRI_string_buffer_t * buffer, QL_ast_node_t *node) {
  QL_ast_node_t *next; 
  bool isFirst;

  if (node == 0) { 
    return;
  }

  next = node->_next; 
  isFirst=true;
 
  while (next != 0) {
    if (isFirst) {
      isFirst=false;
    }
    else {
      TRI_AppendCharStringBuffer(buffer, ',');
    }
    QLJavascripterWalk(buffer, next); 
    next = next->_next;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking the AST
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterWalk (TRI_string_buffer_t *buffer, QL_ast_node_t *node) {
  QL_ast_node_t *lhs, *rhs, *next;
  size_t outLength;

  if (node == 0) {
    return;
  }

  lhs = node->_lhs;
  rhs = node->_rhs;

  switch (node->_type) {
    case QLNodeValueUndefined:
      TRI_AppendStringStringBuffer(buffer, "undefined");
      return;
    case QLNodeValueNull:
      TRI_AppendStringStringBuffer(buffer, "null");
      return;
    case QLNodeValueBool:
      TRI_AppendStringStringBuffer(buffer, node->_value._boolValue ? "true" : "false");
      return;
    case QLNodeValueString:
      TRI_AppendCharStringBuffer(buffer, '\'');
      TRI_AppendStringStringBuffer(buffer, 
        TRI_EscapeUtf8String(node->_value._stringValue, strlen(node->_value._stringValue), false, &outLength));
      TRI_AppendCharStringBuffer(buffer, '\'');
      return;
    case QLNodeValueNumberInt:
      TRI_AppendInt64StringBuffer(buffer, node->_value._intValue);
      return;
    case QLNodeValueNumberDouble:
      TRI_AppendDoubleStringBuffer(buffer, node->_value._doubleValue);
      return;
    case QLNodeValueNumberDoubleString:
      TRI_AppendStringStringBuffer(buffer, node->_value._stringValue);
      return;
    case QLNodeValueArray:
      TRI_AppendCharStringBuffer(buffer, '[');
      QLJavascripterWalkList(buffer, rhs);
      TRI_AppendCharStringBuffer(buffer, ']');
      return; 
    case QLNodeValueDocument:
      TRI_AppendCharStringBuffer(buffer, '{');
      QLJavascripterWalkList(buffer, rhs);
      TRI_AppendCharStringBuffer(buffer, '}');
      return; 
    case QLNodeValueParameterNumeric:
    case QLNodeValueParameterNamed:
      // TODO: 
      return;
    case QLNodeValueIdentifier:
      TRI_AppendStringStringBuffer(buffer, node->_value._stringValue);
      return;
    case QLNodeValueNamedValue:
      QLJavascripterWalk(buffer, lhs);
      TRI_AppendCharStringBuffer(buffer, ':');
      QLJavascripterWalk(buffer, rhs);
      return;
    case QLNodeReferenceAttribute:
      TRI_AppendStringStringBuffer(buffer, "$['");
      QLJavascripterWalk(buffer, lhs);
      TRI_AppendStringStringBuffer(buffer, "']");
      next = (QL_ast_node_t*) rhs->_next;
      while (next != 0) {
        TRI_AppendCharStringBuffer(buffer, '.');
        QLJavascripterWalk(buffer, next);
        next = next->_next;
      }
      return;
    case QLNodeUnaryOperatorPlus:
    case QLNodeUnaryOperatorMinus:
    case QLNodeUnaryOperatorNot:
      TRI_AppendStringStringBuffer(buffer, QLAstNodeGetUnaryOperatorString(node->_type));
      QLJavascripterWalk(buffer, lhs);
      return;
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
      QLJavascripterWalk(buffer, lhs);
      TRI_AppendStringStringBuffer(buffer, QLAstNodeGetBinaryOperatorString(node->_type));
      QLJavascripterWalk(buffer, rhs);
      return;
    case QLNodeControlFunctionCall:
      QLJavascripterWalk(buffer, lhs);
      TRI_AppendCharStringBuffer(buffer, '(');
      QLJavascripterWalkList(buffer, rhs);
      TRI_AppendCharStringBuffer(buffer, ')');
    default:
      return;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

