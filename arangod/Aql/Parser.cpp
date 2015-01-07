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
#include "Aql/AstNode.h"
#include "Aql/QueryResult.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parser
////////////////////////////////////////////////////////////////////////////////

Parser::Parser (Query* query) 
  : _query(query),
    _ast(query->ast()),
    _scanner(nullptr),
    _buffer(query->queryString()),
    _remainingLength(query->queryLength()),
    _offset(0),
    _marker(nullptr),
    _stack(),
    _type(AQL_QUERY_READ) {
  
  _stack.reserve(16);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parser
////////////////////////////////////////////////////////////////////////////////

Parser::~Parser () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief set data for write queries
////////////////////////////////////////////////////////////////////////////////

bool Parser::configureWriteQuery (QueryType type,
                                  AstNode const* collectionNode,
                                  AstNode* optionNode,
                                  char const* newOld,
                                  char const* varInto,
                                  char const* varReturn) {
  TRI_ASSERT(type != AQL_QUERY_READ);

  // check if we currently are in a subquery
  if (_ast->isInSubQuery()) {
    // data modification not allowed in sub-queries
    _query->registerError(TRI_ERROR_QUERY_MODIFY_IN_SUBQUERY);
    return false;
  }

  // check current query type
  if (_type != AQL_QUERY_READ) {
    // already a data-modification query, cannot have two data-modification operations in one query
    _query->registerError(TRI_ERROR_QUERY_MULTI_MODIFY);
    return false;
  }

  // now track which collection is going to be modified
  _ast->setWriteCollection(collectionNode);

  if (optionNode != nullptr && ! optionNode->isConstant()) {
    _query->registerError(TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS);
  }

  // register query type 
  _type = type;

  switch(_type) {
  case AQL_QUERY_READ:
    break;
  case AQL_QUERY_REMOVE:
    if (newOld != nullptr) {
      if (!TRI_CaseEqualString(newOld, "OLD")) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "OLD expected");
        return false;
      }
      if (!TRI_CaseEqualString(varInto, varReturn)) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "WITH thisVariable RETURN thisVariable expected.");
        return false;
      }
    }
    break;
  case AQL_QUERY_INSERT:
    if (newOld != nullptr) {
      if (!TRI_CaseEqualString(newOld, "NEW")) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "NEW expected");
        return false;
      }
      if (!TRI_CaseEqualString(varInto, varReturn)) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "WITH thisVariable RETURN thisVariable expected.");
        return false;
      }
      
    }
    break;
  case AQL_QUERY_UPDATE:
  case AQL_QUERY_REPLACE:
    if (newOld != nullptr) {
      if (!TRI_CaseEqualString(newOld, "OLD") && !TRI_CaseEqualString(newOld, "NEW")) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "NEW|OLD expected");
        return false;
      }
      if (!TRI_CaseEqualString(varInto, varReturn)) {
        _query->registerError(TRI_ERROR_QUERY_PARSE, "WITH thisVariable RETURN thisVariable expected.");
        return false;
      }
    }
    break;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse the query
////////////////////////////////////////////////////////////////////////////////

QueryResult Parser::parse (bool withDetails) {
  char const* q = queryString();
  if (q == nullptr || *q == '\0') {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_EMPTY);
  }

  // start main scope
  auto scopes = _ast->scopes();
  scopes->start(AQL_SCOPE_MAIN);
  
  Aqllex_init(&_scanner);
  Aqlset_extra(this, _scanner);

  try {
    // parse the query string
    if (Aqlparse(this)) {
      // lexing/parsing failed
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_PARSE);
    }

    Aqllex_destroy(_scanner);
  }
  catch (...) {
    Aqllex_destroy(_scanner);
    throw;
  }

  // end main scope
  scopes->endCurrent();

  TRI_ASSERT(scopes->numActive() == 0);

  QueryResult result;

  if (withDetails) {
    result.collectionNames = _query->collectionNames();
    result.bindParameters  = _ast->bindParameters();
    result.json            = _ast->toJson(TRI_UNKNOWN_MEM_ZONE, false);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error, position is specified as line / column
////////////////////////////////////////////////////////////////////////////////

void Parser::registerParseError (int errorCode,
                                 char const* format,
                                 char const* data,
                                 int line,
                                 int column) {
  char buffer[512];
  snprintf(buffer,
           sizeof(buffer),
           format,
           data);

  return registerParseError(errorCode, buffer, line, column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error, position is specified as line / column
////////////////////////////////////////////////////////////////////////////////

void Parser::registerParseError (int errorCode,
                                 char const* data,
                                 int line,
                                 int column) {
  TRI_ASSERT(errorCode != TRI_ERROR_NO_ERROR);
  TRI_ASSERT(data != nullptr);

  // extract the query string part where the error happened
  std::string const region(_query->extractRegion(line, column));

  // note: line numbers reported by bison/flex start at 1, columns start at 0
  std::stringstream errorMessage;
  errorMessage
    << data
    << std::string(" near '")
    << region 
    << std::string("' at position ")
    << line
    << std::string(":")
    << (column + 1);
  
  if (_query->verboseErrors()) {
    errorMessage 
      << std::endl
      << _query->queryString()
      << std::endl;

    // create a neat pointer to the location of the error.
    size_t i;
    for (i = 0; i + 1 < (size_t) column; i++) {
      errorMessage << ' ';
    }
    if (i > 0) {
      errorMessage << '^';
    }
    errorMessage << '^'
                 << '^'
                 << std::endl;
  }

  registerError(errorCode, errorMessage.str().c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a non-parse error
////////////////////////////////////////////////////////////////////////////////

void Parser::registerError (int errorCode,
                            char const* data) {
  _query->registerError(errorCode, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a warning
////////////////////////////////////////////////////////////////////////////////

void Parser::registerWarning (int errorCode,
                              char const* data,
                              int line,
                              int column) {
  // ignore line and column for now
  _query->registerWarning(errorCode, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push an AstNode into the list element on top of the stack
////////////////////////////////////////////////////////////////////////////////

void Parser::pushList (AstNode* node) {
  auto list = static_cast<AstNode*>(peekStack());
  TRI_ASSERT(list->type == NODE_TYPE_ARRAY);
  list->addMember(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push an AstNode into the array element on top of the stack
////////////////////////////////////////////////////////////////////////////////

void Parser::pushArray (char const* attributeName,
                        AstNode* node) {
  auto array = static_cast<AstNode*>(peekStack());
  TRI_ASSERT(array->type == NODE_TYPE_OBJECT);
  auto element = _ast->createNodeArrayElement(attributeName, node);
  array->addMember(element);
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
