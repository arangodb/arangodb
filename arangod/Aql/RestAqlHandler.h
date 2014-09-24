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
/// @brief POST method
////////////////////////////////////////////////////////////////////////////////

        void createQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief DELETE method
////////////////////////////////////////////////////////////////////////////////

        void deleteQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief PUT method
////////////////////////////////////////////////////////////////////////////////

        void useQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief GET method
////////////////////////////////////////////////////////////////////////////////

        void getInfoQuery ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// parseJsonBody, returns a nullptr and produces an error response if parse
/// was not successful.
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* parseJsonBody ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

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
