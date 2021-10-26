////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
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
