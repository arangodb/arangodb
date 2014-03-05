////////////////////////////////////////////////////////////////////////////////
/// @brief please upgrade handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestPleaseUpgradeHandler.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestPleaseUpgradeHandler::RestPleaseUpgradeHandler (HttpRequest* request)
  : HttpHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestPleaseUpgradeHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestPleaseUpgradeHandler::execute () {
  _response = createResponse(HttpResponse::OK);
  _response->setContentType("text/plain; charset=utf-8");

  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "Database: ");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), _request->databaseName().c_str());
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "\r\n\r\n");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "It appears that your database must be upgrade. ");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "Normally this can be done using\r\n\r\n");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "  /etc/init.d/arangodb stop\r\n");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "  /etc/init.d/arangodb upgrade\r\n");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "  /etc/init.d/arangodb start\r\n\r\n");
  TRI_AppendStringStringBuffer(_response->body().stringBuffer(), "Please check the log file for details.\r\n");

  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void RestPleaseUpgradeHandler::handleError (TriagensError const&) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
