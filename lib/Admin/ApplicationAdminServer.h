////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
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

#ifndef ARANGODB_ADMIN_APPLICATION_ADMIN_SERVER_H
#define ARANGODB_ADMIN_APPLICATION_ADMIN_SERVER_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationFeature.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationServer;
    class AsyncJobManager;
    class Dispatcher;
    class HttpHandlerFactory;
    class HttpResponse;
    class HttpRequest;
  }

  namespace admin {

// -----------------------------------------------------------------------------
// --SECTION--                                      class ApplicationAdminServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationAdminServer : public rest::ApplicationFeature {
      private:
        ApplicationAdminServer (ApplicationAdminServer const&);
        ApplicationAdminServer& operator= (ApplicationAdminServer const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationAdminServer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationAdminServer ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a log viewer
////////////////////////////////////////////////////////////////////////////////

        void allowLogViewer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

        void addBasicHandlers (rest::HttpHandlerFactory*,
                               std::string const &prefix,
                               rest::Dispatcher*,
                               rest::AsyncJobManager*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers for administration
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

        void addHandlers (rest::HttpHandlerFactory*,
                          std::string const& prefix);

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (std::map<std::string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool parsePhase2 (basics::ProgramOptions&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief allow log viewer requests
////////////////////////////////////////////////////////////////////////////////

        bool _allowLogViewer;

////////////////////////////////////////////////////////////////////////////////
/// @brief path options for the admin directory
////////////////////////////////////////////////////////////////////////////////

        void* _pathOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief data that needs to be freed at shutdown
////////////////////////////////////////////////////////////////////////////////

        std::pair<rest::Dispatcher*, rest::AsyncJobManager*>* _jobPayload;

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
