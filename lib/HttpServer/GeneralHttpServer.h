////////////////////////////////////////////////////////////////////////////////
/// @brief general http server
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_GENERAL_HTTP_SERVER_H
#define ARANGODB_HTTP_SERVER_GENERAL_HTTP_SERVER_H 1

#include "Basics/Common.h"

#include "GeneralServer/GeneralServerDispatcher.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                           class GeneralHttpServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief http server implementation
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralHttpServer : public GeneralServerDispatcher<S, HF, CT> {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general http server
////////////////////////////////////////////////////////////////////////////////

        GeneralHttpServer (Scheduler* scheduler,
                           Dispatcher* dispatcher,
                           AsyncJobManager* jobManager,
                           double keepAliveTimeout,
                           HF* handlerFactory)
        : GeneralServer<S, HF, CT>(scheduler, keepAliveTimeout),
          GeneralServerDispatcher<S, HF, CT>(scheduler, dispatcher, jobManager, keepAliveTimeout),
          _handlerFactory(handlerFactory) {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the handler factory
////////////////////////////////////////////////////////////////////////////////

        HF* getHandlerFactory () const {
          return _handlerFactory;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

        HF* _handlerFactory;

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
