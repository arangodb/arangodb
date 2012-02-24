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

#include "QL/javascripter.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the to-Javascript conversion context
////////////////////////////////////////////////////////////////////////////////

QL_javascript_conversion_t* QLJavascripterInit (void) {
  TRI_string_buffer_t* buffer;
  QL_javascript_conversion_t* converter;

  converter = (QL_javascript_conversion_t*)  TRI_Allocate(sizeof(QL_javascript_conversion_t));

  if (converter == 0) { 
    return 0;
  }

  // init
  converter->_buffer = 0;
  converter->_prefix = 0;

  buffer = (TRI_string_buffer_t*) TRI_Allocate(sizeof(TRI_string_buffer_t));
  if (buffer == 0) {
    TRI_Free(converter);
    return 0;
  }

  TRI_InitStringBuffer(buffer);
  converter->_buffer = buffer;

  return converter;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the to-Javascript conversion text
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterFree (QL_javascript_conversion_t* converter) {
  if (converter == 0) {
    return;
  }

  TRI_FreeStringBuffer(converter->_buffer);
  TRI_Free(converter->_buffer);
  TRI_Free(converter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Walk a horizontal list of elements and print them
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterWalkList (QL_javascript_conversion_t* converter, 
                             QL_ast_node_t* node, 
                             const char separator, 
                             size_t counter) {
  QL_ast_node_t* next; 

  if (node == 0) { 
    return;
  }

  next = node->_next; 
 
  while (next != 0) {
    if (counter++ > 0) {
      TRI_AppendCharStringBuffer(converter->_buffer, separator);
    }
    QLJavascripterConvert(converter, next); 
    next = next->_next;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an expression AST
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterConvert (QL_javascript_conversion_t* converter, 
                            QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs;
  size_t outLength;

  if (node == 0) {
    return;
  }

  lhs = node->_lhs;
  rhs = node->_rhs;

  switch (node->_type) {
    case QLNodeValueUndefined:
      TRI_AppendStringStringBuffer(converter->_buffer, "undefined");
      return;
    case QLNodeValueNull:
      TRI_AppendStringStringBuffer(converter->_buffer, "null");
      return;
    case QLNodeValueBool:
      TRI_AppendStringStringBuffer(converter->_buffer, node->_value._boolValue ? "true" : "false");
      return;
    case QLNodeValueString:
      TRI_AppendCharStringBuffer(converter->_buffer, '"');
      TRI_AppendStringStringBuffer(converter->_buffer, 
        TRI_EscapeUtf8String(node->_value._stringValue, strlen(node->_value._stringValue), false, &outLength));
      TRI_AppendCharStringBuffer(converter->_buffer, '"');
      return;
    case QLNodeValueNumberInt:
      TRI_AppendInt64StringBuffer(converter->_buffer, node->_value._intValue);
      return;
    case QLNodeValueNumberDouble:
      TRI_AppendDoubleStringBuffer(converter->_buffer, node->_value._doubleValue);
      return;
    case QLNodeValueNumberDoubleString:
      TRI_AppendStringStringBuffer(converter->_buffer, node->_value._stringValue);
      return;
    case QLNodeValueArray:
      TRI_AppendCharStringBuffer(converter->_buffer, '[');
      QLJavascripterWalkList(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ']');
      return; 
    case QLNodeValueDocument:
      TRI_AppendCharStringBuffer(converter->_buffer, '{');
      QLJavascripterWalkList(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, '}');
      return; 
    case QLNodeValueParameterNumeric:
    case QLNodeValueParameterNamed:
      // TODO: 
      return;
    case QLNodeValueIdentifier:
      TRI_AppendStringStringBuffer(converter->_buffer, node->_value._stringValue);
      return;
    case QLNodeValueNamedValue:
      QLJavascripterConvert(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      QLJavascripterConvert(converter, rhs);
      return;
    case QLNodeReferenceCollectionAlias:
      if (converter->_prefix == 0) {
        TRI_AppendStringStringBuffer(converter->_buffer, "$['");
        TRI_AppendStringStringBuffer(converter->_buffer, node->_value._stringValue);
        TRI_AppendStringStringBuffer(converter->_buffer, "']");
      }
      else {
        TRI_AppendStringStringBuffer(converter->_buffer, "$['");
        TRI_AppendStringStringBuffer(converter->_buffer, converter->_prefix);
        TRI_AppendStringStringBuffer(converter->_buffer, "'].");
        TRI_AppendStringStringBuffer(converter->_buffer, node->_value._stringValue);
      }
      return;
    case QLNodeUnaryOperatorPlus:
    case QLNodeUnaryOperatorMinus:
    case QLNodeUnaryOperatorNot:
      TRI_AppendStringStringBuffer(converter->_buffer, QLAstNodeGetUnaryOperatorString(node->_type));
      QLJavascripterConvert(converter, lhs);
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
    case QLNodeBinaryOperatorIn:
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      QLJavascripterConvert(converter, lhs);
      TRI_AppendStringStringBuffer(converter->_buffer, QLAstNodeGetBinaryOperatorString(node->_type));
      QLJavascripterConvert(converter, rhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case QLNodeContainerMemberAccess:
      QLJavascripterConvert(converter, lhs);
      QLJavascripterWalkList(converter, rhs, '.', 1);
      return;
    case QLNodeContainerTernarySwitch:
      QLJavascripterConvert(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      QLJavascripterConvert(converter, rhs);
      return;
    case QLNodeControlFunctionCall:
      QLJavascripterConvert(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      QLJavascripterWalkList(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case QLNodeControlTernary:
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      QLJavascripterConvert(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, '?');
      QLJavascripterConvert(converter, rhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    default:
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an order by AST
////////////////////////////////////////////////////////////////////////////////

void QLJavascripterConvertOrder (QL_javascript_conversion_t* converter, 
                                 QL_ast_node_t* node) {
  QL_ast_node_t *lhs, *rhs, *start;

  if (node == 0) {
    return;
  }

  start = node;
  while (node != 0) {
    lhs = node->_lhs;
    TRI_AppendStringStringBuffer(converter->_buffer, "lhs=");
    converter->_prefix = "l";
    QLJavascripterConvert(converter, lhs); 
    TRI_AppendStringStringBuffer(converter->_buffer, ";rhs=");
    converter->_prefix = "r";
    QLJavascripterConvert(converter, lhs); 
    
    rhs = node->_rhs;
    if (rhs->_value._boolValue) {
      TRI_AppendStringStringBuffer(converter->_buffer, ";if(lhs<rhs)return -1;if(lhs>rhs)return 1;");
    } 
    else {
      TRI_AppendStringStringBuffer(converter->_buffer, ";if(lhs<rhs)return 1;if(lhs>rhs)return -1;");
    }
    if (node->_next == 0) {
      break;
    }
    node = node->_next;
  }
  TRI_AppendStringStringBuffer(converter->_buffer, "return 0;");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

