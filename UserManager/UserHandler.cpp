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

#include "UserHandler.h"

#include <Admin/ApplicationAdminServer.h>
#include <Basics/VariantArray.h>
#include <Basics/VariantBoolean.h>
#include <Basics/VariantInt32.h>
#include <Basics/VariantVector.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

#include "UserManager/User.h"
#include "UserManager/Role.h"
#include "SessionManager/Session.h"

using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    UserHandler::UserHandler (HttpRequest* request, ApplicationAdminServer* server)
      : RestAdminBaseHandler(request), _server(server) {
    }

    // -----------------------------------------------------------------------------
    // HttpHandler methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e UserHandler::execute () {

      // extract the username
      vector<string> const& suffix = request->suffix();

      if (suffix.size() != 1) {
        generateError(HttpResponse::BAD, "expecting <prefix>/user/<username>");
        return HANDLER_DONE;
      }

      string const& name = suffix[0];

      // execute the request
      switch (request->requestType()) {
        case HttpRequest::HTTP_REQUEST_POST: return executePost(name);
        case HttpRequest::HTTP_REQUEST_GET: return executeGet(name);
        case HttpRequest::HTTP_REQUEST_PUT: return executePut(name);
        case HttpRequest::HTTP_REQUEST_DELETE: return executeDelete(name);

        default:
          generateError(HttpResponse::METHOD_NOT_ALLOWED, "method not supported");
          return HANDLER_DONE;
      }
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e UserHandler::executePost (string const& name) {
      struct Description : public InputParser::ObjectDescription {
        Description () {
          attribute("role", role);
          attribute("password", password);
        }

        string role;
        string password;
      } desc;

      bool ok = parseBody(desc);

      if (! ok) {
        return HANDLER_DONE;
      }

      Role* role = Role::lookup(desc.role);

      if (role == 0) {
        generateError(HttpResponse::NOT_FOUND, "role '" + desc.role + "' not found");
        return HANDLER_DONE;
      }

      ok = hasRight(role->rightToManage());

      if (! ok) {
        generateError(HttpResponse::UNAUTHORIZED, "no permission to create user with role '" + desc.role + "'");
        return HANDLER_DONE;
      }

      User* user = User::create(name, role);

      if (user == 0) {
        generateError(HttpResponse::UNAUTHORIZED, "cannot create user '" + name + "'");
        return HANDLER_DONE;
      }

      user->changePassword(desc.password);

      generateUser(user);
      return HANDLER_DONE;
    }



    HttpHandler::status_e UserHandler::executeGet (string const& name) {
      User* user = User::lookup(name);

      if (user == 0) {
        generateError(HttpResponse::NOT_FOUND, "user '" + name + "' not found");
        return HANDLER_DONE;
      }

      generateUser(user);
      return HANDLER_DONE;
    }



    HttpHandler::status_e UserHandler::executePut (string const& name) {
      if (! isSelf(name)) {
        generateError(HttpResponse::UNAUTHORIZED, "cannot manage password for user '" + name + "'");
        return HANDLER_DONE;
      }

      User* user = User::lookup(name);

      if (user == 0) {
        generateError(HttpResponse::NOT_FOUND, "user '" + name + "' not found");
        return HANDLER_DONE;
      }

      struct Description : public InputParser::ObjectDescription {
        Description () {
          optional("password", password, hasPassword);
        }

        bool hasPassword;
        string password;
      } desc;

      bool ok = parseBody(desc);

      if (! ok) {
        return HANDLER_DONE;
      }

      bool changed = true;

      if (desc.hasPassword) {
        changed = user->changePassword(desc.password);
      }

      VariantArray* result = new VariantArray();
      result->add("changed", new VariantBoolean(changed));

      generateResult(result);
      return HANDLER_DONE;
    }



    HttpHandler::status_e UserHandler::executeDelete (string const& name) {
      User* user = User::lookup(name);

      bool removed = false;

      if (user != 0) {
        bool ok = hasRight(user->getRole()->rightToManage());

        if (! ok) {
          generateError(HttpResponse::UNAUTHORIZED,
                        "no permission to create user with role '" + user->getRole()->getName() + "'");
          return HANDLER_DONE;
        }

        removed = User::remove(user);
      }

      VariantArray* result = new VariantArray();
      result->add("removed", new VariantBoolean(removed));

      generateResult(result);
      return HANDLER_DONE;
    }



    void UserHandler::generateUser (User* user) {
      VariantArray* result = new VariantArray();
      result->add("role", user->getRole()->getName());
      result->add("name", user->getName());

      set<right_t> const& rights = user->getRole()->getRights();

      VariantVector* r = new VariantVector();

      for (set<right_t>::const_iterator i = rights.begin();  i != rights.end();  ++i) {
        r->add(new VariantInt32(*i));
      }

      result->add("rights", r);

      generateResult(result);
    }
  }
}
