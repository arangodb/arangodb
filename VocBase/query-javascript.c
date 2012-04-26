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
#include "VocBase/query-base.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the 'undefined' string
////////////////////////////////////////////////////////////////////////////////

static inline const char* GetUndefinedString (void) {
  return "undefined";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the 'null' string
////////////////////////////////////////////////////////////////////////////////

static inline const char* GetNullString (void) {
  return "null";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the boolean value string for a node
////////////////////////////////////////////////////////////////////////////////

static inline const char* GetBoolString (const TRI_query_node_t* const node) {
  return (node->_value._boolValue ? "true" : "false");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the name of a unary operator function for a name
////////////////////////////////////////////////////////////////////////////////

static const char* GetUnaryOperatorFuncString (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeUnaryOperatorPlus:
      return "AQL_UNARY_PLUS";
    case TRI_QueryNodeUnaryOperatorMinus:
      return "AQL_UNARY_MINUS";
    case TRI_QueryNodeUnaryOperatorNot:
      return "AQL_LOGICAL_NOT";
    default:
      assert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the name of a binary operator function for a name
////////////////////////////////////////////////////////////////////////////////

static const char* GetBinaryOperatorFuncString (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeBinaryOperatorAnd: 
      return "AQL_LOGICAL_AND";
    case TRI_QueryNodeBinaryOperatorOr: 
      return "AQL_LOGICAL_OR";
    case TRI_QueryNodeBinaryOperatorEqual: 
      return "AQL_RELATIONAL_EQUAL";
    case TRI_QueryNodeBinaryOperatorUnequal: 
      return "AQL_RELATIONAL_UNEQUAL";
    case TRI_QueryNodeBinaryOperatorLess: 
      return "AQL_RELATIONAL_LESS";
    case TRI_QueryNodeBinaryOperatorGreater: 
      return "AQL_RELATIONAL_GREATER";
    case TRI_QueryNodeBinaryOperatorLessEqual:
      return "AQL_RELATIONAL_LESSEQUAL";
    case TRI_QueryNodeBinaryOperatorGreaterEqual:
      return "AQL_RELATIONAL_GREATEREQUAL";
    case TRI_QueryNodeBinaryOperatorIn:
      return "AQL_RELATIONAL_IN";
    case TRI_QueryNodeBinaryOperatorAdd:
      return "AQL_ARITHMETIC_PLUS";
    case TRI_QueryNodeBinaryOperatorSubtract:
      return "AQL_ARITHMETIC_MINUS";
    case TRI_QueryNodeBinaryOperatorMultiply:
      return "AQL_ARITHMETIC_TIMES";
    case TRI_QueryNodeBinaryOperatorDivide:
      return "AQL_ARITHMETIC_DIVIDE";
    case TRI_QueryNodeBinaryOperatorModulus:
      return "AQL_ARITHMETIC_MODULUS";
    default: 
      assert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Append the function code for a unary operator function
////////////////////////////////////////////////////////////////////////////////

static void AppendUnaryFunc (TRI_query_javascript_converter_t* converter,
                             const TRI_query_node_t* const node,
                             const TRI_query_node_t* const lhs,
                             TRI_associative_pointer_t* bindParameters) {
  // append function name 
  TRI_AppendStringStringBuffer(converter->_buffer, GetUnaryOperatorFuncString(node));
  TRI_AppendCharStringBuffer(converter->_buffer, '(');
  
  // append sole operand
  TRI_ConvertQueryJavascript(converter, lhs, bindParameters);

  // finalize
  TRI_AppendCharStringBuffer(converter->_buffer, ')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Append the function code for a binary operator function
////////////////////////////////////////////////////////////////////////////////

static void AppendBinaryFunc (TRI_query_javascript_converter_t* converter,
                              const TRI_query_node_t* const node,
                              const TRI_query_node_t* const lhs,
                              const TRI_query_node_t* const rhs,
                              TRI_associative_pointer_t* bindParameters) {
  // append function name 
  TRI_AppendStringStringBuffer(converter->_buffer, GetBinaryOperatorFuncString(node));
  TRI_AppendCharStringBuffer(converter->_buffer, '(');

  // append first operand
  TRI_ConvertQueryJavascript(converter, lhs, bindParameters);
  TRI_AppendCharStringBuffer(converter->_buffer, ',');

  // append second operand
  TRI_ConvertQueryJavascript(converter, rhs, bindParameters);

  // finalize
  TRI_AppendCharStringBuffer(converter->_buffer, ')');
}


static size_t WalkMembers (TRI_string_buffer_t* const buffer, 
                           const TRI_query_node_t* const node) {
  TRI_query_node_t* next = node->_next; 
  size_t num = 0;
 
  while (next) {
    TRI_AppendStringStringBuffer(buffer, ",'");
    TRI_AppendStringStringBuffer(buffer, next->_value._stringValue);
    TRI_AppendStringStringBuffer(buffer, "')");
    next = next->_next;
    ++num;
  }

  return num;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Walk a horizontal list of elements and print them
////////////////////////////////////////////////////////////////////////////////

static void MemberAccess (TRI_query_javascript_converter_t* converter, 
                          const TRI_query_node_t* const node,
                          TRI_associative_pointer_t* bindParameters) { 
  TRI_string_buffer_t* buffer;
  size_t length;
  size_t i;

  buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  if (!buffer) {
    return;
  }

  length = WalkMembers(buffer, node->_rhs);

  for (i = 0; i < length; ++i) {
    TRI_AppendStringStringBuffer(converter->_buffer, "AQL_ACCESS_MEMBER(");
  }
  TRI_ConvertQueryJavascript(converter, node->_lhs, bindParameters);
  TRI_AppendStringStringBuffer(converter->_buffer, buffer->_buffer);

  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Walk a horizontal list of elements and print them
////////////////////////////////////////////////////////////////////////////////

static void WalkList (TRI_query_javascript_converter_t* converter, 
                      const TRI_query_node_t* const node, 
                      TRI_associative_pointer_t* bindParameters,
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

    TRI_ConvertQueryJavascript(converter, next, bindParameters); 
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
    TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_query_javascript_converter_t));

  if (!converter) { 
    return NULL;
  }

  // init
  converter->_buffer = NULL;
  converter->_prefix = NULL;

  buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  if (!buffer) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, converter);
    return NULL;
  }

  converter->_buffer = buffer;

  return converter;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the to-Javascript conversion text
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryJavascript (TRI_query_javascript_converter_t* converter) {
  assert(converter);
  assert(converter->_buffer);
  
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, converter->_buffer);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, converter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an expression AST
////////////////////////////////////////////////////////////////////////////////

void TRI_ConvertQueryJavascript (TRI_query_javascript_converter_t* converter, 
                                 const TRI_query_node_t* const node,
                                 TRI_associative_pointer_t* bindParameters) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_bind_parameter_t* parameter;
  char* escapedString;
  size_t outLength;

  assert(converter);
  assert(bindParameters);

  if (!node) {
    return;
  }

  lhs = node->_lhs;
  rhs = node->_rhs;

  switch (node->_type) {
    case TRI_QueryNodeValueUndefined:
      TRI_AppendStringStringBuffer(converter->_buffer, GetUndefinedString());
      return;
    case TRI_QueryNodeValueNull:
      TRI_AppendStringStringBuffer(converter->_buffer, GetNullString());
      return;
    case TRI_QueryNodeValueBool:
      TRI_AppendStringStringBuffer(converter->_buffer, GetBoolString(node));
      return;
    case TRI_QueryNodeValueString:
      TRI_AppendCharStringBuffer(converter->_buffer, '"');
      escapedString = TRI_EscapeUtf8String(
        node->_value._stringValue, 
        strlen(node->_value._stringValue), 
        false, 
        &outLength
      );
      if (escapedString) {
        TRI_AppendStringStringBuffer(converter->_buffer, escapedString);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, escapedString); 
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
      WalkList(converter, rhs, bindParameters, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ']');
      return; 
    case TRI_QueryNodeValueDocument:
      TRI_AppendCharStringBuffer(converter->_buffer, '{');
      WalkList(converter, rhs, bindParameters, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, '}');
      return; 
    case TRI_QueryNodeValueParameterNamed:
      parameter = (TRI_bind_parameter_t*) 
        TRI_LookupByKeyAssociativePointer(bindParameters, 
                                          node->_value._stringValue);
      assert(parameter);  
      TRI_StringifyJson(converter->_buffer, parameter->_data); 
      return;
    case TRI_QueryNodeValueIdentifier:
      TRI_AppendStringStringBuffer(converter->_buffer, 
                                   node->_value._stringValue);
      return;
    case TRI_QueryNodeValueNamedValue:
      TRI_ConvertQueryJavascript(converter, lhs, bindParameters);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      TRI_ConvertQueryJavascript(converter, rhs, bindParameters);
      return;
    case TRI_QueryNodeReferenceCollectionAlias:
      if (!converter->_prefix) {
        TRI_AppendStringStringBuffer(converter->_buffer, "$['");
        assert(node->_value._stringValue);
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
      AppendUnaryFunc(converter, node, lhs, bindParameters);
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
      AppendBinaryFunc(converter, node, lhs, rhs, bindParameters);
      return;
    case TRI_QueryNodeContainerMemberAccess:
      MemberAccess(converter, node, bindParameters);
      return;
    case TRI_QueryNodeControlFunctionCall:
      TRI_ConvertQueryJavascript(converter, lhs, bindParameters);
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      WalkList(converter, rhs, bindParameters, ',', 0);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case TRI_QueryNodeContainerTernarySwitch:
      TRI_AppendStringStringBuffer(converter->_buffer, "(r?");
      TRI_ConvertQueryJavascript(converter, lhs, bindParameters);
      TRI_AppendCharStringBuffer(converter->_buffer, ':');
      TRI_ConvertQueryJavascript(converter, rhs, bindParameters);
      TRI_AppendCharStringBuffer(converter->_buffer, ')');
      return;
    case TRI_QueryNodeControlTernary:
      TRI_AppendCharStringBuffer(converter->_buffer, '(');
      TRI_AppendStringStringBuffer(converter->_buffer, "AQL_TYPEWEIGHT(r=(");
      TRI_ConvertQueryJavascript(converter, lhs, bindParameters);
      TRI_AppendStringStringBuffer(converter->_buffer, "))===AQL_TYPEWEIGHT_UNDEFINED?undefined:");
      TRI_ConvertQueryJavascript(converter, rhs, bindParameters);
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
                                      const TRI_query_node_t* const node,
                                      TRI_associative_pointer_t* bindParameters) {
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_query_node_t* current;

  if (!node) {
    return;
  }

  current = (TRI_query_node_t*) node;

  // iterate over all sort dimensions
  while (current) {
    // set the initial lhs and rhs values
    assert(current->_lhs);
    assert(current->_rhs);

    lhs = current->_lhs;
    TRI_AppendStringStringBuffer(converter->_buffer, "lhs=");
    converter->_prefix = "l";
    TRI_ConvertQueryJavascript(converter, lhs, bindParameters); 
    TRI_AppendStringStringBuffer(converter->_buffer, ";rhs=");
    converter->_prefix = "r";
    TRI_ConvertQueryJavascript(converter, lhs, bindParameters); 
    
    rhs = current->_rhs;

    if (rhs->_value._boolValue) {
      // sort ascending
      TRI_AppendStringStringBuffer(
        converter->_buffer, 
        ";if(AQL_RELATIONAL_LESS(lhs,rhs))return -1;if(AQL_RELATIONAL_GREATER(lhs,rhs))return 1;"
      );
    } 
    else {
      // sort descending
      TRI_AppendStringStringBuffer(
        converter->_buffer, 
        ";if(AQL_RELATIONAL_LESS(lhs,rhs))return 1;if(AQL_RELATIONAL_GREATER(lhs,rhs))return -1;"
      );
    }
    
    // next sort dimension
    current = current->_next;
  }

  // finally return 0 if all values are the same
  TRI_AppendStringStringBuffer(converter->_buffer, "return 0;");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create javascript function code for a query part
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetFunctionCodeQueryJavascript (const TRI_query_node_t* const node,
                                          TRI_associative_pointer_t* bindParameters) {
  TRI_query_javascript_converter_t* js;
  char* function = NULL;
  
  assert(node);
  js = TRI_InitQueryJavascript();

  if (js) {
    TRI_AppendStringStringBuffer(js->_buffer, "(function($) { return ");
    TRI_ConvertQueryJavascript(js, node, bindParameters);
    TRI_AppendStringStringBuffer(js->_buffer, " })");

    function = TRI_DuplicateString(js->_buffer->_buffer);
    TRI_FreeQueryJavascript(js);
  }

  return function;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create javascript function code for the order part of a query
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetOrderFunctionCodeQueryJavascript (const TRI_query_node_t* const node,
                                               TRI_associative_pointer_t* bindParameters) {
  TRI_query_javascript_converter_t* js;
  char* function = NULL;
  
  assert(node);
  assert(node->_next);

  js = TRI_InitQueryJavascript();
  if (js) {
    TRI_AppendStringStringBuffer(js->_buffer, "(function($) { var lhs, rhs; ");
    TRI_ConvertOrderQueryJavascript(js, node->_next, bindParameters);
    TRI_AppendStringStringBuffer(js->_buffer, " })");

    function = TRI_DuplicateString(js->_buffer->_buffer);
    TRI_FreeQueryJavascript(js);
  }

  return function;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

