////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint, ip-based
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
/// @author Dr. Oreste Costa-Panaia
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_ENDPOINT_IP_H
#define ARANGODB_REST_ENDPOINT_IP_H 1

#include "Basics/Common.h"

#include "Rest/Endpoint.h"

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                                        EndpointIp
// -----------------------------------------------------------------------------

    class EndpointIp : public Endpoint {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an endpoint
////////////////////////////////////////////////////////////////////////////////

      protected:
        EndpointIp (const EndpointType,
                    const DomainType,
                    const EncryptionType,
                    const std::string&,
                    int,
                    bool,
                    const std::string&,
                    const uint16_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint
////////////////////////////////////////////////////////////////////////////////

      public:
        ~EndpointIp ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief default port number if none specified
////////////////////////////////////////////////////////////////////////////////

        static const uint16_t _defaultPort;

////////////////////////////////////////////////////////////////////////////////
/// @brief default host if none specified
////////////////////////////////////////////////////////////////////////////////

        static const std::string _defaultHost;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the socket
////////////////////////////////////////////////////////////////////////////////

        TRI_socket_t connectSocket (const struct addrinfo*, double, double);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

        TRI_socket_t connect (double, double);

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect the endpoint
////////////////////////////////////////////////////////////////////////////////

        virtual void disconnect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

        virtual bool initIncoming (TRI_socket_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief get port
////////////////////////////////////////////////////////////////////////////////

        int getPort () const {
          return _port;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get host
////////////////////////////////////////////////////////////////////////////////

        std::string getHost () const {
          return _host;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to reuse the address
////////////////////////////////////////////////////////////////////////////////

        bool _reuseAddress;

////////////////////////////////////////////////////////////////////////////////
/// @brief host name / address (IPv4 or IPv6)
////////////////////////////////////////////////////////////////////////////////

        std::string _host;

////////////////////////////////////////////////////////////////////////////////
/// @brief port number
////////////////////////////////////////////////////////////////////////////////

        uint16_t _port;
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
