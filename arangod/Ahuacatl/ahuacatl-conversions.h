////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, conversions
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

#ifndef TRIAGENS_DURHAM_AHUACATL_CONVERSIONS_H
#define TRIAGENS_DURHAM_AHUACATL_CONVERSIONS_H 1

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/json.h>
#include <BasicsC/string-buffer.h>

#include "Ahuacatl/ahuacatl-ast-node.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a json struct from a value node
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_NodeJsonAql (TRI_aql_context_t* const,
                             const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a value node from a json struct
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_JsonNodeAql (TRI_aql_context_t* const,
                                 const TRI_json_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a value node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValueJavascriptAql (TRI_string_buffer_t* const, 
                             const TRI_aql_value_t* const,
                             const TRI_aql_value_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_NodeJavascriptAql (TRI_string_buffer_t* const, 
                            const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a value node to a string representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValueStringAql (TRI_string_buffer_t* const, 
                         const TRI_aql_value_t* const,
                         const TRI_aql_value_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its string representation, used for printing it
////////////////////////////////////////////////////////////////////////////////

bool TRI_NodeStringAql (TRI_string_buffer_t* const , 
                        const TRI_aql_node_t* const);

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
