////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, errors
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

#include "Ahuacatl/ahuacatl-error.h"

#include "Basics/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief error snippet length
////////////////////////////////////////////////////////////////////////////////

#define SNIPPET_LENGTH 32

////////////////////////////////////////////////////////////////////////////////
/// @brief error snippet suffix
////////////////////////////////////////////////////////////////////////////////

#define SNIPPET_SUFFIX "..."

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the error code registered last
////////////////////////////////////////////////////////////////////////////////

int TRI_GetErrorCodeAql (const TRI_aql_error_t* const error) {
  TRI_ASSERT(error);

  return error->_code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetErrorMessageAql (const TRI_aql_error_t* const error) {
  char* message;
  char buffer[256];
  int code;

  TRI_ASSERT(error);

  code = TRI_GetErrorCodeAql(error);

  if (code == TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  message = error->_message;

  if (message == NULL) {
    return NULL;
  }

  if (error->_data && (NULL != strstr(message, "%s"))) {
    int written = snprintf(buffer, sizeof(buffer), message, error->_data);

    return TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, (const char*) &buffer, (size_t) written);
  }

  return TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an error structure
////////////////////////////////////////////////////////////////////////////////

void TRI_InitErrorAql (TRI_aql_error_t* const error) {
  TRI_ASSERT(error);

  error->_code = TRI_ERROR_NO_ERROR;
  error->_data = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure, not freeing the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyErrorAql (TRI_aql_error_t* const error) {
  TRI_ASSERT(error);

  if (error->_data) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, error->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeErrorAql (TRI_aql_error_t* const error) {
  TRI_ASSERT(error);

  TRI_DestroyErrorAql(error);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, error);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a formatted query error message
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetContextErrorAql (const char* const query,
                              const size_t queryLength,
                              const size_t line,
                              const size_t column) {
  const char* p;
  char* q;
  char* result;
  char c;
  // note: line numbers reported by bison/flex start at 1, columns start at 0
  size_t offset;
  size_t currentLine = 1;
  size_t currentColumn = 0;

  TRI_ASSERT(query);

  p = query;
  while ((c = *p)) {
    if (currentLine > line || (currentLine >= line && currentColumn >= column)) {
      break;
    }

    if (c == '\n') {
      ++p;
      ++currentLine;
      currentColumn = 0;
    }
    else if (c == '\r') {
      ++p;
      ++currentLine;
      currentColumn = 0;

      if (*p == '\n') {
        ++p;
      }
    }
    else {
      ++currentColumn;
      ++p;
    }
  }

  // p is pointing at the position in the query the parse error occurred at
  TRI_ASSERT(p >= query);

  offset = (size_t) (p - query);

  if (queryLength < offset + SNIPPET_LENGTH) {
    return TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, query + offset, queryLength - offset);
  }

  q = result = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                       SNIPPET_LENGTH + strlen(SNIPPET_SUFFIX) + 1, false));

  if (result == NULL) {
    // out of memory
    return NULL;
  }

  // copy query part
  memcpy(q, query + offset, SNIPPET_LENGTH);
  q += SNIPPET_LENGTH;

  // copy ...
  memcpy(q, SNIPPET_SUFFIX, strlen(SNIPPET_SUFFIX));
  q += strlen(SNIPPET_SUFFIX);

  *q = '\0';

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
