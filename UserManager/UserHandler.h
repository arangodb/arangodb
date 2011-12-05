////////////////////////////////////////////////////////////////////////////////
/// @brief user handler
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BORNHOLM_USER_MANAGER_USER_HANDLER_H
#define TRIAGENS_BORNHOLM_USER_MANAGER_USER_HANDLER_H 1

#include <Admin/RestAdminBaseHandler.h>

#include <Admin/Right.h>

namespace triagens {
  namespace admin {
    class ApplicationAdminServer;
    class User;
    class Session;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief user handler
    ////////////////////////////////////////////////////////////////////////////////

    class UserHandler : public RestAdminBaseHandler {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief static constructor
        ////////////////////////////////////////////////////////////////////////////////

        static rest::HttpHandler* create (rest::HttpRequest* request, void* data) {
          return new UserHandler(request, (ApplicationAdminServer*) data);
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

        UserHandler (rest::HttpRequest* request, ApplicationAdminServer*);

      private:
        status_e executePost (string const& name);
        status_e executeGet (string const& name);
        status_e executePut (string const& name);
        status_e executeDelete (string const& name);

        void generateUser (User*);

      private:
        ApplicationAdminServer* _server;
    };
  }
}

#endif
