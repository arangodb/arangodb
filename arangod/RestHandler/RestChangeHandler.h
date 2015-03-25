////////////////////////////////////////////////////////////////////////////////
/// @brief change request handler
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

#ifndef ARANGODB_REST_HANDLER_REST_CHANGE_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_CHANGE_HANDLER_H 1

#include "Basics/Common.h"

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "HttpServer/HttpServer.h"

struct TRI_vocbase_col_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                 RestChangeHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief change request handler
////////////////////////////////////////////////////////////////////////////////

    class RestChangeHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestChangeHandler (rest::HttpRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RestChangeHandler ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        Handler::status_t execute ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief determine chunk size from request
////////////////////////////////////////////////////////////////////////////////

        uint64_t determineChunkSize () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract and return changes
////////////////////////////////////////////////////////////////////////////////

        void handleCommand (std::vector<struct TRI_vocbase_col_s*> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum chunk size
////////////////////////////////////////////////////////////////////////////////

        static const uint64_t defaultChunkSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum chunk size
////////////////////////////////////////////////////////////////////////////////

        static const uint64_t maxChunkSize;

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
