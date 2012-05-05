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

#ifndef TRIAGENS_DURHAM_AHUACATL_CODEGEN_JS_H
#define TRIAGENS_DURHAM_AHUACATL_CODEGEN_JS_H 1

#include <BasicsC/common.h>
#include <BasicsC/associative.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/vector.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-conversions.h"

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

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a variable or function register number
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_aql_codegen_register_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief scope types used by the code generator
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_SCOPE_MAIN,
  TRI_AQL_SCOPE_LET,
  TRI_AQL_SCOPE_FOR,
  TRI_AQL_SCOPE_FOR_NESTED,
  TRI_AQL_SCOPE_FUNCTION,
  TRI_AQL_SCOPE_EXPAND
}
TRI_aql_codegen_scope_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief variable (name + register index) type used by code generator
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_variable_s {
  char* _name;
  TRI_aql_codegen_register_t _register;
}
TRI_aql_codegen_variable_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator scope
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_scope_s {
  TRI_string_buffer_t* _buffer;
  TRI_aql_codegen_scope_e _type;
  TRI_aql_codegen_register_t _listRegister;
  TRI_aql_codegen_register_t _keyRegister;
  TRI_aql_codegen_register_t _ownRegister;
  TRI_aql_codegen_register_t _resultRegister;
  TRI_associative_pointer_t _variables;
  const char* _variableName;
  const char* _name;
  char* _prefix; // prefix for variable names
}
TRI_aql_codegen_scope_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_js_s {  
  TRI_string_buffer_t _buffer;
  TRI_string_buffer_t _functionBuffer;
  TRI_vector_pointer_t _scopes;

  size_t _registerIndex;
  size_t _functionIndex;
  bool _error;
}
TRI_aql_codegen_js_t;

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
