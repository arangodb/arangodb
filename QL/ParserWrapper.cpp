////////////////////////////////////////////////////////////////////////////////
/// @brief QL parser wrapper and utility classes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ParserWrapper.h"

using namespace std;
using namespace triagens::avocado;


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                  class ParseError
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new parse error instance
////////////////////////////////////////////////////////////////////////////////

ParseError::ParseError (const string& message, const size_t line, const size_t column) :
  _message(message), _line(line), _column(column) {
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Destroy the instance
////////////////////////////////////////////////////////////////////////////////

ParseError::~ParseError () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the parse error as a formatted string
////////////////////////////////////////////////////////////////////////////////

string ParseError::getDescription () const {
  ostringstream errorMessage;
  errorMessage << "Parse error at " << _line << "," << _column << ": " << _message;

  return errorMessage.str();
}


// -----------------------------------------------------------------------------
// --SECTION--                                               class ParserWrapper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new instance
////////////////////////////////////////////////////////////////////////////////

ParserWrapper::ParserWrapper (const char *query) : 
  _query(query), _parseError(0), _isParsed(false) {
  _context = (QL_parser_context_t*) TRI_Allocate(sizeof(QL_parser_context_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destroy the instance and free all associated memory
////////////////////////////////////////////////////////////////////////////////

ParserWrapper::~ParserWrapper () {
  if (_context != 0) {
    QLParseFree(_context);
  }
  if (_parseError) {
    delete _parseError;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Parse the query passed 
////////////////////////////////////////////////////////////////////////////////

bool ParserWrapper::parse () {
  if (_context == 0) {
    return false;
  }

  if (_query == 0) {
    return false;
  }

  if (_isParsed) { 
    return false;
  }

  _isParsed = true;

  if (!QLParseInit(_context, _query)) {
    return false;
  }

  if (QLparse(_context)) {
    // parse error
    QL_error_state_t *errorState = &_context->_lexState._errorState;
    _parseError= new ParseError(errorState->_message, errorState->_line, errorState->_column);
    return false;
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get a parse error that may have occurred
////////////////////////////////////////////////////////////////////////////////

ParseError *ParserWrapper::getParseError () {
  return _parseError;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the type of the query
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_type_e ParserWrapper::getQueryType () {
  if (!_isParsed) {
    return QLQueryTypeUndefined;
  }
  
  return _context->_query->_type;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Create a select clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t *ParserWrapper::getSelect () {
  TRI_qry_select_t* select = 0;

  if (_isParsed) {
    QLOptimize(_context->_query->_select._base);
    QL_ast_query_select_type_e selectType = QLOptimizeGetSelectType(_context->_query);

    if (selectType == QLQuerySelectTypeSimple) {
      select = TRI_CreateQuerySelectDocument();
    } 
    else if (selectType == QLQuerySelectTypeEvaluated) {
      TRI_string_buffer_t *selectJs = QLJavascripterInit();
      TRI_AppendStringStringBuffer(selectJs, "(function($) { return ");
      QLJavascripterWalk(selectJs, _context->_query->_select._base);
      TRI_AppendStringStringBuffer(selectJs, " })");
      select = TRI_CreateQuerySelectGeneral(selectJs->_buffer);
      QLJavascripterFree(selectJs);
    }
  }

  return select;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the alias of the primary collection
////////////////////////////////////////////////////////////////////////////////

char *ParserWrapper::getPrimaryAlias () {
  if (_isParsed) {
    return QLAstQueryGetPrimaryAlias(_context->_query);
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the name of the primary collection
////////////////////////////////////////////////////////////////////////////////

char *ParserWrapper::getPrimaryName () {
  if (_isParsed) {
    return QLAstQueryGetPrimaryName(_context->_query);
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Create a where clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t *ParserWrapper::getWhere () {
  TRI_qry_where_t* where = 0;

  if (_isParsed) {
    QLOptimize(_context->_query->_where._base);
QL_formatter_t f;
f.indentLevel = 0;
QLFormatterDump(_context->_query->_where._base, &f, 0);

    _context->_query->_where._type = QLOptimizeGetWhereType(_context->_query);

    if (_context->_query->_where._type == QLQueryWhereTypeAlwaysTrue) {
      // where condition is always true 
      where = TRI_CreateQueryWhereBoolean(true);
    } 
    else if (_context->_query->_where._type == QLQueryWhereTypeAlwaysFalse) {
      // where condition is always false
      where = TRI_CreateQueryWhereBoolean(false);
    }
    else {
      // where condition must be evaluated for each result
      TRI_string_buffer_t *whereJs = QLJavascripterInit();
      TRI_AppendStringStringBuffer(whereJs, "(function($) { return (");
      QLJavascripterWalk(whereJs, _context->_query->_where._base);
      TRI_AppendStringStringBuffer(whereJs, "); })");
      where = TRI_CreateQueryWhereGeneral(whereJs->_buffer);
std::cout << "WHERE: " << whereJs->_buffer << "\n";
      QLJavascripterFree(whereJs);
    }
  }

  return where;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the skip value
////////////////////////////////////////////////////////////////////////////////

TRI_voc_size_t ParserWrapper::getSkip () {
  TRI_voc_size_t  skip  = 0;

  if (_isParsed) {
    if (_context->_query->_limit._isUsed) {
      skip  = (TRI_voc_size_t)  _context->_query->_limit._offset;
    }
    else {
      skip  = TRI_QRY_NO_SKIP;
    }
  }

  return skip;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Get the limit value
////////////////////////////////////////////////////////////////////////////////

TRI_voc_ssize_t ParserWrapper::getLimit () {
  TRI_voc_ssize_t limit = 0;

  if (_isParsed) {
    if (_context->_query->_limit._isUsed) {
      limit = (TRI_voc_ssize_t) _context->_query->_limit._count;
    }
    else {
      limit = TRI_QRY_NO_LIMIT;
    }
  }

  return limit;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
