////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationAdminServer.h"

#include "Admin/RestAdminLogHandler.h"
#include "Admin/RestJobHandler.h"
#include "Admin/RestHandlerCreator.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/logging.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/PathHandler.h"
#include "Rest/HttpResponse.h"

using namespace std;
using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief variables to hold legacy options (unused, but kept here so that
/// starting the server with deprecated options doesn't fail
////////////////////////////////////////////////////////////////////////////////

static string UnusedAdminDirectory;

static bool UnusedDisableAdminInterface;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationAdminServer::ApplicationAdminServer ()
  : ApplicationFeature("admin"),
    _allowLogViewer(false),
    _pathOptions(0),
    _jobPayload(0) {
  _pathOptions = new PathHandler::Options();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationAdminServer::~ApplicationAdminServer () {
  if (_jobPayload != 0) {
    delete _jobPayload;
  }

  delete reinterpret_cast<PathHandler::Options*>(_pathOptions);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a log viewer
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::allowLogViewer () {
  _allowLogViewer = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the http handlers
///
/// Note that the server does not claim ownership of the factory.
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::addBasicHandlers (HttpHandlerFactory* factory,
                                               string const& prefix,
                                               Dispatcher* dispatcher,
                                               AsyncJobManager* jobManager) {
  factory->addHandler(prefix + "/version", RestHandlerCreator<RestVersionHandler>::createNoData, 0);

  if (_jobPayload == 0) {
    _jobPayload = new pair<Dispatcher*, AsyncJobManager*>(dispatcher, jobManager);
  }

  factory->addPrefixHandler(prefix + "/job",
                            RestHandlerCreator<RestJobHandler>::createData< pair<Dispatcher*, AsyncJobManager*>* >,
                            _jobPayload);
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
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationAdminServer::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  // deprecated options
  options[ApplicationServer::OPTIONS_HIDDEN]
    ("server.admin-directory", &UnusedAdminDirectory, "directory containing the ADMIN front-end (deprecated)")
    ("server.disable-admin-interface", &UnusedDisableAdminInterface, "turn off the HTML admin interface (deprecated)")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationAdminServer::prepare () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationAdminServer::parsePhase2 (ProgramOptions& options) {
  if (options.has("server.admin-directory")) {
    LOG_WARNING("usage of obsolete option --server.admin-directory");
  }

  if (options.has("server.disable-admin-interface")) {
    LOG_WARNING("usage of obsolete option --server.disable-admin-interface");
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
