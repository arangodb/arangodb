////////////////////////////////////////////////////////////////////////////////
/// @brief user handler
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

#include "UserHandler.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "UserManager/Role.h"
#include "UserManager/Session.h"
#include "UserManager/User.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantInt32.h"
#include "Variant/VariantVector.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the authenticating session
////////////////////////////////////////////////////////////////////////////////

static Session* authSession (HttpRequest* request) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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

HttpHandler* UserHandler::create (rest::HttpRequest* request, void* data) {
  return new UserHandler(request, (ApplicationUserManager*) data);
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

UserHandler::UserHandler (HttpRequest* request, ApplicationUserManager* server)
  : RestAdminBaseHandler(request), _server(server) {
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

bool UserHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e UserHandler::execute () {

  // extract the username
  vector<string> const& suffix = _request->suffix();
  
  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_SESSION_USERHANDLER_URL_INVALID);
    return HANDLER_DONE;
  }
  
  string const& name = suffix[0];
  
  // execute the request
  switch (_request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST: return executePost(name);
    case HttpRequest::HTTP_REQUEST_GET: return executeGet(name);
    case HttpRequest::HTTP_REQUEST_PUT: return executePut(name);
    case HttpRequest::HTTP_REQUEST_DELETE: return executeDelete(name);
      
    default:
      generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return HANDLER_DONE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if authentication session has given right
////////////////////////////////////////////////////////////////////////////////

bool UserHandler::hasRight (right_t right) {
  Session* session = authSession(_request);

  if (session == 0) {
    return false;
  }

  return session->hasRight(right);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if authentication session is bound to given user
////////////////////////////////////////////////////////////////////////////////

bool UserHandler::isSelf (string const& username) {
  Session* session = authSession(_request);

  if (session == 0) {
    return false;
  }

  User* user = User::lookup(username);

  if (user == 0) {
    return false;
  }

  return session->getUser() != user;
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
/// @brief creates an user
///
/// @RESTHEADER{POST /_admin/user-manager/user,creates a front-end user}
///
/// @REST{POST /_admin/user-manager/user/@FA{user-name}}
///
/// Creates a new user named @FA{user-name}. Expects the following object.
///
/// - @LIT{role}: The role of the newly created user.
///
/// - @LIT{password}: The password of the user. The password must be a SHA256
///   hash of the real password.
///
/// The server will return a @LIT{HTTP 401}, if the current user has no right
/// to create a new user.
///
/// If the user was created a LIT{HTTP 200} is returned and the body contains
/// a description of the user, see @LIT{GET}.
////////////////////////////////////////////////////////////////////////////////

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
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_SESSION_USERHANDLER_ROLE_NOT_FOUND);
    return HANDLER_DONE;
  }
  
  ok = hasRight(role->rightToManage());
  
  if (! ok) {
    generateError(HttpResponse::UNAUTHORIZED, TRI_ERROR_SESSION_USERHANDLER_NO_CREATE_PERMISSION);
    return HANDLER_DONE;
  }
  
  User* user = User::create(name, role);
  
  if (user == 0) {
    generateError(HttpResponse::UNAUTHORIZED, TRI_ERROR_SESSION_USERHANDLER_CANNOT_CREATE_USER);
    return HANDLER_DONE;
  }
  
  user->changePassword(desc.password);
  
  generateUser(user);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an user
///
/// @RESTHEADER{POST /_admin/user-manager/user,gets a front-end user}
///
/// @REST{GET /_admin/user-manager/user/@FA{user-name}}
///
/// Returns information about the user named @FA{user-name}.
///
/// - @LIT{role}: The role of the user. You should not use the role for
///   permissioning. Use the @LIT{rights} instead.
///
/// - @LIT{name}: The name of the user. You should not use the name for
///   permissioning. Use the @LIT{rights} instead.
///
/// - @LIT{rights}: A list of rights.
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e UserHandler::executeGet (string const& name) {
  User* user = User::lookup(name);
  
  if (user == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_SESSION_USERHANDLER_USER_NOT_FOUND);
    return HANDLER_DONE;
  }
  
  generateUser(user);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an user
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e UserHandler::executePut (string const& name) {
  if (! isSelf(name)) {
    generateError(HttpResponse::UNAUTHORIZED, TRI_ERROR_SESSION_USERHANDLER_CANNOT_CHANGE_PW);
    return HANDLER_DONE;
  }
  
  User* user = User::lookup(name);
  
  if (user == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_SESSION_USERHANDLER_USER_NOT_FOUND);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an user
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e UserHandler::executeDelete (string const& name) {
  User* user = User::lookup(name);
  
  bool removed = false;
  
  if (user != 0) {
    bool ok = hasRight(user->getRole()->rightToManage());
    
    if (! ok) {
      generateError(HttpResponse::UNAUTHORIZED, TRI_ERROR_SESSION_USERHANDLER_NO_CREATE_PERMISSION);
      return HANDLER_DONE;
    }
    
    removed = User::remove(user);
  }
  
  VariantArray* result = new VariantArray();
  result->add("removed", new VariantBoolean(removed));
  
  generateResult(result);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an new user
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
