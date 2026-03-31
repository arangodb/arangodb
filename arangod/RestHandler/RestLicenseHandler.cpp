////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestLicenseHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/Rbac/Actions.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/License/LicenseFeature.h"
#endif
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestLicenseHandler::RestLicenseHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

#ifndef USE_ENTERPRISE
// Mounted at /_admin/license (prefix)
RestStatus RestLicenseHandler::execute() {
  ServerSecurityFeature& security =
      server().getFeature<ServerSecurityFeature>();

  if (auto r = security.canAccessHardenedApi(
          arangodb::rbac::Category::AdminLicense{});
      r.fail()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  r.errorMessage());
    return RestStatus::DONE;
  }

  VPackBuilder builder;
  switch (_request->requestType()) {
    case RequestType::GET: {
      VPackObjectBuilder b(&builder);
      builder.add("license", VPackValue("none"));
    }
      generateResult(rest::ResponseCode::OK, builder.slice());
      break;
    case RequestType::PUT:
      generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                    TRI_ERROR_ONLY_ENTERPRISE,
                    "The community edition cannot be licensed.");
      break;
    default:
      generateError(
          rest::ResponseCode::METHOD_NOT_ALLOWED,
          TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
          "Method not allowed. Only GET and PUT requests are handled.");
  }

  return RestStatus::DONE;
}
#endif
