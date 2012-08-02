////////////////////////////////////////////////////////////////////////////////
/// @brief users handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "UsersHandler.h"

#include "Admin/ApplicationAdminServer.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantString.h"
#include "Variant/VariantVector.h"
#include "Rest/HttpRequest.h"

#include "UserManager/User.h"

using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace admin {

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

    UsersHandler::UsersHandler (HttpRequest* request, ApplicationAdminServer* server)
      : RestAdminBaseHandler(request), _server(server) {
    }

// -----------------------------------------------------------------------------
// HttpHandler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all user names
///
/// @RESTHEADER{GET /_admin/user-manager/users,reads all front-end users}
///
/// @REST{GET /_admin/user-manager/users}
///
/// Returns all user names. The result object contains:
///
/// - @LIT{users}: a list of all user names.
////////////////////////////////////////////////////////////////////////////////

    HttpHandler::status_e UsersHandler::execute () {
      vector<string> const& suffix = _request->suffix();

      if (suffix.size() != 0) {
        generateError(HttpResponse::BAD, TRI_ERROR_SESSION_USERSHANDLER_INVALID_URL);
        return HANDLER_DONE;
      }

      if (_request->requestType() != HttpRequest::HTTP_REQUEST_GET) {
        generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
        return HANDLER_DONE;
      }

      vector<User*> users = User::users();

      VariantArray* result = new VariantArray();
      VariantVector* list = new VariantVector();
      result->add("users", list);

      for (vector<User*>::iterator i = users.begin();  i != users.end();  ++i) {
        list->add((*i)->getName());
      }

      generateResult(result);
      return HANDLER_DONE;
    }
  }
}
