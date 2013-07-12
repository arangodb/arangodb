////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser functionality
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

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init the lexer
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitParserAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_parser);

  Ahuacatllex_init(&context->_parser->_scanner);
  Ahuacatlset_extra(context, context->_parser->_scanner);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse & validate the query string
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseAql (TRI_aql_context_t* const context) {
  if (!TRI_StartScopeAql(context, TRI_AQL_SCOPE_MAIN)) {
    return false;
  }

  if (Ahuacatlparse(context)) {
    // lexing/parsing failed
    return false;
  }

  if (!TRI_EndScopeAql(context)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetErrorParseAql (TRI_aql_context_t* const context,
                           const char* const message,
                           const int line,
                           const int column) {
  char buffer[1024];
  char* region;

  assert(context);
  assert(context->_query);
  assert(message);

  region = TRI_GetContextErrorAql(context->_query, 
                                  context->_parser->_queryLength, 
                                  (size_t) line, 
                                  (size_t) column);

  if (region == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return;
  }

  // note: line numbers reported by bison/flex start at 1, columns start at 0
  snprintf(buffer,
           sizeof(buffer),
           "%d:%d %s near '%s'",
           line,
           column + 1,
           message,
           region);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, region);
  TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_PARSE, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push something on the stack
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushStackParseAql (TRI_aql_context_t* const context,
                            const void* const value) {
  assert(context);

  if (value == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL); \

    return NULL;
  }

  TRI_PushBackVectorPointer(&context->_parser->_stack, (void*) value);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop something from the stack
////////////////////////////////////////////////////////////////////////////////

void* TRI_PopStackParseAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_parser->_stack._length > 0);

  return TRI_RemoveVectorPointer(&context->_parser->_stack, context->_parser->_stack._length - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief peek at the end of the stack
////////////////////////////////////////////////////////////////////////////////

void* TRI_PeekStackParseAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_parser->_stack._length > 0);

  return context->_parser->_stack._buffer[context->_parser->_stack._length - 1];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new unique variable name
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetNameParseAql (TRI_aql_context_t* const context) {
  char buffer[16];
  int n;

  assert(context);

  n = snprintf(buffer, sizeof(buffer) - 1, "_%d", (int) ++context->_variableIndex);

  // register the string and return the copy of it
  return TRI_RegisterStringAql(context, buffer, (size_t) n, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
