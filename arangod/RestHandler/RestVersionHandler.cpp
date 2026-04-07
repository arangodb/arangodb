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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/ApiVersion.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "RestVersionHandler.h"

#include <velocypack/Builder.h>

#include <algorithm>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static void addApiVersions(VPackBuilder& result) {
  // Collect all supported versions and sort in descending order
  std::vector<uint32_t> versions;
  for (size_t i = 0; i < ApiVersion::numSupportedApiVersions(); ++i) {
    versions.push_back(ApiVersion::supportedApiVersions[i]);
  }
  std::sort(versions.begin(), versions.end(), std::greater<uint32_t>());

  // Add apiVersions array
  result.add("apiVersions", VPackValue(VPackValueType::Array));
  for (uint32_t version : versions) {
    result.add(VPackValue("v" + std::to_string(version)));
  }
  result.close();

  // Collect deprecated versions and sort in descending order
  std::vector<uint32_t> deprecatedVersions;
  constexpr size_t numDeprecated = sizeof(ApiVersion::deprecatedApiVersions) /
                                   sizeof(ApiVersion::deprecatedApiVersions[0]);
  for (size_t i = 0; i < numDeprecated; ++i) {
    deprecatedVersions.push_back(ApiVersion::deprecatedApiVersions[i]);
  }
  std::sort(deprecatedVersions.begin(), deprecatedVersions.end(),
            std::greater<uint32_t>());

  // Add deprecatedApiVersions array
  result.add("deprecatedApiVersions", VPackValue(VPackValueType::Array));
  for (uint32_t version : deprecatedVersions) {
    result.add(VPackValue("v" + std::to_string(version)));
  }
  result.close();
}

static void addVersionDetails(application_features::ApplicationServer& server,
                              VPackBuilder& result) {
  result.add("details", VPackValue(VPackValueType::Object));
  Version::getVPack(result);

  auto serverState = ServerState::instance();
  if (serverState != nullptr) {
    result.add("role",
               VPackValue(ServerState::roleToString(serverState->getRole())));
  }

  std::string host = ServerState::instance()->getHost();
  if (!host.empty()) {
    result.add("host", VPackValue(host));
  }
  result.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

void RestVersionHandler::getVersion(
    application_features::ApplicationServer& server, bool allowInfo,
    bool includeDetails, VPackBuilder& result, uint32_t requestedApiVersion) {
  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  if (allowInfo) {
    result.add("version", VPackValue(ARANGODB_VERSION));

    if (includeDetails) {
      addVersionDetails(server, result);
    }
  }

  // Add API version information (both in short and long form)
  addApiVersions(result);

  // Add the requested API version that was used to call this handler
  result.add("requestedApiVersion",
             VPackValue("v" + std::to_string(requestedApiVersion)));

  result.close();
}

RestStatus RestVersionHandler::execute() {
  if (!isAllowedHttpMethod({RequestType::GET})) {
    return RestStatus::DONE;
  }

  VPackBuilder result;

  ServerSecurityFeature& security =
      server().getFeature<ServerSecurityFeature>();

  bool const allowInfo = security.canAccessHardenedApi();
  bool const includeDetails = _request->parsedValue("details", false);
  getVersion(server(), allowInfo, includeDetails, result,
             _request->requestedApiVersion());

  response()->setAllowCompression(
      rest::ResponseCompressionType::kAllowCompression);

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
