////////////////////////////////////////////////////////////////////////////////
/// @brief connection info
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

#ifndef ARANGODB_REST_CONNECTION_INFO_H
#define ARANGODB_REST_CONNECTION_INFO_H 1

#include "Basics/Common.h"

#include "Basics/StringUtils.h"
#include "Rest/Endpoint.h"

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief connection info
////////////////////////////////////////////////////////////////////////////////

    struct ConnectionInfo {
      public:
        ConnectionInfo ()
          : serverPort(0),
            clientPort(0),
            serverAddress(),
            clientAddress(),
            endpoint(),
            endpointType(Endpoint::DOMAIN_UNKNOWN),
            sslContext(nullptr) {
        }

      public:

        std::string portType () const {
          if (endpointType == Endpoint::DOMAIN_UNIX) {
            return "unix";
          }
          else if (endpointType == Endpoint::DOMAIN_IPV4 ||
                   endpointType == Endpoint::DOMAIN_IPV6) {
            return "tcp/ip";
          }
          return "unknown";
        }

        int serverPort;
        int clientPort;

        std::string serverAddress;
        std::string clientAddress;
        std::string endpoint;
        Endpoint::DomainType endpointType;

        void* sslContext;
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
