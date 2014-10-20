////////////////////////////////////////////////////////////////////////////////
/// @brief job control request handler
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

#ifndef ARANGODB_ADMIN_REST_JOB_HANDLER_H
#define ARANGODB_ADMIN_REST_JOB_HANDLER_H 1

#include "Basics/Common.h"

#include "Admin/RestBaseHandler.h"
#include "HttpServer/AsyncJobManager.h"

namespace triagens {
  namespace rest {
    class AsyncJobManager;
    class Dispatcher;
  }

  namespace admin {

// -----------------------------------------------------------------------------
// --SECTION--                                              class RestJobHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief job control request handler
////////////////////////////////////////////////////////////////////////////////

    class RestJobHandler : public RestBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestJobHandler (rest::HttpRequest* request,
                        pair<rest::Dispatcher*, rest::AsyncJobManager*>*);

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief put handler
////////////////////////////////////////////////////////////////////////////////

        void putJob ();

////////////////////////////////////////////////////////////////////////////////
/// @brief put method handler
////////////////////////////////////////////////////////////////////////////////

        void putJobMethod ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get handler
////////////////////////////////////////////////////////////////////////////////

        void getJob ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get a job's status by its id
////////////////////////////////////////////////////////////////////////////////

        void getJobId (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get job status by type
////////////////////////////////////////////////////////////////////////////////

        void getJobType (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief delete handler
////////////////////////////////////////////////////////////////////////////////

        void deleteJob ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher
////////////////////////////////////////////////////////////////////////////////

        rest::Dispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief async job manager
////////////////////////////////////////////////////////////////////////////////

        rest::AsyncJobManager* _jobManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

        static const std::string QUEUE_NAME;

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
