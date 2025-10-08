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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminDeploymentHandler.h"

#include <velocypack/Builder.h>

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TransactionBuilder.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utils/ExecContext.h"

using namespace std::chrono_literals;

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
      co_await handleId();
      co_return;
    }
  }

  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                "unknown path for /_admin/deployment");
  co_return;
}

async<void> RestAdminDeploymentHandler::handleId() {
  if (request()->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    co_return;
  }

  std::string deploymentId = "SOME_ID";

  // For coordinators, query the cluster ID from the agency
  if (ServerState::instance()->isCoordinator()) {
    if (AsyncAgencyCommManager::INSTANCE == nullptr) {
      generateError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                    TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
                    "agency communication not available");
      co_return;
    }

    try {
      // Build a read transaction to query /arango/cluster
      auto rootPath = arangodb::cluster::paths::root()->arango();
      VPackBuffer<uint8_t> trx;
      {
        VPackBuilder builder(trx);
        arangodb::agency::envelope::into_builder(builder)
            .read()
            .key(rootPath->cluster()->str())
            .end()
            .done();
      }

      // Send the read transaction to the agency
      auto result =
          co_await AsyncAgencyComm().sendReadTransaction(60.0s, std::move(trx));

      if (result.ok() && result.statusCode() == fuerte::StatusOK) {
        // Extract the cluster ID from the response
        // The result is an array with one element containing the value
        VPackSlice slice = result.slice();
        if (slice.isArray() && slice.length() > 0) {
          VPackSlice clusterSlice = slice.at(0);
          if (clusterSlice.isObject()) {
            VPackSlice arangoSlice =
                clusterSlice.get(std::vector<std::string>{"arango", "Cluster"});
            if (arangoSlice.isString()) {
              deploymentId = arangoSlice.copyString();
            }
          }
        }
      } else {
        generateError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                      TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
                      "failed to query cluster ID from agency");
        co_return;
      }
    } catch (std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_HTTP_SERVER_ERROR, e.what());
      co_return;
    }
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("id", VPackValue(deploymentId));
  }

  generateResult(rest::ResponseCode::OK, builder.slice());
  co_return;
}
