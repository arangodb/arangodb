////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
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

#include "ApplicationAdminServer.h"

#include "build.h"

#include <Admin/RestAdminFeConfigurationHandler.h>
#include <Admin/RestAdminLogHandler.h>
#include <Admin/RestHandlerCreator.h>
#include <Admin/RestVersionHandler.h>
#include <Rest/HttpHandlerFactory.h>
#include <Rest/HttpResponse.h>
#include <Basics/ProgramOptionsDescription.h>

#include "HttpServer/PathHandler.h"

#include "SessionManager/Session.h"
#include "SessionManager/SessionHandler.h"


#include "UserManager/Role.h"
#include "UserManager/User.h"
#include "UserManager/UserHandler.h"
#include "UserManager/UsersHandler.h"


using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // static variables
    // -----------------------------------------------------------------------------

    string ApplicationAdminServer::optionAdminDirectory;
    string ApplicationAdminServer::optionUserDatabase;

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationAdminServer* ApplicationAdminServer::create (ApplicationServer*) {
      return new ApplicationAdminServer();
    }



    ApplicationAdminServer::ApplicationAdminServer ()
      : _allowSessionManagement(false),
        _allowLogViewer(false),
        _allowAdminDirectory(false),
        _allowFeConfiguration(false),
        _allowVersion(false) {
      _pathOptions = new PathHandler::Options();
    }



    ApplicationAdminServer::~ApplicationAdminServer () {
      delete reinterpret_cast<PathHandler::Options*>(_pathOptions);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationAdminServer::allowVersion () {
      _allowVersion = true;
      _version = TRIAGENS_VERSION;
    }


    void ApplicationAdminServer::allowVersion (string name, string version) {
      _allowVersion = true;
      _name = name;
      _version = version;
    }


    void ApplicationAdminServer::addBasicHandlers (HttpHandlerFactory* factory) {

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // add a version handler
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      if (_allowVersion) {
        pair<string, string>* data = new pair<string,string>(_name, _version);
        factory->addHandler("/version", RestHandlerCreator<RestVersionHandler>::createData<pair<string,string> const*>, (void*) data);
      }
    }



    void ApplicationAdminServer::addHandlers (HttpHandlerFactory* factory, string const& prefix) {

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // add session management
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      if (_allowSessionManagement) {
        factory->addPrefixHandler(prefix + "/user-manager/user", UserHandler::create, this);
        factory->addPrefixHandler(prefix + "/user-manager/users", UsersHandler::create, this);

        factory->addPrefixHandler(prefix + "/session-manager/session", SessionHandler::create, this);
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // add log viewer
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      if (_allowLogViewer) {
        factory->addHandler("/admin/log", RestHandlerCreator<RestAdminLogHandler>::createNoData, 0);
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // add a web-admin directory
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      if (_allowAdminDirectory) {
        if (! optionAdminDirectory.empty()) {
          reinterpret_cast<PathHandler::Options*>(_pathOptions)->path = optionAdminDirectory;
          reinterpret_cast<PathHandler::Options*>(_pathOptions)->contentType = "text/plain";
          reinterpret_cast<PathHandler::Options*>(_pathOptions)->allowSymbolicLink = false;
          reinterpret_cast<PathHandler::Options*>(_pathOptions)->defaultFile = "index.html";

          factory->addPrefixHandler(prefix, RestHandlerCreator<PathHandler>::createData<PathHandler::Options*>, _pathOptions);
        }
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // add a front-end configuration
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      if (_allowFeConfiguration) {
        factory->addHandler(prefix + "/fe-configuration", RestHandlerCreator<RestAdminFeConfigurationHandler>::createData<char const*>, (void*) _feConfiguration.c_str());
      }
    }




    bool ApplicationAdminServer::createRole (string const& name,
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



    bool ApplicationAdminServer::createUser (string const& name,
                                             string const& rolename) {
      Role* role = Role::lookup(rolename);

      if (role == 0) {
        LOGGER_WARNING << "cannot create user '" << name << "', unknown role '" << rolename << "'";
        return false;
      }

      User* user = User::create(name, role);

      return user != 0;
    }



    bool ApplicationAdminServer::loadUser () {
      LOGGER_DEBUG << "trying to load user database '" << optionUserDatabase << "'";

      if (optionUserDatabase.empty()) {
        return false;
      }

      return User::loadUser(optionUserDatabase);
    }




    void ApplicationAdminServer::setAnonymousRights (vector<right_t> const& rights) {
      Session::setAnonymousRights(rights);
    }

    // -----------------------------------------------------------------------------
    // ApplicationFeature methods
    // -----------------------------------------------------------------------------

    void ApplicationAdminServer::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
      if (_allowAdminDirectory) {
        options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
          ("server.admin-directory", &optionAdminDirectory, "directory containing the ADMIN front-end")
        ;
      }

      if (_allowFeConfiguration) {
        options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
          ("server.fe-configuration", &_feConfiguration, "file to store the front-end preferences")
        ;
      }

      if (_allowSessionManagement) {
        options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
          ("server.user-database", &optionUserDatabase, "file for storing the user database")
        ;
      }
    }
  }
}
