////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for admin handlers
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

#include "RestAdminBaseHandler.h"

#include <Rest/HttpRequest.h>

#include "SessionManager/Session.h"

#include "UserManager/User.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------

namespace {
  Session* authSession (HttpRequest* request) {
    bool found;
    string const& sid = request->value("authSid", found);

    if (! found) {
      return 0;
    }

    Session* session = Session::lookup(sid);

    if (session == 0) {
      return 0;
    }

    return session;
  }
}

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    RestAdminBaseHandler::RestAdminBaseHandler (HttpRequest* request)
      : RestBaseHandler(request) {
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    bool RestAdminBaseHandler::hasRight (right_t right) {
      Session* session = authSession(request);

      if (session == 0) {
        return false;
      }

      return session->hasRight(right);
    }




    bool RestAdminBaseHandler::isSelf (string const& username) {
      Session* session = authSession(request);

      if (session == 0) {
        return false;
      }

      User* user = User::lookup(username);

      if (user == 0) {
        return false;
      }

      return session->getUser() != user;
    }

  }
}
