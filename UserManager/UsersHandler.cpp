////////////////////////////////////////////////////////////////////////////////
/// @brief users handler
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

#include "UsersHandler.h"

#include <Admin/ApplicationAdminServer.h>
#include <Basics/VariantArray.h>
#include <Basics/VariantString.h>
#include <Basics/VariantVector.h>
#include <Rest/HttpRequest.h>

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

    HttpHandler::status_e UsersHandler::execute () {
      vector<string> const& suffix = request->suffix();

      if (suffix.size() != 0) {
        generateError(HttpResponse::BAD, "expecting GET <prefix>/users");
        return HANDLER_DONE;
      }

      if (request->requestType() != HttpRequest::HTTP_REQUEST_GET) {
        generateError(HttpResponse::METHOD_NOT_ALLOWED, "expecting GET");
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
