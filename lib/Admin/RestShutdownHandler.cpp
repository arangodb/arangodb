////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown request handler
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestShutdownHandler.h"

#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"

using namespace std;
using namespace triagens::admin;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                          static private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

const string RestShutdownHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestShutdownHandler::RestShutdownHandler (triagens::rest::HttpRequest* request,
                                          void* applicationServer)
  : RestBaseHandler(request),
    _applicationServer(
      static_cast<triagens::rest::ApplicationServer*>(applicationServer)) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestShutdownHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestShutdownHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_initiate
/// @brief initiates the shutdown sequence
///
/// @RESTHEADER{GET /_admin/shutdown, Initiate shutdown sequence}
///
/// @RESTDESCRIPTION
/// This call initiates a clean shutdown sequence.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned in all cases.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestShutdownHandler::execute () {

  _applicationServer->beginShutdown();

  TRI_json_t result;
  TRI_InitStringJson(&result, TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "OK"));
  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);

  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
