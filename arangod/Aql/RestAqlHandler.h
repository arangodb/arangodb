////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query/cursor request handler
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_REST_AQL_HANDLER_H
#define ARANGODB_AQL_REST_AQL_HANDLER_H 1

#include "Basics/Common.h"

#include "Admin/RestBaseHandler.h"
#include "V8Server/ApplicationV8.h"
#include "RestServer/VocbaseContext.h"
#include "Aql/QueryRegistry.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                        i     class RestAqlHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief shard control request handler
////////////////////////////////////////////////////////////////////////////////

    class RestAqlHandler : public admin::RestBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestAqlHandler (rest::HttpRequest* request,
                        std::pair<arango::ApplicationV8*, QueryRegistry*>* pair);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool isDirect ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        std::string const& queue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler
////////////////////////////////////////////////////////////////////////////////

        status_t execute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/instanciate
/// The body is a JSON with attributes "plan" for the execution plan and
/// "options" for the options, all exactly as in AQL_EXECUTEJSON.
////////////////////////////////////////////////////////////////////////////////

        void createQueryFromJson ();

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/parse
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_PARSE with exactly these parameters and
/// does not keep the query hanging around.
////////////////////////////////////////////////////////////////////////////////

        void parseQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/explain
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_EXPLAIN with exactly these parameters and
/// does not keep the query hanging around.
////////////////////////////////////////////////////////////////////////////////

        void explainQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/query
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This sets up the query as as AQL_EXECUTE would, but does not use
/// the cursor API yet. Rather, the query is stored in the query registry
/// for later use by PUT requests.
////////////////////////////////////////////////////////////////////////////////

        void createQueryFromString ();

////////////////////////////////////////////////////////////////////////////////
/// @brief DELETE method for /_api/aql/<queryId>
/// The query specified by <queryId> is deleted.
////////////////////////////////////////////////////////////////////////////////

        void deleteQuery (std::string const& idString);

////////////////////////////////////////////////////////////////////////////////
/// @brief PUT method for /_api/aql/<operation>/<queryId>, this is using 
/// the part of the cursor API with side effects.
/// <operation>: can be "getSome" or "skip".
/// The body must be a Json with the following attributes:
/// For the "getSome" operation one has to give:
///   "atLeast": 
///   "atMost": both must be positive integers, the cursor returns never 
///             more than "atMost" items and tries to return at least
///             "atLeast". Note that it is possible to return fewer than
///             "atLeast", for example if there are only fewer items
///             left. However, the implementation may return fewer items
///             than "atLeast" for internal reasons, for example to avoid
///             excessive copying. The result is the JSON representation of an 
///             AqlItemBlock.
/// For the "skip" operation one has to give:
///   "number": must be a positive integer, the cursor skips as many items,
///             possibly exhausting the cursor.
///             The result is a JSON with the attributes "error" (boolean),
///             "errorMessage" (if applicable) and "exhausted" (boolean)
///             to indicate whether or not the cursor is exhausted.
////////////////////////////////////////////////////////////////////////////////

        void useQuery (std::string const& operation,
                       std::string const& idString);

////////////////////////////////////////////////////////////////////////////////
/// @brief GET method for /_api/aql/<queryId>
////////////////////////////////////////////////////////////////////////////////

        void getInfoQuery (std::string const& operation,
                           std::string const& idString);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief handle for useQuery
////////////////////////////////////////////////////////////////////////////////

        void handleUseQuery (std::string const&,
                             Query*,
                             triagens::basics::Json const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief parseJsonBody, returns a nullptr and produces an error response if
/// parse was not successful.
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* parseJsonBody ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief dig out vocbase from context and query from ID, handle errors
////////////////////////////////////////////////////////////////////////////////

        bool findQuery (std::string const& idString,
                        Query*& query);

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

        static const std::string QUEUE_NAME;

////////////////////////////////////////////////////////////////////////////////
/// @brief _applicationV8
////////////////////////////////////////////////////////////////////////////////

        arango::ApplicationV8* _applicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief request context
////////////////////////////////////////////////////////////////////////////////

        arango::VocbaseContext* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief our query registry
////////////////////////////////////////////////////////////////////////////////

        QueryRegistry* _queryRegistry;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of current query
////////////////////////////////////////////////////////////////////////////////
                        
        QueryId _qId;

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
