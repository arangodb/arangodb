////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST to JS code generator
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

#ifndef TRIAGENS_DURHAM_AHUACATL_AST_CODEGEN_JS_H
#define TRIAGENS_DURHAM_AHUACATL_AST_CODEGEN_JS_H 1

#include <BasicsC/common.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/vector.h>
#include <BasicsC/conversions.h>
#include <BasicsC/associative.h>

#include "Ahuacatl/ast-node.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  AQL_SCOPE_STANDALONE = 0,
  AQL_SCOPE_RESULT     = 1,
  AQL_SCOPE_COMPARE    = 2
}
TRI_aql_scope_type_e;

typedef struct TRI_aql_codegen_scope_s {
  TRI_aql_scope_type_e _type;
  TRI_string_buffer_t* _buffer;
  char* _funcName;
  char* _variablePrefix;
  size_t _forLoops;
  size_t _indent;
}
TRI_aql_codegen_scope_t;

typedef struct TRI_aql_codegen_s {
  TRI_string_buffer_t _buffer;
  TRI_vector_pointer_t _scopes;
  size_t _funcIndex;
  bool _error;
  char* _funcName;
}
TRI_aql_codegen_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator 
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_t* TRI_CreateCodegenAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the code generator 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCodegenAql (TRI_aql_codegen_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate Javascript code for the AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

char* TRI_GenerateCodeAql (const void* const);

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
