////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestLicenseHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/License/LicenseFeature.h"
#endif
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestLicenseHandler::RestLicenseHandler(application_features::ApplicationServer& server,
                                       GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(server, request, response) {}

#ifndef USE_ENTERPRISE
RestStatus RestLicenseHandler::execute() {

  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }


  VPackBuilder builder;
  switch(_request->requestType()) {
  case RequestType::GET:
    {
      VPackObjectBuilder b(&builder);
      builder.add("license", VPackValue("none"));
    }
    generateResult(rest::ResponseCode::OK, builder.slice());
    break;
  case RequestType::PUT:
    generateError(
      rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_ONLY_ENTERPRISE,
      "The community edition cannot be licensed.");
    break;
  default:
    generateError(
      rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
      "Method not allowed. Only GET and PUT requests are handled.");
  }

  return RestStatus::DONE;

}
#endif

/// @brief check for administrator rights
arangodb::Result RestLicenseHandler::verifyPermitted() {
  #ifdef USE_ENTERPRISE
  auto& feature = server().getFeature<arangodb::LicenseFeature>();

  // do we have admin rights (if rights are active)
  if (feature.onlySuperUser()) {
    if (!ExecContext::current().isSuperuser()) {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN, "you need super user rights for license operations");
    }
  } else {
    if (!ExecContext::current().isAdminUser()) {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN, "you need admin rights for license operations");
    }
  }
  #endif
  return arangodb::Result();

}
