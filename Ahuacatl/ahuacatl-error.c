////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, errors
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

#include "Ahuacatl/ahuacatl-error.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief error snippet length
////////////////////////////////////////////////////////////////////////////////

#define SNIPPET_LENGTH 32

////////////////////////////////////////////////////////////////////////////////
/// @brief error snippet suffix
////////////////////////////////////////////////////////////////////////////////

#define SNIPPET_SUFFIX "..."

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
/// @brief get the error code registered last
////////////////////////////////////////////////////////////////////////////////

int TRI_GetErrorCodeAql (const TRI_aql_error_t* const error) {
  assert(error);
  
  return error->_code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetErrorMessageAql (const TRI_aql_error_t* const error) {
  char* message = NULL;
  char buffer[1024];
  int code;

  assert(error);
  code = TRI_GetErrorCodeAql(error);
  if (!code) {
    return NULL;
  }

  message = error->_message;
  if (!message) {
    return NULL;
  }

  if (error->_data && (NULL != strstr(message, "%s"))) {
    snprintf(buffer, sizeof(buffer), message, error->_data);
    return TRI_DuplicateString((const char*) &buffer);
  }

  return TRI_DuplicateString(message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an error structure
////////////////////////////////////////////////////////////////////////////////

void TRI_InitErrorAql (TRI_aql_error_t* const error) {
  assert(error);

  error->_code = 0;
  error->_data = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure, not freeing the pointer
////////////////////////////////////////////////////////////////////////////////
  
void TRI_DestroyErrorAql (TRI_aql_error_t* const error) {
  assert(error);
  
  if (error->_data) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, error->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_FreeErrorAql (TRI_aql_error_t* const error) {
  assert(error);

  TRI_DestroyErrorAql(error);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, error);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a formatted query error message
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetContextErrorAql (const char* const query, const size_t line, const size_t column) {
  size_t currentLine = 1;
  size_t currentColumn = 1;
  const char* p = query;
  size_t offset;
  char c;
 
  while ((c = *p++)) {
    if (c == '\n') {
      ++currentLine;
      currentColumn = 0;
    }
    else if (c == '\r') {
      if (*p == '\n') {
        ++currentLine;
        currentColumn = 0;
        p++;
      }
    }

    ++currentColumn;

    if (currentLine >= line && currentColumn >= column) {
      break;
    }
  }

  offset = p - query;
  if (strlen(query) < offset + SNIPPET_LENGTH) {
    return TRI_DuplicateString2(query + offset, strlen(query) - offset);
  }

  return TRI_Concatenate2String(TRI_DuplicateString2(query + offset, SNIPPET_LENGTH), SNIPPET_SUFFIX);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
