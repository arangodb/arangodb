////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, error handling
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

#include "Aql/error.h"

#include "BasicsC/tri-strings.h"

using namespace triagens::aql;

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
/// @brief get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* query_error_t::getMessage () const {
  if (getCode() == TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  if (_message == nullptr) {
    return nullptr;
  }

  if (_data && (nullptr != strstr(_message, "%s"))) {
    char buffer[256];

    int written = snprintf(buffer, sizeof(buffer), _message, _data);

    return TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, (const char*) &buffer, (size_t) written);
  }

  return TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, _message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a formatted query error message
////////////////////////////////////////////////////////////////////////////////

char* query_error_t::getContextError (char const* query,
                                      size_t queryLength,
                                      size_t line,
                                      size_t column) const {
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

  if (result == nullptr) {
    // out of memory
    return nullptr;
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
