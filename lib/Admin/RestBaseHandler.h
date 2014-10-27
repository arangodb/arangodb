////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ADMIN_REST_BASE_HANDLER_H
#define ARANGODB_ADMIN_REST_BASE_HANDLER_H 1

#include "Basics/Common.h"

#include "HttpServer/HttpHandler.h"

#include "Basics/json.h"
#include "Rest/HttpResponse.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             class RestBaseHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace admin {

////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
////////////////////////////////////////////////////////////////////////////////

    class RestBaseHandler : public rest::HttpHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestBaseHandler (rest::HttpRequest* request);

// -----------------------------------------------------------------------------
// --SECTION--                                               HttpHandler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void generateResult (TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void generateResult (rest::HttpResponse::HttpResponseCode,
                                     TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

        virtual void generateCanceled ();

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

        virtual void generateError (rest::HttpResponse::HttpResponseCode,
                                    int);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

        virtual void generateError (rest::HttpResponse::HttpResponseCode,
                                    int,
                                    std::string const&);

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
