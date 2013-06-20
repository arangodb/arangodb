////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationAdminServer.h"

#include "build.h"

#include "Admin/RestAdminLogHandler.h"
#include "Admin/RestHandlerCreator.h"
#include "Basics/ProgramOptionsDescription.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/PathHandler.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"

using namespace std;
using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

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

ApplicationAdminServer::ApplicationAdminServer ()
  : ApplicationFeature("admin"),
    _allowLogViewer(false),
    _allowAdminDirectory(false),
    _allowVersion(false),
    _adminDirectory(),
    _pathOptions(0),
    _name(),
    _version(),
#ifdef TRI_ENABLE_MAINTAINER_MODE
    _versionDataQueued(0),
#endif
    _versionDataDirect(0) {
  _pathOptions = new PathHandler::Options();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationAdminServer::~ApplicationAdminServer () {
  delete reinterpret_cast<PathHandler::Options*>(_pathOptions);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  if (_versionDataQueued != 0) {
    delete _versionDataQueued;
  }
#endif

  if (_versionDataDirect != 0) {
    delete _versionDataDirect;
  }
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
/// @brief add a log viewer
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowLogViewer () {
  _allowLogViewer = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows for a webadmin directory
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowAdminDirectory () {
  _allowAdminDirectory = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows or disallows webadmin directory
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowAdminDirectory (bool value) {
  _allowAdminDirectory = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows for a webadmin directory
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowAdminDirectory (string const& adminDirectory) {
  _allowAdminDirectory = true;
  _adminDirectory = adminDirectory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows for a version handler using the default version
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowVersion () {
  _allowVersion = true;
  _version = TRIAGENS_VERSION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows for a version handler
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowVersion (string name, string version) {
  _allowVersion = true;
  _name = name;
  _version = version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::addBasicHandlers (HttpHandlerFactory* factory, string const& prefix) {
#if TRI_ENABLE_MAINTAINER_MODE
    // this handler does not provide any real benefit. we only use it to compare
    // the performance of direct vs. the performance of queued execution
    if (! _versionDataQueued) {
      _versionDataQueued = new RestVersionHandler::version_options_t;
      _versionDataQueued->_name = _name;
      _versionDataQueued->_version = _version;
      _versionDataQueued->_isDirect = false;
      _versionDataQueued->_queue = "STANDARD";
    }

    factory->addHandler(prefix + "/queued-version",
                        RestHandlerCreator<RestVersionHandler>::createData<RestVersionHandler::version_options_t const*>,
                        (void*) _versionDataQueued);
#endif

  if (_allowVersion) {
    if (! _versionDataDirect) {
      _versionDataDirect = new RestVersionHandler::version_options_t;
      _versionDataDirect->_name = _name;
      _versionDataDirect->_version = _version;
      _versionDataDirect->_isDirect = true;
    }

    factory->addHandler(prefix + "/version",
                        RestHandlerCreator<RestVersionHandler>::createData<RestVersionHandler::version_options_t const*>,
                        (void*) _versionDataDirect);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers for administration
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::addHandlers (HttpHandlerFactory* factory, string const& prefix) {

  // .............................................................................
  // add log viewer
  // .............................................................................

  if (_allowLogViewer) {
    factory->addHandler(prefix + "/log", RestHandlerCreator<RestAdminLogHandler>::createNoData, 0);
  }

  // .............................................................................
  // add a web-admin directory
  // .............................................................................

  if (_allowAdminDirectory) {
    LOGGER_DEBUG("using JavaScript front-end path '" << _adminDirectory << "'");

    reinterpret_cast<PathHandler::Options*>(_pathOptions)->path = _adminDirectory;
    reinterpret_cast<PathHandler::Options*>(_pathOptions)->contentType = "text/plain";
    reinterpret_cast<PathHandler::Options*>(_pathOptions)->allowSymbolicLink = false;
    reinterpret_cast<PathHandler::Options*>(_pathOptions)->defaultFile = "index.html";
    reinterpret_cast<PathHandler::Options*>(_pathOptions)->cacheMaxAge = 86400;

    factory->addPrefixHandler(prefix + "/html", RestHandlerCreator<PathHandler>::createData<PathHandler::Options*>, _pathOptions);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  if (_allowAdminDirectory) {
    options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
      ("server.admin-directory", &_adminDirectory, "directory containing the ADMIN front-end")
    ;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationAdminServer::prepare () {
  if (_allowAdminDirectory && _adminDirectory.empty()) {
    LOGGER_FATAL_AND_EXIT("you must specify an admin directory, giving up!");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
