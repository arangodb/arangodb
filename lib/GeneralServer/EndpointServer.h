////////////////////////////////////////////////////////////////////////////////
/// @brief base class for all servers
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GENERAL_SERVER_ENDPOINT_SERVER_H
#define ARANGODB_GENERAL_SERVER_ENDPOINT_SERVER_H 1

#include "Basics/Common.h"

#include "lib/Rest/Endpoint.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              class EndpointServer
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

    class EndpointList;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint server
////////////////////////////////////////////////////////////////////////////////

    class EndpointServer {
      private:
        EndpointServer (EndpointServer const&);
        EndpointServer const& operator= (EndpointServer const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new endpoint server
////////////////////////////////////////////////////////////////////////////////

        explicit
        EndpointServer () :
          _endpointList(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

        virtual ~EndpointServer () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the encryption to be used
////////////////////////////////////////////////////////////////////////////////

        virtual Endpoint::EncryptionType getEncryption () const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief add the endpoint list
////////////////////////////////////////////////////////////////////////////////

        void setEndpointList (const EndpointList* list) {
          _endpointList = list;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add another endpoint at runtime
////////////////////////////////////////////////////////////////////////////////

        virtual bool addEndpoint (Endpoint*) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an endpoint at runtime
////////////////////////////////////////////////////////////////////////////////

        virtual bool removeEndpoint (Endpoint*) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listening
////////////////////////////////////////////////////////////////////////////////

        virtual void startListening () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down handlers
////////////////////////////////////////////////////////////////////////////////

        virtual void shutdownHandlers () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listining
////////////////////////////////////////////////////////////////////////////////

        virtual void stopListening () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

        virtual void stop () = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief defined ports and addresses
////////////////////////////////////////////////////////////////////////////////

        const EndpointList* _endpointList;

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
