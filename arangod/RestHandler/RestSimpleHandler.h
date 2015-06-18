////////////////////////////////////////////////////////////////////////////////
/// @brief simple document batch request handler
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
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_SIMPLE_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_SIMPLE_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Aql/QueryResult.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

struct TRI_json_t;

// -----------------------------------------------------------------------------
// --SECTION--                                           class RestSimpleHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {
    class Query;
    class QueryRegistry;
  }

  namespace arango {
    class ApplicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief document batch request handler
////////////////////////////////////////////////////////////////////////////////

    class RestSimpleHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestSimpleHandler (rest::HttpRequest*,
                           std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t execute () override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool cancel (bool running) override;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

        void registerQuery (triagens::aql::Query*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

        void unregisterQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

        bool cancelQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was cancelled
////////////////////////////////////////////////////////////////////////////////

        bool wasCancelled ();

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a batch remove operation
////////////////////////////////////////////////////////////////////////////////

        void removeByKeys (struct TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a batch lookup operation
////////////////////////////////////////////////////////////////////////////////

        void lookupByKeys (struct TRI_json_t const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief _applicationV8
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::ApplicationV8* _applicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief our query registry
////////////////////////////////////////////////////////////////////////////////

        triagens::aql::QueryRegistry* _queryRegistry;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for currently running query
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _queryLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief currently running query
////////////////////////////////////////////////////////////////////////////////

        triagens::aql::Query* _query;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was killed
////////////////////////////////////////////////////////////////////////////////

        bool _queryKilled;
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
