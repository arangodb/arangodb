////////////////////////////////////////////////////////////////////////////////
/// @brief session handler
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
/// @author Martin Schoenert
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SessionHandler.h"

#include "Basics/MutexLocker.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "UserManager/Role.h"
#include "UserManager/Session.h"
#include "UserManager/User.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantInt32.h"
#include "Variant/VariantVector.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief static constructor
////////////////////////////////////////////////////////////////////////////////

HttpHandler* SessionHandler::create (rest::HttpRequest* request, void* data) {
  return new SessionHandler(request, (ApplicationAdminServer*) data);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

SessionHandler::SessionHandler (HttpRequest* request, ApplicationAdminServer* server)
  : RestBaseHandler(request), _server(server) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool SessionHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::execute () {

  // execute the request
  switch (request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST: return executePost();
    case HttpRequest::HTTP_REQUEST_GET: return executeGet();
    case HttpRequest::HTTP_REQUEST_PUT: return executePut();
    case HttpRequest::HTTP_REQUEST_DELETE: return executeDelete();
      
    default:
      generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return HANDLER_DONE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a session
///
/// @REST{POST /_admin/user-manager/session}
///
/// Creates a new session. Returns an object with the following attributes.
///
/// - @LIT{sid}: The session identifier.
///
/// - @LIT{rights}: A list of rights for the newly created session.
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executePost () {
  if (request->suffix().size() != 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID1);
    return HANDLER_DONE;
  }
  
  // create a new session
  MUTEX_LOCKER(Session::lock);
  
  Session* session = Session::create();
  
  generateSession(session);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a session
///
/// @REST{GET /_admin/user-manager/session/@FA{sid}}
///
/// Returns an object with the following attributes describing the session
/// @FA{sid}.
///
/// - @LIT{sid}: The session identifier.
///
/// - @LIT{rights}: A list of rights for the newly created session.
///
/// Note that @LIT{HTTP 404} is returned, if the session is unknown or expired.
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executeGet () {
  vector<string> const& suffix = request->suffix();
  
  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID2);
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
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_SESSION_SESSIONHANDLER_SESSION_UNKNOWN);
  }
  
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a session
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executePut () {
  vector<string> const& suffix = request->suffix();
  
  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID3);
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
    
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return HANDLER_DONE;
  }
  else {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_SESSION_SESSIONHANDLER_SESSION_UNKNOWN);
  }
  
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a session
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executeDelete () {
  vector<string> const& suffix = request->suffix();
  
  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID4);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief logs in an user
///
/// @REST{PUT /_admin/user-manager/session/@FA{sid}/login}
///
/// Logs an user into an existing session. Expects an object with the following
/// attributes.
///
/// - @LIT{user}: The user name.
///
/// - @LIT{password}: The password. The password must be a SHA256 hash of the
///   real password.
////////////////////////////////////////////////////////////////////////////////

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
    generateError(HttpResponse::UNAUTHORIZED, TRI_ERROR_SESSION_SESSIONHANDLER_CANNOT_LOGIN, reason);
    return HANDLER_DONE;
  }
  
  generateSession(session);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs out an user
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executeLogout (Session* session) {
  session->logout();

  generateSession(session);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the password
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e SessionHandler::executePassword (Session* session) {
  User* user = session->getUser();

  if (user == 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_SESSIONHANDLER_SESSION_NOT_BOUND);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new session
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
