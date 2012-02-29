////////////////////////////////////////////////////////////////////////////////
/// @brief AST dump functions
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

#include "QL/formatter.h" 

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Increase indentation
////////////////////////////////////////////////////////////////////////////////

void QLFormatterIndentationInc (QL_formatter_t* formatter) {
  formatter->_indentLevel++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Decrease indentation
////////////////////////////////////////////////////////////////////////////////

void QLFormatterIndentationDec (QL_formatter_t* formatter) {
  formatter->_indentLevel--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print current indentation
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintIndentation (QL_formatter_t* formatter) {
  unsigned int i=0;

  for (; i < formatter->_indentLevel; i++) {
     printf("  ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print start of a block
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintBlockStart (QL_formatter_t* formatter, const char* name) {
  QLFormatterPrintIndentation(formatter);
  printf("%s {\n",name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print end of a block
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintBlockEnd (QL_formatter_t* formatter, const char* name) {
  QLFormatterPrintIndentation(formatter);
  printf("}\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print an indented int value
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintInt (QL_formatter_t* formatter, const char* name, uint64_t value) {
  QLFormatterPrintIndentation(formatter);
  printf("%s: %lu\n",name,(unsigned long) value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print an indented double value
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintDouble (QL_formatter_t* formatter, const char* name, double value) {
  QLFormatterPrintIndentation(formatter);
  printf("%s: %f\n",name,value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Print an indented string value
////////////////////////////////////////////////////////////////////////////////

void QLFormatterPrintStr (QL_formatter_t* formatter, const char* name, const char* value) {
  QLFormatterPrintIndentation(formatter);
  printf("%s: %s\n",name,value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Recursively dump an AST
////////////////////////////////////////////////////////////////////////////////

void QLFormatterDump (TRI_query_node_t* node, QL_formatter_t* formatter) {
  const char* name;
  TRI_query_node_type_e type;
  TRI_query_node_t* lhs;
  TRI_query_node_t* rhs;
  TRI_query_node_t* next;

  if (!node) {
    return;
  }

  type   = node->_type;
  name   = TRI_QueryNodeGetName(type);

  if (type == TRI_QueryNodeContainerList) {
    next   = node->_next;
    QLFormatterPrintBlockStart(formatter,name);
    QLFormatterIndentationInc(formatter);
    while (next) {
      QLFormatterDump(next,formatter);
      next = next->_next;
    }
    QLFormatterIndentationDec(formatter);
    QLFormatterPrintBlockEnd(formatter,"");
    return;
  }

  QLFormatterPrintBlockStart(formatter,name);
  QLFormatterIndentationInc(formatter);
  
  if (type == TRI_QueryNodeValueString || 
      type == TRI_QueryNodeValueNumberDoubleString || 
      type == TRI_QueryNodeValueIdentifier || 
      type == TRI_QueryNodeValueParameterNamed || 
      type == TRI_QueryNodeReferenceCollectionAlias) { 
    QLFormatterPrintStr(formatter,"value", node->_value._stringValue);
  } else if (type == TRI_QueryNodeValueNumberInt) { 
    QLFormatterPrintInt(formatter,"value", node->_value._intValue);
  } else if (type == TRI_QueryNodeValueNumberDouble) { 
    QLFormatterPrintDouble(formatter,"value", node->_value._doubleValue);
  } else if (type == TRI_QueryNodeValueBool) {
    QLFormatterPrintStr(formatter,"value", node->_value._boolValue ? "true" : "false");
  } else if (type == TRI_QueryNodeValueOrderDirection) {
    QLFormatterPrintStr(formatter,"value", node->_value._boolValue ? "asc" : "desc");
  } else if (type == TRI_QueryNodeValueParameterNumeric) {
    QLFormatterPrintInt(formatter,"value", node->_value._intValue);
  }

  lhs    = node->_lhs;
  if (lhs) {
    QLFormatterDump(lhs, formatter);
  }

  rhs    = node->_rhs;
  if (rhs) { 
    QLFormatterDump(rhs, formatter);
  }

  QLFormatterIndentationDec(formatter);
  QLFormatterPrintBlockEnd(formatter,"");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

