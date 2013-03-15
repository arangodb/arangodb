////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, conversions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_AHUACATL_AHUACATL_CONVERSIONS_H
#define TRIAGENS_AHUACATL_AHUACATL_CONVERSIONS_H 1

#include "BasicsC/common.h"

#include "Ahuacatl/ahuacatl-ast-node.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_aql_context_s;
struct TRI_string_buffer_s;

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

struct TRI_json_s* TRI_NodeJsonAql (struct TRI_aql_context_s* const,
                                    const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a value node from a json struct
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_JsonNodeAql (struct TRI_aql_context_s* const,
                                 const struct TRI_json_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a value node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValueJavascriptAql (struct TRI_string_buffer_s* const,
                             const TRI_aql_value_t* const,
                             const TRI_aql_value_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_NodeJavascriptAql (struct TRI_string_buffer_s* const,
                            const TRI_aql_node_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a value node to a string representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValueStringAql (struct TRI_string_buffer_s* const,
                         const TRI_aql_value_t* const,
                         const TRI_aql_value_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its string representation, used for printing it
////////////////////////////////////////////////////////////////////////////////

bool TRI_NodeStringAql (struct TRI_string_buffer_s* const,
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
