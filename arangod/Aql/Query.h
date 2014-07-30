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
#include "Aql/BindParameters.h"
#include "Aql/ParseResult.h"
#include "Aql/QueryAst.h"

struct TRI_json_s;
struct TRI_vocbase_s;

namespace triagens {
  namespace aql {

    class V8Executor;

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
/// @brief extract a region from the query
////////////////////////////////////////////////////////////////////////////////

        std::string extractRegion (int, 
                                   int) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

        void registerError (int,
                            char const* = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        ParseResult execute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query
////////////////////////////////////////////////////////////////////////////////

        ParseResult parse ();

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

        void explain ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get v8 executor
////////////////////////////////////////////////////////////////////////////////

        V8Executor* getExecutor ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        struct TRI_vocbase_s*  _vocbase;
        
        V8Executor*            _executor;

        char const*            _queryString;
        size_t const           _queryLength;
        QueryType              _type;

        BindParameters         _bindParameters;

    };

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
