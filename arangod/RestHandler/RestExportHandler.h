////////////////////////////////////////////////////////////////////////////////
/// @brief export request handler
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

#ifndef ARANGODB_REST_HANDLER_REST_EXPORT_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_EXPORT_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Utils/CollectionExport.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                           class RestExportHandler
// -----------------------------------------------------------------------------

namespace triagens {

  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
////////////////////////////////////////////////////////////////////////////////

    class RestExportHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        explicit RestExportHandler (rest::HttpRequest*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t execute () override;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json buildOptions (TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an export cursor and return the first results
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
/// @brief restrictions for export
////////////////////////////////////////////////////////////////////////////////

        CollectionExport::Restrictions _restrictions; 

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
