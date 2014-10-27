////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown request handler
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ADMIN_REST_SHUTDOWN_HANDLER_H
#define ARANGODB_ADMIN_REST_SHUTDOWN_HANDLER_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationServer.h"

#include "Admin/RestBaseHandler.h"

#include "Rest/HttpResponse.h"

namespace triagens {
  namespace admin {

// -----------------------------------------------------------------------------
// --SECTION--                                         class RestShutdownHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown request handler
////////////////////////////////////////////////////////////////////////////////

    class RestShutdownHandler : public RestBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestShutdownHandler (rest::HttpRequest*, void* applicationServer);

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
/// @brief initiates the shutdown process
////////////////////////////////////////////////////////////////////////////////

        status_t execute ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

        static const std::string QUEUE_NAME;

////////////////////////////////////////////////////////////////////////////////
/// @brief application server
////////////////////////////////////////////////////////////////////////////////

        triagens::rest::ApplicationServer* _applicationServer;
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
