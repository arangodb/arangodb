////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST to JS code generator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AHUACATL_AHUACATL__CODEGEN_H
#define ARANGODB_AHUACATL_AHUACATL__CODEGEN_H 1

#include "Basics/Common.h"
#include "Basics/associative.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Basics/string-buffer.h"
#include "Basics/vector.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a variable or function register number
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_aql_codegen_register_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief variable (name + register index) type used by code generator
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_variable_s {
  char* _name;                          // variable name
  TRI_aql_codegen_register_t _register; // the assigned register
  bool _isCachedSubquery;
}
TRI_aql_codegen_variable_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator scope
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_scope_s {
  TRI_string_buffer_t* _buffer;   // generated code
  TRI_aql_scope_e _type;          // scope type
  TRI_aql_codegen_register_t _listRegister;
  TRI_aql_codegen_register_t _keyRegister;
  TRI_aql_codegen_register_t _ownRegister;
  TRI_aql_codegen_register_t _resultRegister;
  TRI_aql_codegen_register_t _offsetRegister; // limit offset, limit
  TRI_aql_codegen_register_t _limitRegister;  // limit offset, limit
  TRI_aql_codegen_register_t _subqueryRegister;
  TRI_associative_pointer_t _variables; // list of variables in scope
  char const* _prefix;                  // prefix for variable names, used in FUNCTION scopes only
  TRI_aql_for_hint_t* _hint;            // generic hint
}
TRI_aql_codegen_scope_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief code generator
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_codegen_js_s {
  TRI_aql_context_t* _context;
  TRI_string_buffer_t _buffer;
  TRI_string_buffer_t _functionBuffer;
  TRI_vector_pointer_t _scopes;
  TRI_aql_codegen_register_t _registerIndex;
  TRI_aql_codegen_register_t _functionIndex;
  int _errorCode; // error number
  char* _errorValue; // error context string
}
TRI_aql_codegen_js_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generate Javascript code for the AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

char* TRI_GenerateCodeAql (TRI_aql_context_t* const,
                           size_t*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
