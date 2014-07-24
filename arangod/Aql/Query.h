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

#ifndef ARANGODB_AQL_QUERY_H
#define ARANGODB_AQL_QUERY_H 1

#include "Basics/Common.h"
#include "Aql/QueryError.h"

struct TRI_json_s;
struct TRI_vocbase_s;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the type of query to execute
////////////////////////////////////////////////////////////////////////////////

    enum QueryType {
      AQL_QUERY_READ,
      AQL_QUERY_REMOVE,
      AQL_QUERY_INSERT,
      AQL_QUERY_UPDATE,
      AQL_QUERY_REPLACE
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Query
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief an AQL query
////////////////////////////////////////////////////////////////////////////////

    class Query {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        Query (struct TRI_vocbase_s*,
               char const*,
               size_t,
               struct TRI_json_s*);

        ~Query ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query type
////////////////////////////////////////////////////////////////////////////////

        inline QueryType type () const {
          return _type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query type
////////////////////////////////////////////////////////////////////////////////

        void type (QueryType type) {
          _type = type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query string
////////////////////////////////////////////////////////////////////////////////

        char const* queryString () const {
          return _queryString;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of the query string
////////////////////////////////////////////////////////////////////////////////

        size_t queryLength () const {
          return _queryLength;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        void execute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        void parse ();

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        void explain ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        void parseQuery ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      private:

        struct TRI_vocbase_s*  _vocbase;

        char const*            _queryString;
        size_t const           _queryLength;
        QueryType              _type;

        struct TRI_json_s*     _bindParameters;

        QueryError             _error;
    };

/*
typedef struct TRI_aql_context_s {
  struct TRI_vocbase_s*       _vocbase;
  struct TRI_aql_parser_s*    _parser;
  TRI_aql_statement_list_t*   _statements;
  TRI_aql_error_t             _error;
  TRI_vector_pointer_t        _collections;
  TRI_associative_pointer_t   _collectionNames;

  struct {
    TRI_vector_pointer_t      _nodes;
    TRI_vector_pointer_t      _strings;
    TRI_vector_pointer_t      _scopes;
  }
  _memory;

  TRI_vector_pointer_t        _currentScopes;

  struct {
    TRI_associative_pointer_t _values;
    TRI_associative_pointer_t _names;
  }
  _parameters;

  const char*                 _query;

  size_t                      _variableIndex;
  size_t                      _scopeIndex;
  size_t                      _subQueries;

  TRI_aql_query_type_e        _type;
  char*                       _writeCollection;
  struct TRI_aql_node_t*      _writeOptions;

  struct TRI_json_s*          _userOptions;
  bool                        _fullCount;
  bool                        _isCoordinator;
}
TRI_aql_context_t;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create and initialize a context
////////////////////////////////////////////////////////////////////////////////

TRI_aql_context_t* TRI_CreateContextAql (struct TRI_vocbase_s*,
                                         const char* const,
                                         const size_t,
                                         bool,
                                         struct TRI_json_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse & validate the query string
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateQueryContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief add bind parameters to the query context
////////////////////////////////////////////////////////////////////////////////

bool TRI_BindQueryContextAql (TRI_aql_context_t* const,
                              const struct TRI_json_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief perform some AST optimisations
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseQueryContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief set up all collections used in the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetupCollectionsContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_RegisterStringAql (TRI_aql_context_t* const,
                             const char* const,
                             const size_t,
                             const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a combined string
////////////////////////////////////////////////////////////////////////////////

char* TRI_RegisterString2Aql (TRI_aql_context_t* const,
                              const char* const,
                              const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a combined string
////////////////////////////////////////////////////////////////////////////////

char* TRI_RegisterString3Aql (TRI_aql_context_t* const,
                              const char* const,
                              const char* const,
                              const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a node
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterNodeContextAql (TRI_aql_context_t* const,
                                 void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetErrorContextAql (const char* file,
                             int line,
                             TRI_aql_context_t* const,
                             const int,
                             const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the value of an option variable
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_GetOptionContextAql (TRI_aql_context_t* const,
                                            const char*);

*/

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
