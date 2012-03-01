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

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_JAVASCRIPT_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_JAVASCRIPT_H 1

#include <BasicsC/string-buffer.h>
#include <BasicsC/strings.h>
#include <BasicsC/associative.h>

#include "VocBase/query-node.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief to-Javascript conversion context
////////////////////////////////////////////////////////////////////////////////

typedef struct QL_query_javascript_converter_s {
  TRI_string_buffer_t* _buffer;
  char*                _prefix;
}
TRI_query_javascript_converter_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the to-Javascript conversion context
////////////////////////////////////////////////////////////////////////////////

TRI_query_javascript_converter_t* TRI_InitQueryJavascript (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the to-Javascript conversion text
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryJavascript (TRI_query_javascript_converter_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an expression AST
////////////////////////////////////////////////////////////////////////////////

void TRI_ConvertQueryJavascript (TRI_query_javascript_converter_t*, 
                                 const TRI_query_node_t* const,
                                 TRI_associative_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a javascript string by recursively walking an order by AST
////////////////////////////////////////////////////////////////////////////////

void TRI_ConvertOrderQueryJavascript (TRI_query_javascript_converter_t*, 
                                      const TRI_query_node_t* const,
                                      TRI_associative_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create javascript function code for a query part
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetFunctionCodeQueryJavascript (const TRI_query_node_t* const,
                                          TRI_associative_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create javascript function code for the order part of a query
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetOrderFunctionCodeQueryJavascript (const TRI_query_node_t* const,
                                               TRI_associative_pointer_t*);

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

