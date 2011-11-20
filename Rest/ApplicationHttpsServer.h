////////////////////////////////////////////////////////////////////////////////
/// @brief application https server feature
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_APPLICATION_HTTPS_SERVER_H
#define TRIAGENS_REST_APPLICATION_HTTPS_SERVER_H 1

#include <Rest/ApplicationFeature.h>

#include <Rest/AddressPort.h>

namespace triagens {
  namespace rest {
    class ApplicationServer;
    class HttpHandlerFactory;
    class HttpsServer;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief application https server feature
    ////////////////////////////////////////////////////////////////////////////////

    class ApplicationHttpsServer : public ApplicationFeature {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief creates a new feature
        ////////////////////////////////////////////////////////////////////////////////

        static ApplicationHttpsServer* create (ApplicationServer*);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief shows the port options
        ////////////////////////////////////////////////////////////////////////////////

        virtual void showPortOptions (bool value = true) = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a https address:port
        ////////////////////////////////////////////////////////////////////////////////

        virtual AddressPort addPort (string const&) = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief builds the https server
        ///
        /// Note that the server claims ownership of the factory.
        ////////////////////////////////////////////////////////////////////////////////

        virtual HttpsServer* buildServer (HttpHandlerFactory*) = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief builds the https server
        ///
        /// Note that the server claims ownership of the factory.
        ////////////////////////////////////////////////////////////////////////////////

        virtual HttpsServer* buildServer (HttpHandlerFactory*, vector<AddressPort> const&) = 0;
    };
  }
}

#endif
