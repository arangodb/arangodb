////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser
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

#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parser
////////////////////////////////////////////////////////////////////////////////

TRI_aql_parser_t* TRI_CreateParserAql (const char* const query,
                                       const size_t queryLength) {
  TRI_aql_parser_t* parser;
  int res;

  assert(query);

  parser = (TRI_aql_parser_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_parser_t), false);

  if (parser == NULL) {
    return NULL;
  }

  res = TRI_InitVectorPointer2(&parser->_scopes, TRI_UNKNOWN_MEM_ZONE, 4);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parser);

    return NULL;
  }

  res = TRI_InitVectorPointer2(&parser->_stack, TRI_UNKNOWN_MEM_ZONE, 4);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&parser->_scopes);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parser);

    return NULL;
  }

  parser->_buffer = (char*) query;
  parser->_queryLength = queryLength;
  parser->_length = (size_t) queryLength;

  return parser;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a parse context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeParserAql (TRI_aql_parser_t* const parser) {
  if (parser != NULL) {
    TRI_DestroyVectorPointer(&parser->_scopes);
    TRI_DestroyVectorPointer(&parser->_stack);

    // free lexer
    Ahuacatllex_destroy(parser->_scanner);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parser);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
