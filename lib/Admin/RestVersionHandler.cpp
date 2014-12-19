////////////////////////////////////////////////////////////////////////////////
/// @brief version request handler
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestVersionHandler.h"

#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Basics/conversions.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

const string RestVersionHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler (HttpRequest* request)
  : RestBaseHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestVersionHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestVersionHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_return
/// @brief returns the server version number
///
/// @RESTHEADER{GET /_api/version, Return server version}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{details,boolean,optional}
/// If set to *true*, the response will contain a *details* attribute with
/// additional information about included components and their versions. The
/// attribute names and internals of the *details* object may vary depending on
/// platform and ArangoDB version.
///
/// @RESTDESCRIPTION
/// Returns the server name and version number. The response is a JSON object
/// with the following attributes:
///
/// - *server*: will always contain *arango*
///
/// - *version*: the server version string. The string has the format
///   "*major*.*minor*.*sub*". *major* and *minor* will be numeric, and *sub*
///   may contain a number or a textual version.
///
/// - *details*: an optional JSON object with additional details. This is
///   returned only if the *details* URL parameter is set to *true* in the
///   request.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned in all cases.
///
/// @EXAMPLES
///
/// Returns the version information.
///
/// @EXAMPLE_ARANGOSH_RUN{RestVersion}
///     var response = logCurlRequest('GET', '/_api/version');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Returns the version information with details.
///
/// @EXAMPLE_ARANGOSH_RUN{RestVersionDetails}
///     var response = logCurlRequest('GET', '/_api/version?details=true');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestVersionHandler::execute () {
  TRI_json_t result;

  RequestStatisticsAgentSetIgnore(this);

  TRI_InitObject2Json(TRI_CORE_MEM_ZONE, &result, 3);

  TRI_json_t server;
  TRI_InitStringJson(&server, TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "arango"));
  TRI_Insert2ObjectJson(TRI_CORE_MEM_ZONE, &result, "server", &server);

  TRI_json_t version;
  TRI_InitStringJson(&version, TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, TRI_VERSION));
  TRI_Insert2ObjectJson(TRI_CORE_MEM_ZONE, &result, "version", &version);

  bool found;
  char const* detailsStr = _request->value("details", found);

  if (found && StringUtils::boolean(detailsStr)) {
    TRI_json_t details;

    TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &details);

    Version::getJson(TRI_CORE_MEM_ZONE, &details);
    TRI_Insert2ObjectJson(TRI_CORE_MEM_ZONE, &result, "details", &details);
  }

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
