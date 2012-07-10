////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
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

#include "ApplicationUserManager.h"

#include "build.h"

#include "Admin/RestHandlerCreator.h"
#include "Basics/ProgramOptionsDescription.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/HttpResponse.h"
#include "UserManager/Role.h"
#include "UserManager/Session.h"
#include "UserManager/SessionHandler.h"
#include "UserManager/User.h"
#include "UserManager/UserHandler.h"
#include "UserManager/UsersHandler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                          static private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief option for the path of the user database
////////////////////////////////////////////////////////////////////////////////

string ApplicationUserManager::optionUserDatabase;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationUserManager::ApplicationUserManager () 
  : ApplicationFeature("user manager") {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationUserManager::~ApplicationUserManager () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers for administration
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

void ApplicationUserManager::addHandlers (HttpHandlerFactory* factory, string const& prefix) {
  factory->addPrefixHandler(prefix + "/user-manager/user", UserHandler::create, this);
  factory->addPrefixHandler(prefix + "/user-manager/users", UsersHandler::create, this);
    
  factory->addPrefixHandler(prefix + "/user-manager/session", SessionHandler::create, this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a role
////////////////////////////////////////////////////////////////////////////////

bool ApplicationUserManager::createRole (string const& name,
                                         vector<right_t> const& rights,
                                         right_t rightToManage) {
  Role* role = Role::create(name, rightToManage);
  
  if (role == 0) {
    LOGGER_WARNING << "cannot create role '" << name << "'";
    return false;
  }
  
  role->setRights(rights);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a user
////////////////////////////////////////////////////////////////////////////////

bool ApplicationUserManager::createUser (string const& name,
                                         string const& rolename) {
  Role* role = Role::lookup(rolename);
  
  if (role == 0) {
    LOGGER_WARNING << "cannot create user '" << name << "', unknown role '" << rolename << "'";
    return false;
  }
  
  User* user = User::create(name, role);
  
  return user != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads the user database
////////////////////////////////////////////////////////////////////////////////

bool ApplicationUserManager::loadUser () {
  LOGGER_DEBUG << "trying to load user database '" << optionUserDatabase << "'";
  
  if (optionUserDatabase.empty()) {
    return false;
  }
  
  return User::loadUser(optionUserDatabase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads all users
////////////////////////////////////////////////////////////////////////////////

void ApplicationUserManager::unloadUsers () {
  User::unloadUsers();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads all roles
////////////////////////////////////////////////////////////////////////////////

void ApplicationUserManager::unloadRoles () {
  Role::unloadRoles();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the right of an anonymous session
////////////////////////////////////////////////////////////////////////////////

void ApplicationUserManager::setAnonymousRights (vector<right_t> const& rights) {
  Session::setAnonymousRights(rights);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationUserManager::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("server.user-database", &optionUserDatabase, "file for storing the user database")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
