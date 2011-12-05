////////////////////////////////////////////////////////////////////////////////
/// @brief session handler
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
/// @auther Martin Schoenert
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BORNHOLM_SESSION_MANAGER_SESSION_HANDLER_H
#define TRIAGENS_BORNHOLM_SESSION_MANAGER_SESSION_HANDLER_H 1

#include <Admin/RestBaseHandler.h>

namespace triagens {
  namespace admin {
    class ApplicationAdminServer;
    class Session;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief session handler
    ////////////////////////////////////////////////////////////////////////////////

    class SessionHandler : public RestBaseHandler {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief static constructor
        ////////////////////////////////////////////////////////////////////////////////

        static rest::HttpHandler* create (rest::HttpRequest* request, void* data) {
          return new SessionHandler(request, (ApplicationAdminServer*) data);
        }

      public:


        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool isDirect () {
          return true;
        }


        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        status_e execute ();

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        SessionHandler (rest::HttpRequest* request, ApplicationAdminServer*);

      private:
        status_e executePost ();
        status_e executeGet ();
        status_e executePut ();
        status_e executeDelete ();

        status_e executeLogin (Session*);
        status_e executeLogout (Session*);
        status_e executePassword (Session*);

        void generateSession (Session*);

      private:
        ApplicationAdminServer* _server;
    };
  }
}

#endif
