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

#include "VocBase/query-javascript.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Walk a horizontal list of elements and print them
////////////////////////////////////////////////////////////////////////////////

static void TRI_WalkListQueryJavascript (TRI_query_javascript_converter_t* converter, 
                                         const TRI_query_node_t* const node, 
                                         const char separator, 
                                         size_t counter) {
  TRI_query_node_t* next; 

  if (!node) { 
    return;
  }

  next = node->_next; 
 
  while (next) {
    if (counter++ > 0) {
      TRI_AppendCharStringBuffer(converter->_buffer, separator);
    }
    TRI_ConvertQueryJavascript(converter, next); 
    next = next->_next;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the to-Javascript conversion context
////////////////////////////////////////////////////////////////////////////////

TRI_query_javascript_converter_t* TRI_InitQueryJavascript (void) {
  TRI_string_buffer_t* buffer;
  TRI_query_javascript_converter_t* converter;

  converter = (TRI_query_javascript_converter_t*) 
    TRI_Allocate(sizeof(TRI_query_javascript_converter_t));
  if (!converter) { 
    return NULL;
  }

  // init
  converter->_buffer = NULL;
  converter->_prefix = NULL;

  buffer = (TRI_string_buffer_t*) TRI_Allocate(sizeof(TRI_string_buffer_t));
  if (!buffer) {
    TRI_Free(converter);
    return NULL;
  }

  TRI_InitStringBuffer(buffer);
  converter->_buffer = buffer;

  return converter;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the to-Javascript conversion text
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryJavascript (TRI_query_javascript_converter_t* converter) {
  TRI_FreeStringBuffer(converter->_buffer);
  TRI_Free(converter->_buffer);
  TRI_Free(converter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an expression AST
////////////////////////////////////////////////////////////////////////////////

void TRI_ConvertQueryJavascript (TRI_query_javascript_converter_t* converter, 
                                 const TRI_query_node_t* const node) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  char* escapedString;
  size_t outLength;

  if (!node) {
    return;
  }

  lhs = node->_lhs;
  rhs = node->_rhs;

  switch (node->_type) {
    case TRI_QueryNodeValueUndefined:
      TRI_AppendStringStringBuffer(converter->_buffer, "undefined");
      return;
    case TRI_QueryNodeValueNull:
      TRI_AppendStringStringBuffer(converter->_buffer, "null");
      return;
    case TRI_QueryNodeValueBool:
      TRI_AppendStringStringBuffer(converter->_buffer, 
                                   node->_value._boolValue ? "true" : "false");
      return;
    case TRI_QueryNodeValueString:
      TRI_AppendCharStringBuffer(converter->_buffer, '"');
      escapedString = TRI_EscapeUtf8String(node->_value._stringValue, 
                                           strlen(node->_value._stringValue), 
                                           false, 
                                           &outLength);
      if (escapedString) {
        TRI_AppendStringStringBuffer(converter->_buffer, escapedString);
        TRI_Free(escapedString); 
      }
      TRI_AppendCharStringBuffer(converter->_buffer, '"');
      return;
    case TRI_QueryNodeValueNumberInt:
      TRI_AppendInt64StringBuffer(converter->_buffer, node->_value._intValue);
      return;
    case TRI_QueryNodeValueNumberDouble:
      TRI_AppendDoubleStringBuffer(converter->_buffer, node->_value._doubleValue);
      return;
    case TRI_QueryNodeValueNumberDoubleString:
      TRI_AppendStringStringBuffer(converter->_buffer, node->_value._stringValue);
      return;
    case TRI_QueryNodeValueArray:
      TRI_AppendCharStringBuffer(converter->_buffer, '[');
      TRI_WalkListQueryJavascript(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ']');
      return; 
    case TRI_QueryNodeValueDocument:
      TRI_AppendCharStringBuffer(converter->_buffer, '{');
      TRI_WalkListQueryJavascript(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, '}');
      return; 
    case TRI_QueryNodeValueParameterNumeric:
    case TRI_QueryNodeValueParameterNamed:
      // TODO: 
      return;
    case TRI_QueryNodeValueIdentifier:
      TRI_AppendStringStringBuffer(converter->_buffer, 
                                   node->_value._stringValue);
      return;
    case TRI_QueryNodeValueNamedValue:
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      TRI_ConvertQueryJavascript(converter, rhs);
      return;
    case TRI_QueryNodeReferenceCollectionAlias:
      if (!converter->_prefix) {
        TRI_AppendStringStringBuffer(converter->_buffer, "$['");
        TRI_AppendStringStringBuffer(converter->_buffer, 
                                     node->_value._stringValue);
        TRI_AppendStringStringBuffer(converter->_buffer, "']");
      }
      else {
        TRI_AppendStringStringBuffer(converter->_buffer, "$['");
        TRI_AppendStringStringBuffer(converter->_buffer, 
                                     converter->_prefix);
        TRI_AppendStringStringBuffer(converter->_buffer, "'].");
        TRI_AppendStringStringBuffer(converter->_buffer, 
                                     node->_value._stringValue);
      }
      return;
    case TRI_QueryNodeUnaryOperatorPlus:
    case TRI_QueryNodeUnaryOperatorMinus:
    case TRI_QueryNodeUnaryOperatorNot:
      TRI_AppendStringStringBuffer(converter->_buffer, 
                                   TRI_QueryNodeGetUnaryOperatorString(node->_type));
      TRI_ConvertQueryJavascript(converter, lhs);
      return;
    case TRI_QueryNodeBinaryOperatorAnd: 
    case TRI_QueryNodeBinaryOperatorOr: 
    case TRI_QueryNodeBinaryOperatorIdentical: 
    case TRI_QueryNodeBinaryOperatorUnidentical: 
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
    case TRI_QueryNodeBinaryOperatorIn:
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_AppendStringStringBuffer(converter->_buffer, 
                                   TRI_QueryNodeGetBinaryOperatorString(node->_type));
      TRI_ConvertQueryJavascript(converter, rhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case TRI_QueryNodeContainerMemberAccess:
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_WalkListQueryJavascript(converter, rhs, '.', 1);
      return;
    case TRI_QueryNodeContainerTernarySwitch:
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      TRI_ConvertQueryJavascript(converter, rhs);
      return;
    case TRI_QueryNodeControlFunctionCall:
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      TRI_WalkListQueryJavascript(converter, rhs, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case TRI_QueryNodeControlTernary:
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      TRI_ConvertQueryJavascript(converter, lhs);
      TRI_AppendCharStringBuffer(converter->_buffer, '?');
      TRI_ConvertQueryJavascript(converter, rhs);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    default:
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an order by AST
////////////////////////////////////////////////////////////////////////////////

void TRI_ConvertOrderQueryJavascript (TRI_query_javascript_converter_t* converter, 
                                      const TRI_query_node_t* const node) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_query_node_t* current;

  if (!node) {
    return;
  }

  current = (TRI_query_node_t*) node;
  while (current) {
    lhs = current->_lhs;
    TRI_AppendStringStringBuffer(converter->_buffer, "lhs=");
    converter->_prefix = "l";
    TRI_ConvertQueryJavascript(converter, lhs); 
    TRI_AppendStringStringBuffer(converter->_buffer, ";rhs=");
    converter->_prefix = "r";
    TRI_ConvertQueryJavascript(converter, lhs); 
    
    rhs = current->_rhs;
    if (rhs->_value._boolValue) {
      TRI_AppendStringStringBuffer(converter->_buffer, ";if(lhs<rhs)return -1;if(lhs>rhs)return 1;");
    } 
    else {
      TRI_AppendStringStringBuffer(converter->_buffer, ";if(lhs<rhs)return 1;if(lhs>rhs)return -1;");
    }
    if (!current->_next) {
      break;
    }
    current = current->_next;
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

