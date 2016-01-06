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
#include "Rest/AnyServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern AnyServer* ArangoInstance;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler(HttpRequest* request)
    : RestBaseHandler(request) {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestVersionHandler::isDirect() const { return true; }

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
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned in all cases.
///
/// @RESTREPLYBODY{server,string,required,string}
/// will always contain *arango*
///
/// @RESTREPLYBODY{version,string,required,string}
/// the server version string. The string has the format
/// "*major*.*minor*.*sub*". *major* and *minor* will be numeric, and *sub*
/// may contain a number or a textual version.
///
/// @RESTREPLYBODY{details,object,optional,}
/// an optional JSON object with additional details. This is
/// returned only if the *details* query parameter is set to *true* in the
/// request.
///
/// @EXAMPLES
///
/// Return the version information
///
/// @EXAMPLE_ARANGOSH_RUN{RestVersion}
///     var response = logCurlRequest('GET', '/_api/version');
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Return the version information with details
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

HttpHandler::status_t RestVersionHandler::execute() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("server", VPackValue("arango"));
    result.add("version", VPackValue(TRI_VERSION));

    bool found;
    char const* detailsStr = _request->value("details", found);

    if (found && StringUtils::boolean(detailsStr)) {
      result.add("details", VPackValue(VPackValueType::Object));

      Version::getVPack(result);

      if (ArangoInstance != nullptr) {
        result.add("mode", VPackValue(ArangoInstance->modeString()));
      }
      result.close();
    }
    result.close();
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (...) {
    // Ignore this error
  }
  return status_t(HANDLER_DONE);
}


