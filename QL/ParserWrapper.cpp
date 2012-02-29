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

ParseError::ParseError (const string& data, const size_t line, const size_t column) :
  _data(data), _line(line), _column(column) {
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
  errorMessage << "Parse error at " << _line << "," << _column << ": " << _data;

  return errorMessage.str();
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class ParserWrapper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new instance
////////////////////////////////////////////////////////////////////////////////

ParserWrapper::ParserWrapper (const TRI_vocbase_t* vocbase, const char* query) : 
  _vocbase(vocbase), _query(query), _parseError(0) { 
  _template = TRI_CreateQueryTemplate(query, vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destroy the instance and free all associated memory
////////////////////////////////////////////////////////////////////////////////

ParserWrapper::~ParserWrapper () {
  if (_template) {
    TRI_FreeQueryTemplate(_template);
  }

  if (_parseError) {
    delete _parseError;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Parse the query passed 
////////////////////////////////////////////////////////////////////////////////

bool ParserWrapper::parse () {
  if (!_template) {
    return false;
  }

  return TRI_ParseQueryTemplate(_template);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a parse error that may have occurred
////////////////////////////////////////////////////////////////////////////////

ParseError* ParserWrapper::getParseError () {
  char* message = TRI_GetStringQueryError((const TRI_query_error_t* const) &_template->_error);

  _parseError = new ParseError(message, 0, 0);
  if (message) {
    TRI_Free(message);
  }
  return _parseError;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the type of the query
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_type_e ParserWrapper::getQueryType () {
  return _template->_query->_type;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a select clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* ParserWrapper::getSelect () {
  TRI_qry_select_t* select = NULL;

  QL_ast_query_select_type_e selectType = QLOptimizeGetSelectType(_template->_query);

  if (selectType == QLQuerySelectTypeSimple) {
    select = TRI_CreateQuerySelectDocument();
  } 
  else if (selectType == QLQuerySelectTypeEvaluated) {
    TRI_query_javascript_converter_t* selectJs = TRI_InitQueryJavascript();
    if (selectJs) {
      TRI_AppendStringStringBuffer(selectJs->_buffer, "(function($) { return ");
      TRI_ConvertQueryJavascript(selectJs, _template->_query->_select._base);
      TRI_AppendStringStringBuffer(selectJs->_buffer, " })");
      select = TRI_CreateQuerySelectGeneral(selectJs->_buffer->_buffer);
      // TODO: REMOVE ME
      // std::cout << "SELECT: " << selectJs->_buffer->_buffer << "\n";
      TRI_FreeQueryJavascript(selectJs);
    }
  }

  return select;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create joins
////////////////////////////////////////////////////////////////////////////////

TRI_select_join_t* ParserWrapper::getJoins () {
  TRI_select_join_t* join = NULL;
  TRI_vector_pointer_t* ranges;
  TRI_join_type_e joinType;
  QL_ast_query_collection_t* collection;
  char* collectionName;
  char* collectionAlias;

  join = TRI_CreateSelectJoin();
  if (!join) {
    return NULL;
  }

  TRI_query_node_t* node = _template->_query->_from._base;
  node = node->_next;
  //QL_formatter_t f;
  //f._indentLevel = 0;
  //QLFormatterDump(_template->_query->_from._base, &f, 0);

  assert(node);

  // primary table
  TRI_query_node_t* lhs = node->_lhs;
  TRI_query_node_t* rhs = node->_rhs;
  collectionName = lhs->_value._stringValue;
  collectionAlias = rhs->_value._stringValue;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&_template->_query->_from._collections, collectionAlias);

  if (collection->_geoRestriction) {
    ranges = NULL;
  }
  else {
    ranges = QLOptimizeCondition(_template->_query->_where._base);
  }

  TRI_AddPartSelectJoin(join, 
                        JOIN_TYPE_PRIMARY, 
                        NULL, 
                        ranges, 
                        collectionName, 
                        collectionAlias,
                        QLAstQueryCloneRestriction(collection->_geoRestriction));

  while (node->_next) {
    node = node->_next;
    TRI_query_node_t* ref = node->_lhs;
    TRI_query_node_t* condition = node->_rhs;

    QL_ast_query_where_type_e conditionType = QLOptimizeGetWhereType(condition);

    TRI_qry_where_t* joinWhere = NULL;
    ranges = NULL;

    collectionName = ref->_lhs->_value._stringValue;
    collectionAlias = ref->_rhs->_value._stringValue;

    collection = (QL_ast_query_collection_t*) 
      TRI_LookupByKeyAssociativePointer(&_template->_query->_from._collections, collectionAlias);

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
      TRI_query_javascript_converter_t* conditionJs = TRI_InitQueryJavascript();
      if (conditionJs != 0) {
        TRI_AppendStringStringBuffer(conditionJs->_buffer, "(function($) { return (");
        TRI_ConvertQueryJavascript(conditionJs, condition);
        TRI_AppendStringStringBuffer(conditionJs->_buffer, "); })");
        joinWhere = TRI_CreateQueryWhereGeneral(conditionJs->_buffer->_buffer);
        TRI_FreeQueryJavascript(conditionJs);
        if (!collection->_geoRestriction) {
          ranges = QLOptimizeCondition(condition);
        }
      }
      if (!joinWhere) {
        join->free(join);
        return NULL;
      }
    }

    if (node->_type == TRI_QueryNodeJoinList) {
      joinType = JOIN_TYPE_LIST;
    }
    else if (node->_type == TRI_QueryNodeJoinInner) {
      joinType = JOIN_TYPE_INNER;
    }
    else if (node->_type == TRI_QueryNodeJoinLeft) {
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
                          QLAstQueryCloneRestriction(collection->_geoRestriction));
  }

  return join;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a where clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* ParserWrapper::getWhere () {
  TRI_qry_where_t* where = NULL;

  // TODO: REMOVE ME
  // QL_formatter_t f;
  // f._indentLevel = 0;
  // QLFormatterDump(_template->_query->_where._base, &f, 0);

  _template->_query->_where._type = QLOptimizeGetWhereType(_template->_query->_where._base);

  if (_template->_query->_where._type == QLQueryWhereTypeAlwaysTrue) {
    // where condition is always true 
    where = TRI_CreateQueryWhereBoolean(true);
  } 
  else if (_template->_query->_where._type == QLQueryWhereTypeAlwaysFalse) {
    // where condition is always false
    where = TRI_CreateQueryWhereBoolean(false);
  }
  else {
    // where condition must be evaluated for each result
    TRI_query_javascript_converter_t* whereJs = TRI_InitQueryJavascript();
    if (whereJs) {
      TRI_AppendStringStringBuffer(whereJs->_buffer, "(function($) { return (");
      TRI_ConvertQueryJavascript(whereJs, _template->_query->_where._base);
      TRI_AppendStringStringBuffer(whereJs->_buffer, "); })");
      where = TRI_CreateQueryWhereGeneral(whereJs->_buffer->_buffer);
      // TODO: REMOVE ME
      // std::cout << "WHERE: " << whereJs->_buffer->_buffer << "\n";
      TRI_FreeQueryJavascript(whereJs);
    }
  }

  return where;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an order clause
////////////////////////////////////////////////////////////////////////////////

TRI_qry_order_t* ParserWrapper::getOrder () {
  TRI_qry_order_t* order = NULL;

  if (QLOptimizeGetOrderType(_template->_query->_order._base) == QLQueryOrderTypeNone) {
    return NULL;
  }

  TRI_query_javascript_converter_t* orderJs = TRI_InitQueryJavascript();
  if (orderJs) {
    TRI_AppendStringStringBuffer(orderJs->_buffer, "(function($){var lhs,rhs;");
    TRI_ConvertOrderQueryJavascript(orderJs, _template->_query->_order._base->_next);
    TRI_AppendStringStringBuffer(orderJs->_buffer, "})");
    order = TRI_CreateQueryOrderGeneral(orderJs->_buffer->_buffer);
    // TODO: REMOVE ME
    // std::cout << "ORDER: " << orderJs->_buffer->_buffer << "\n";
    TRI_FreeQueryJavascript(orderJs);
  }

  return order;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the skip value
////////////////////////////////////////////////////////////////////////////////

TRI_voc_size_t ParserWrapper::getSkip () {
  TRI_voc_size_t skip;

  if (_template->_query->_limit._isUsed) {
    skip  = (TRI_voc_size_t)  _template->_query->_limit._offset;
  }
  else {
    skip = TRI_QRY_NO_SKIP;
  }

  return skip;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the limit value
////////////////////////////////////////////////////////////////////////////////

TRI_voc_ssize_t ParserWrapper::getLimit () {
  TRI_voc_ssize_t limit;

  if (_template->_query->_limit._isUsed) {
    limit = (TRI_voc_ssize_t) _template->_query->_limit._count;
  }
  else {
    limit = TRI_QRY_NO_LIMIT;
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
