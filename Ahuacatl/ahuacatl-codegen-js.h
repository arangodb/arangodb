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
/// @brief internal function types
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_aql_codegen_register_t;

typedef enum {
  AQL_FUNCTION_STANDALONE, 
  AQL_FUNCTION_COMPARE
}
TRI_aql_codegen_function_type_e; 

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator internal function struct
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_functionx_s {
  TRI_string_buffer_t _buffer;
  size_t _index;
  size_t _forCount;
  char* _prefix;
}
TRI_aql_codegen_functionx_t;


typedef enum {
  TRI_AQL_SCOPE_MAIN,
  TRI_AQL_SCOPE_LET,
  TRI_AQL_SCOPE_FOR,
  TRI_AQL_SCOPE_FOR_NESTED,
  TRI_AQL_SCOPE_FUNCTION
}
TRI_aql_codegen_scope_e;

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
  char* _prefix;
}
TRI_aql_codegen_scope_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator struct
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_js_s {  
  TRI_string_buffer_t _buffer;
  TRI_vector_pointer_t _functions;
  TRI_vector_pointer_t _scopes;

  size_t _registerIndex;
  size_t _functionIndex;
  bool _error;
  size_t _last;
}
TRI_aql_codegen_js_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory associated with a code generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeneratorAql (TRI_aql_codegen_js_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_js_t* TRI_CreateGeneratorAql (void);

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

TRI_aql_codegen_js_t* TRI_GenerateCodeAql (const void* const);

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
