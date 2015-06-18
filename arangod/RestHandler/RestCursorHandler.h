////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
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

#ifndef ARANGODB_REST_HANDLER_REST_CURSOR_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_CURSOR_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Aql/QueryResult.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

struct TRI_json_t;

// -----------------------------------------------------------------------------
// --SECTION--                                           class RestCursorHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {
    class Query;
    class QueryRegistry;
  }

  namespace arango {
    class ApplicationV8;
    class Cursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
////////////////////////////////////////////////////////////////////////////////

    class RestCursorHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestCursorHandler (rest::HttpRequest*,
                           std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        virtual status_t execute () override;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool cancel (bool running) override;

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief processes the query and returns the results/cursor
/// this method is also used by derived classes
////////////////////////////////////////////////////////////////////////////////

        void processQuery (struct TRI_json_t const*);

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
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json buildOptions (TRI_json_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the "extra" attribute values from the result.
/// note that the "extra" object will take ownership from the result for 
/// several values
////////////////////////////////////////////////////////////////////////////////
      
        triagens::basics::Json buildExtra (triagens::aql::QueryResult&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the cursor into the response body
////////////////////////////////////////////////////////////////////////////////

        void dumpCursor (triagens::arango::Cursor*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor and return the first results
////////////////////////////////////////////////////////////////////////////////

        void createCursor ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
////////////////////////////////////////////////////////////////////////////////
        
        void modifyCursor ();

////////////////////////////////////////////////////////////////////////////////
/// @brief dispose an existing cursor
////////////////////////////////////////////////////////////////////////////////
        
        void deleteCursor ();

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
