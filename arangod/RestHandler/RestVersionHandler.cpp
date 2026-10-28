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
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ApiVersion.h"
#include "Cluster/ServerState.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "RestVersionHandler.h"

#include <velocypack/Builder.h>

#include <span>
#include <string_view>

namespace arangodb {

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {

// Emits the version names newest first. The tables are compile-time constants
// kept in ascending order (see the static_asserts in ApiVersion.h), hence the
// reverse iteration
void addVersionArray(VPackBuilder& result, std::string_view name,
                     std::span<ApiVersion const> versions) {
  result.add(name, VPackValue(VPackValueType::Array));
  for (auto it = versions.rbegin(); it != versions.rend(); ++it) {
    result.add(VPackValue(to_string(*it)));
  }
  result.close();
}

void addApiVersions(VPackBuilder& result) {
  addVersionArray(result, "apiVersions", api_version::supportedApiVersions);
  addVersionArray(result, "deprecatedApiVersions",
                  api_version::deprecatedApiVersions);
}

}  // namespace

static void addVersionDetails(application_features::ApplicationServer& server,
                              VPackBuilder& result,
                              ApiVersion requestedApiVersion) {
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

async<RestHandler::AuthenticationGrant>
RestVersionHandler::checkUserAuthentication() const {
  // Note that this particular RestHandler might be called during startup (or
  // in maintenance mode). The AuthenticationFeature might not yet be available
  // for authorization, and must not be consulted.
  if (auto const mode = ServerState::mode();
      mode == ServerState::Mode::STARTUP ||
      mode == ServerState::Mode::MAINTENANCE) {
    co_return request()->authenticated()  // with JWT can also be authenticated
        ? AuthenticationGrant::GRANTED_EARLY
        : AuthenticationGrant::DENIED;
  }

  co_return co_await RestBaseHandler::checkUserAuthentication();
}

async<Result> RestVersionHandler::checkApiVersionAccess() const {
  auto version = request()->requestedApiVersion();
  if (version == api_version::defaultApiVersion) {
    // default api-version is always allowed
    co_return Result{};
  }

  co_return co_await RestBaseHandler::checkApiVersionAccess();
}

void RestVersionHandler::getVersion(
    application_features::ApplicationServer& server, bool allowInfo,
    bool includeDetails, VPackBuilder& result, ApiVersion requestedApiVersion) {
  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  // "version" can be added unconditionally in 4.0
  if (allowInfo || !ExecContext::current().isClassic() ||
      requestedApiVersion > ApiVersion::V0) {
    result.add("version", VPackValue(ARANGODB_VERSION));
  }

  if (allowInfo && includeDetails) {
    addVersionDetails(server, result, requestedApiVersion);
  }

  // Add API version information (both in short and long form)
  addApiVersions(result);

  // Add the requested API version that was used to call this handler
  result.add("requestedApiVersion", VPackValue(to_string(requestedApiVersion)));

  result.close();
}

// Mounted at /_api/version (exact) and /_admin/version (exact)
RestStatus RestVersionHandler::execute() {
  if (!isAllowedHttpMethod({RequestType::GET})) {
    return RestStatus::DONE;
  }

  VPackBuilder result;

  bool const allowInfo =
      ExecContext::current()
          .canUseHardenedAction(auth::perms::AdminMonitoringInternal{})
          .ok();
  bool const includeDetails = _request->parsedValue("details", false);
  getVersion(server(), allowInfo, includeDetails, result,
             _request->requestedApiVersion());

  response()->setAllowCompression(
      rest::ResponseCompressionType::kAllowCompression);

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}

}  // namespace arangodb