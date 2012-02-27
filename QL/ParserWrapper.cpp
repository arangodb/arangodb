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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
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

ParserWrapper::ParserWrapper (const TRI_vocbase_t* vocbase, const char* query) : 
  _vocbase(vocbase), _query(query), _parseError(0), 
  _isParsed(false), _isOptimized(false) {
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

  if (!QLParseInit(_vocbase, _context, _query)) {
    return false;
  }

  if (QLparse(_context)) {
    // parse error
    setParseError();
    return false;
  }

  size_t order;
  if (!QLParseValidateCollections(_context, 
                                  _context->_query->_select._base, 
                                  &QLAstQueryIsValidAlias,
                                  &order)) {
    // select expression invalid
    setParseError();
    return false;
  }

  if (!QLParseValidateCollections(_context, 
                                  _context->_query->_where._base,
                                  &QLAstQueryIsValidAlias,
                                  &order)) {
    // where expression invalid
    setParseError();
    return false;
  }
  
  if (!QLParseValidateCollections(_context, 
                                  _context->_query->_order._base,
                                  &QLAstQueryIsValidAlias,
                                  &order)) {
    // where expression invalid
    setParseError();
    return false;
  }
  
  order = 0;
  if (!QLParseValidateCollections(_context, 
                                  _context->_query->_from._base,
                                  &QLAstQueryIsValidAliasOrdered,
                                  &order)) {
    // from expression(s) invalid
    setParseError();
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Register the parse error set by the parser
////////////////////////////////////////////////////////////////////////////////

void ParserWrapper::setParseError () {
  QL_error_state_t* errorState = &_context->_lexState._errorState;
  _parseError= new ParseError(errorState->_message, errorState->_line, errorState->_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a parse error that may have occurred
////////////////////////////////////////////////////////////////////////////////

ParseError* ParserWrapper::getParseError () {
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
/// @brief Run optimizations on the different query parts
////////////////////////////////////////////////////////////////////////////////
 
void ParserWrapper::optimize () { 
  if (!_isParsed) {
    return;
  }

  if (_isOptimized) {
    return;
  }

  // optimize select clause
  QLOptimizeExpression(_context->_query->_select._base);

  // optimize where clause
  QLOptimizeExpression(_context->_query->_where._base);

  // optimize order clause
  if (_context->_query->_order._base) {
    QLOptimizeOrder(_context->_query->_order._base);
  }

  // optimize joins/on clauses
  QLOptimizeFrom(_context);

//  QLOptimizeDetermineIndexes(_context->_query);

  _isOptimized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a select clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* ParserWrapper::getSelect () {
  TRI_qry_select_t* select = 0;

  if (!_isParsed) {
    return 0;
  }

  optimize();

  QL_ast_query_select_type_e selectType = QLOptimizeGetSelectType(_context->_query);

  if (selectType == QLQuerySelectTypeSimple) {
    select = TRI_CreateQuerySelectDocument();
  } 
  else if (selectType == QLQuerySelectTypeEvaluated) {
    QL_javascript_conversion_t* selectJs = QLJavascripterInit();
    if (selectJs != 0) {
      TRI_AppendStringStringBuffer(selectJs->_buffer, "(function($) { return ");
      QLJavascripterConvert(selectJs, _context->_query->_select._base);
      TRI_AppendStringStringBuffer(selectJs->_buffer, " })");
      select = TRI_CreateQuerySelectGeneral(selectJs->_buffer->_buffer);
      // TODO: REMOVE ME
      // std::cout << "SELECT: " << selectJs->_buffer->_buffer << "\n";
      QLJavascripterFree(selectJs);
    }
  }

  return select;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create joins
////////////////////////////////////////////////////////////////////////////////

TRI_select_join_t* ParserWrapper::getJoins () {
  TRI_select_join_t* join = 0;
  TRI_vector_pointer_t* ranges;
  TRI_join_type_e joinType;
  QL_ast_query_collection_t* collection;
  char* collectionName;
  char* collectionAlias;

  if (!_isParsed ) {
    return NULL;
  }

  optimize();

  join = TRI_CreateSelectJoin();
  if (!join) {
    return NULL;
  }

  QL_ast_node_t* node = (QL_ast_node_t*) _context->_query->_from._base;
  node = (QL_ast_node_t*) node->_next;
  //QL_formatter_t f;
  //f._indentLevel = 0;
  //QLFormatterDump(_context->_query->_from._base, &f, 0);

  assert(node != 0);

  // primary table
  QL_ast_node_t* lhs = (QL_ast_node_t*) node->_lhs;
  QL_ast_node_t* rhs = (QL_ast_node_t*) node->_rhs;
  collectionName = lhs->_value._stringValue;
  collectionAlias = rhs->_value._stringValue;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&_context->_query->_from._collections, collectionAlias);

  if (collection->_restriction) {
    ranges = NULL;
  }
  else {
    ranges = QLOptimizeCondition(_context->_query->_where._base);
  }

  TRI_AddPartSelectJoin(join, 
                        JOIN_TYPE_PRIMARY, 
                        NULL, 
                        ranges, 
                        collectionName, 
                        collectionAlias,
                        QLAstQueryCloneRestriction(collection->_restriction));

  while (node->_next) {
    node = (QL_ast_node_t*) node->_next;
    QL_ast_node_t* ref = (QL_ast_node_t*) node->_lhs;
    QL_ast_node_t* condition = (QL_ast_node_t*) node->_rhs;

    QL_ast_query_where_type_e conditionType = QLOptimizeGetWhereType(condition);

    TRI_qry_where_t* joinWhere = 0;
    ranges = NULL;
    collection = (QL_ast_query_collection_t*) 
      TRI_LookupByKeyAssociativePointer(&_context->_query->_from._collections, collectionAlias);

    if (conditionType == QLQueryWhereTypeAlwaysTrue) {
      // join condition is always true 
      joinWhere = TRI_CreateQueryWhereBoolean(true);
      if (!joinWhere) {
        join->free(join);
        return NULL;
      }
    } 
    else if (conditionType == QLQueryWhereTypeAlwaysFalse) {
      // join condition is always false
      joinWhere = TRI_CreateQueryWhereBoolean(false);
      if (!joinWhere) {
        join->free(join);
        return NULL;
      }
    }
    else {
      // join condition must be evaluated for each result
      QL_javascript_conversion_t* conditionJs = QLJavascripterInit();
      if (conditionJs != 0) {
        TRI_AppendStringStringBuffer(conditionJs->_buffer, "(function($) { return (");
        QLJavascripterConvert(conditionJs, condition);
        TRI_AppendStringStringBuffer(conditionJs->_buffer, "); })");
        joinWhere = TRI_CreateQueryWhereGeneral(conditionJs->_buffer->_buffer);
        QLJavascripterFree(conditionJs);
        if (!collection->_restriction) {
          ranges = QLOptimizeCondition(condition);
        }
      }
      if (!joinWhere) {
        join->free(join);
        return NULL;
      }
    }

    collectionName = ((QL_ast_node_t*) (ref->_lhs))->_value._stringValue;
    collectionAlias = ((QL_ast_node_t*) (ref->_rhs))->_value._stringValue;

    if (node->_type == QLNodeJoinList) {
      joinType = JOIN_TYPE_LIST;
    }
    else if (node->_type == QLNodeJoinInner) {
      joinType = JOIN_TYPE_INNER;
    }
    else if (node->_type == QLNodeJoinLeft) {
      joinType = JOIN_TYPE_OUTER;
    }
    else {
      assert(false);
    }
  
    TRI_AddPartSelectJoin(join, 
                          joinType, 
                          joinWhere, 
                          ranges, 
                          collectionName, 
                          collectionAlias,
                          QLAstQueryCloneRestriction(collection->_restriction));
  }

  return join;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a where clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* ParserWrapper::getWhere () {
  TRI_qry_where_t* where = 0;

  if (!_isParsed) {
    return NULL;
  }

  optimize();
  // TODO: REMOVE ME
  // QL_formatter_t f;
  // f._indentLevel = 0;
  // QLFormatterDump(_context->_query->_where._base, &f, 0);

  _context->_query->_where._type = QLOptimizeGetWhereType(_context->_query->_where._base);

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
    QL_javascript_conversion_t* whereJs = QLJavascripterInit();
    if (whereJs != 0) {
      TRI_AppendStringStringBuffer(whereJs->_buffer, "(function($) { return (");
      QLJavascripterConvert(whereJs, _context->_query->_where._base);
      TRI_AppendStringStringBuffer(whereJs->_buffer, "); })");
      where = TRI_CreateQueryWhereGeneral(whereJs->_buffer->_buffer);
      // TODO: REMOVE ME
      // std::cout << "WHERE: " << whereJs->_buffer->_buffer << "\n";
      QLJavascripterFree(whereJs);
    }
  }

  return where;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an order clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_order_t* ParserWrapper::getOrder () {
  TRI_qry_order_t* order = 0;

  if (!_isParsed ) {
    return NULL;
  }
  
  optimize();

  if (QLOptimizeGetOrderType(_context->_query->_order._base) == QLQueryOrderTypeNone) {
    return 0;
  }

  QL_javascript_conversion_t* orderJs = QLJavascripterInit();
  if (orderJs != 0) {
    TRI_AppendStringStringBuffer(orderJs->_buffer, "(function($){var lhs,rhs;");
    QLJavascripterConvertOrder(orderJs, (QL_ast_node_t*) _context->_query->_order._base->_next);
    TRI_AppendStringStringBuffer(orderJs->_buffer, "})");
    order = TRI_CreateQueryOrderGeneral(orderJs->_buffer->_buffer);
    // TODO: REMOVE ME
    // std::cout << "ORDER: " << orderJs->_buffer->_buffer << "\n";
    QLJavascripterFree(orderJs);
  }

  return order;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the skip value
////////////////////////////////////////////////////////////////////////////////

TRI_voc_size_t ParserWrapper::getSkip () {
  TRI_voc_size_t  skip = 0;

  if (_isParsed) {
    if (_context->_query->_limit._isUsed) {
      skip  = (TRI_voc_size_t)  _context->_query->_limit._offset;
    }
    else {
      skip = TRI_QRY_NO_SKIP;
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
