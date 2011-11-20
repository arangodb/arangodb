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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SessionHandler.h"

#include <Admin/ApplicationAdminServer.h>
#include <Basics/MutexLocker.h>
#include <Basics/VariantArray.h>
#include <Basics/VariantBoolean.h>
#include <Basics/VariantInt32.h>
#include <Basics/VariantVector.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

#include "SessionManager/Session.h"

#include "UserManager/Role.h"
#include "UserManager/User.h"

using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SessionHandler::SessionHandler (HttpRequest* request, ApplicationAdminServer* server)
      : RestBaseHandler(request), _server(server) {
    }

    // -----------------------------------------------------------------------------
    // HttpHandler methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e SessionHandler::execute () {

      // execute the request
      switch (request->requestType()) {
        case HttpRequest::HTTP_REQUEST_POST: return executePost();
        case HttpRequest::HTTP_REQUEST_GET: return executeGet();
        case HttpRequest::HTTP_REQUEST_PUT: return executePut();
        case HttpRequest::HTTP_REQUEST_DELETE: return executeDelete();

        default:
          generateError(HttpResponse::METHOD_NOT_ALLOWED, "method not supported");
          return HANDLER_DONE;
      }
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e SessionHandler::executePost () {
      if (request->suffix().size() != 0) {
        generateError(HttpResponse::BAD, "expecting POST <prefix>/session");
        return HANDLER_DONE;
      }

      // create a new session
      MUTEX_LOCKER(Session::lock);

      Session* session = Session::create();

      generateSession(session);
      return HANDLER_DONE;
    }



    HttpHandler::status_e SessionHandler::executeGet () {
      vector<string> const& suffix = request->suffix();

      if (suffix.size() != 1) {
        generateError(HttpResponse::BAD, "expecting GET <prefix>/session/<sid>");
        return HANDLER_DONE;
      }

      string const& sid = suffix[0];

      // lookup an existing session
      MUTEX_LOCKER(Session::lock);

      Session* session = Session::lookup(sid);

      if (session != 0) {
        generateSession(session);
      }
      else {
        generateError(HttpResponse::NOT_FOUND, "unknown session");
      }

      return HANDLER_DONE;
    }



    HttpHandler::status_e SessionHandler::executePut () {
      vector<string> const& suffix = request->suffix();

      if (suffix.size() != 2) {
        generateError(HttpResponse::BAD, "expecting PUT <prefix>/session/<sid>/<method>");
        return HANDLER_DONE;
      }

      string const& sid = suffix[0];
      string const& method = suffix[1];

      // lookup an existing session
      MUTEX_LOCKER(Session::lock);

      Session* session = Session::lookup(sid);

      if (session != 0) {
        if (method == "login") {
          return executeLogin(session);
        }
        else if (method == "logout") {
          return executeLogout(session);
        }
        else if (method == "password") {
          return executePassword(session);
        }

        generateError(HttpResponse::BAD, "unknown method '" + method + "'");
        return HANDLER_DONE;
      }
      else {
        generateError(HttpResponse::NOT_FOUND, "unknown session");
      }

      return HANDLER_DONE;
    }



    HttpHandler::status_e SessionHandler::executeDelete () {
      vector<string> const& suffix = request->suffix();

      if (suffix.size() != 1) {
        generateError(HttpResponse::BAD, "expecting DELETE <prefix>/session/<sid>");
        return HANDLER_DONE;
      }

      string const& sid = suffix[0];

      // lookup an existing session
      MUTEX_LOCKER(Session::lock);

      Session* session = Session::lookup(sid);
      bool removed = false;

      if (session != 0) {
        removed = Session::remove(session);
      }

      VariantArray* result = new VariantArray();
      result->add("removed", new VariantBoolean(removed));

      generateResult(result);
      return HANDLER_DONE;
    }




    HttpHandler::status_e SessionHandler::executeLogin (Session* session) {
      struct Description : public InputParser::ObjectDescription {
        Description () {
          attribute("user", user);
          attribute("password", password);
        }

        string user;
        string password;
      } desc;

      bool ok = parseBody(desc);

      if (! ok) {
        return HANDLER_DONE;
      }

      string reason;
      ok = session->login(desc.user, desc.password, reason);

      if (! ok) {
        generateError(HttpResponse::UNAUTHORIZED, reason);
        return HANDLER_DONE;
      }

      generateSession(session);
      return HANDLER_DONE;
    }



    HttpHandler::status_e SessionHandler::executeLogout (Session* session) {
      session->logout();

      generateSession(session);
      return HANDLER_DONE;
    }



    HttpHandler::status_e SessionHandler::executePassword (Session* session) {
      User* user = session->getUser();

      if (user == 0) {
        generateError(HttpResponse::BAD, "session has not bound to user");
        return HANDLER_DONE;
      }

      struct Description : public InputParser::ObjectDescription {
        Description () {
          attribute("password", password);
        }

        string password;
      } desc;

      bool ok = parseBody(desc);

      if (! ok) {
        return HANDLER_DONE;
      }

      bool changed = user->changePassword(desc.password);

      VariantArray* result = new VariantArray();
      result->add("changed", new VariantBoolean(changed));

      generateResult(result);
      return HANDLER_DONE;
    }



    void SessionHandler::generateSession (Session* session) {
      VariantArray* result = new VariantArray();
      result->add("sid", session->getSid());

      User* user = session->getUser();

      if (user != 0) {
        result->add("user", user->getName());
      }

      VariantVector* r = new VariantVector();

      set<right_t> const& rights = (user == 0) ? Session::anonymousRights() : user->getRole()->getRights();

      for (set<right_t>::const_iterator i = rights.begin();  i != rights.end();  ++i) {
        r->add(new VariantInt32(*i));
      }

      result->add("rights", r);

      generateResult(result);
    }

  }
}
