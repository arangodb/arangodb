////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query context
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

#include "Aql/Query.h"
#include "Aql/Parser.h"
#include "BasicsC/json.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

Query::Query (TRI_vocbase_t* vocbase,
              char const* queryString,
              size_t queryLength,
              TRI_json_t* bindParameters)
  : _vocbase(vocbase),
    _queryString(queryString),
    _queryLength(queryLength),
    _type(AQL_QUERY_READ),
    _bindParameters(bindParameters),
    _error() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a query
////////////////////////////////////////////////////////////////////////////////

Query::~Query () {
  if (_bindParameters != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _bindParameters);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a region from the query
////////////////////////////////////////////////////////////////////////////////

std::string Query::extractRegion (int line, 
                                  int column) const {
  // note: line numbers reported by bison/flex start at 1, columns start at 0
  int currentLine   = 1;
  int currentColumn = 0;

  char c;
  char const* p = _queryString;

  while ((c = *p)) {
    if (currentLine > line || 
        (currentLine >= line && currentColumn >= column)) {
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

      // eat a following newline
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
  TRI_ASSERT(p >= _queryString);

  size_t offset = static_cast<size_t>(p - _queryString);

  static int const   SNIPPET_LENGTH = 32;
  static char const* SNIPPET_SUFFIX = "...";

  if (_queryLength < offset + SNIPPET_LENGTH) {
    // return a copy of the region
    return std::string(_queryString + offset, _queryLength - offset);
  }

  // copy query part
  std::string result(_queryString + offset, SNIPPET_LENGTH);
  result.append(SNIPPET_SUFFIX);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void Query::registerError (int errorCode,
                           std::string const& explanation) {
  TRI_ASSERT(errorCode != TRI_ERROR_NO_ERROR);

  if (_error.code != TRI_ERROR_NO_ERROR) {
    // there's already an error registered. do not overwrite it
    return;
  }

  _error.code        = errorCode;
  _error.explanation = explanation;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void Query::registerError (int errorCode) {
  registerError(errorCode, "");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

void Query::execute () {
  // TODO 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query
////////////////////////////////////////////////////////////////////////////////

ParseResult Query::parse () {
  Parser parser(this);
  
  return parser.parse();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

void Query::explain () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
