////////////////////////////////////////////////////////////////////////////////
/// @brief query request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_QUERY_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_QUERY_HANDLER_H 1

#include "Basics/Common.h"

#include "RestHandler/RestVocbaseBaseHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {
    class ApplicationV8;

// -----------------------------------------------------------------------------
// --SECTION--                                            class RestQueryHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
////////////////////////////////////////////////////////////////////////////////

    class RestQueryHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestQueryHandler (rest::HttpRequest*, ApplicationV8*);

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

        status_t execute ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of properties
////////////////////////////////////////////////////////////////////////////////

        bool readQueryProperties ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of slow queries
////////////////////////////////////////////////////////////////////////////////

        bool readQuery (bool slow);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns AQL query tracking
////////////////////////////////////////////////////////////////////////////////

        bool readQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the slow log
////////////////////////////////////////////////////////////////////////////////

        bool deleteQuerySlow ();

////////////////////////////////////////////////////////////////////////////////
/// @brief interrupts a named query
////////////////////////////////////////////////////////////////////////////////

        bool deleteQuery (const std::string& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief interrupts a query
////////////////////////////////////////////////////////////////////////////////

        bool deleteQuery ();

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the settings
////////////////////////////////////////////////////////////////////////////////

        bool replaceProperties ();

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a query
////////////////////////////////////////////////////////////////////////////////

        bool parseQuery ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief application V8
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8* _applicationV8;
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
