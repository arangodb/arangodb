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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "RestVersionHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler(application_features::ApplicationServer& server,
                                       GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestVersionHandler::execute() {
  VPackBuilder result;

  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  bool const allowInfo = security.canAccessHardenedApi();

  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
#ifdef USE_ENTERPRISE
    result.add("license", VPackValue("enterprise"));
#else
    result.add("license", VPackValue("community"));
#endif

  if (allowInfo) {
    result.add("version", VPackValue(ARANGODB_VERSION));

    bool found;
    std::string const& detailsStr = _request->value("details", found);
    if (found && StringUtils::boolean(detailsStr)) {
      result.add("details", VPackValue(VPackValueType::Object));
      Version::getVPack(result);

      auto& serverFeature = server().getFeature<ServerFeature>();
      result.add("mode", VPackValue(serverFeature.operationModeString()));
      auto serverState = ServerState::instance();
      if (serverState != nullptr) {
        result.add("role", VPackValue(ServerState::roleToString(serverState->getRole())));
      }

      std::string host = ServerState::instance()->getHost();
      if (!host.empty()) {
        result.add("host", VPackValue(host));
      }
      result.close();
    }  // found
  }    // allowInfo
  result.close();
  response()->setAllowCompression(true);

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
