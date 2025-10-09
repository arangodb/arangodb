////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminDeploymentHandler.h"

#include <velocypack/Builder.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminDeploymentHandler::RestAdminDeploymentHandler(
    ArangodServer& server, GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

std::string const RestAdminDeploymentHandler::Id = "id";

auto RestAdminDeploymentHandler::executeAsync()
    -> futures::Future<futures::Unit> {
  if (!ServerState::instance()->isCoordinator() &&
      !ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "only allowed on single server and coordinators");
    co_return;
  }

  auto const& suffixes = request()->suffixes();
  size_t const len = suffixes.size();

  if (len == 1) {
    std::string const& command = suffixes.at(0);

    if (command == Id) {
      handleId();
      co_return;
    }
  }

  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                "unknown path for /_admin/deployment");
  co_return;
}

void RestAdminDeploymentHandler::handleId() {
  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return;
  }

  // Get the deployment ID from ClusterFeature
  auto& clusterFeature = server().getFeature<ClusterFeature>();
  auto result = clusterFeature.getDeploymentId();

  if (result.fail()) {
    ErrorCode errorCode = result.errorNumber();
    rest::ResponseCode responseCode = rest::ResponseCode::SERVER_ERROR;

    if (errorCode == TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE ||
        errorCode == TRI_ERROR_HTTP_SERVICE_UNAVAILABLE) {
      responseCode = rest::ResponseCode::SERVICE_UNAVAILABLE;
    }

    generateError(responseCode, errorCode, result.errorMessage());
    return;
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("id", VPackValue(result.get()));
  }

  generateResult(rest::ResponseCode::OK, builder.slice());
}
