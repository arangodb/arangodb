////////////////////////////////////////////////////////////////////////////////
/// @brief request context
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_REQUEST_CONTEXT_H
#define ARANGODB_REST_REQUEST_CONTEXT_H 1

#include "Basics/Common.h"

#include "Rest/RequestUser.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

namespace triagens {
  namespace rest {

    class HttpRequest;

// -----------------------------------------------------------------------------
// --SECTION--                                              class RequestContext
// -----------------------------------------------------------------------------

    class RequestContext {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the request context
////////////////////////////////////////////////////////////////////////////////

        RequestContext (HttpRequest* request) :
          _request(request) {
        }

        virtual ~RequestContext () {
        }

    private:

        RequestContext (const RequestContext&);
        RequestContext& operator= (const RequestContext&);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get request user
////////////////////////////////////////////////////////////////////////////////

        virtual RequestUser* getRequestUser () {
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticate user
////////////////////////////////////////////////////////////////////////////////

        virtual HttpResponse::HttpResponseCode authenticate () = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the request of the context
////////////////////////////////////////////////////////////////////////////////

        HttpRequest* _request;

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
