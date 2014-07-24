////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query parser
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

#include "Aql/Parser.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parser
////////////////////////////////////////////////////////////////////////////////

Parser::Parser (Query* query)
  : _query(query),
    _scanner(nullptr),
    _buffer(query->queryString()),
    _remainingLength(query->queryLength()),
    _offset(0),
    _marker(nullptr),
    _uniqueId(0),
    _stack() {
  
  Aqllex_init(&_scanner);
  Aqlset_extra(this, _scanner);

  _stack.reserve(16);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parser
////////////////////////////////////////////////////////////////////////////////

Parser::~Parser () {
  Aqllex_destroy(_scanner);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parse the query
////////////////////////////////////////////////////////////////////////////////

bool Parser::parse () {
  auto scopes = ast()->scopes();
  scopes->start(AQL_SCOPE_MAIN);

  if (Aqlparse(this)) {
    // lexing/parsing failed
    return false;
  }
  
  scopes->endCurrent();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new unique name
////////////////////////////////////////////////////////////////////////////////
  
char* Parser::generateName () {
  char buffer[16];
  int n = snprintf(buffer, sizeof(buffer) - 1, "_%d", (int) ++_uniqueId);

  // register the string and return a copy of it
  return registerString(buffer, static_cast<size_t>(n), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error, position is specified as line / column
////////////////////////////////////////////////////////////////////////////////

void Parser::registerError (char const* message,
                            int line,
                            int column) {
  TRI_ASSERT(message != nullptr);

  // extract the query string part where the error happened
  std::string const region(_query->extractRegion(line, column));

  // note: line numbers reported by bison/flex start at 1, columns start at 0
  char buffer[512];
  snprintf(buffer,
           sizeof(buffer),
           "%s near '%s' at position %d:%d",
           message,
           region.c_str(),
           line,
           column + 1);

  _query->registerError(TRI_ERROR_QUERY_PARSE, std::string(buffer));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a non-parse error
////////////////////////////////////////////////////////////////////////////////

void Parser::registerError (int code,
                            char const* message) {
 
  if (message == nullptr) { 
    _query->registerError(code);
  }
  else {
    _query->registerError(code, std::string(message));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push a temporary value on the parser's stack
////////////////////////////////////////////////////////////////////////////////

void Parser::pushStack (void* value) {
  _stack.push_back(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop a temporary value from the parser's stack
////////////////////////////////////////////////////////////////////////////////
        
void* Parser::popStack () {
  TRI_ASSERT(! _stack.empty());

  void* result = _stack.back();
  _stack.pop_back();
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief peek at a temporary value from the parser's stack
////////////////////////////////////////////////////////////////////////////////
        
void* Parser::peekStack () {
  TRI_ASSERT(! _stack.empty());

  return _stack.back();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
