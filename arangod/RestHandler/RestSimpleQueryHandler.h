////////////////////////////////////////////////////////////////////////////////
/// @brief simple query handler
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

#ifndef ARANGODB_REST_HANDLER_REST_SIMPLE_QUERY_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_SIMPLE_QUERY_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestCursorHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                      class RestSimpleQueryHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {
    class QueryRegistry;
  }

  namespace arango {
    class ApplicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
////////////////////////////////////////////////////////////////////////////////

    class RestSimpleQueryHandler : public RestCursorHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestSimpleQueryHandler (rest::HttpRequest*,
                                std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t execute () override final;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor with all documents from the collection
////////////////////////////////////////////////////////////////////////////////

        void allDocuments ();

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
