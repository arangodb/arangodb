////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestStatusHandler.h"

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"

#if defined(TRI_HAVE_POSIX_THREADS)
#include <unistd.h>
#endif

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestStatusHandler::RestStatusHandler(GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestStatusHandler::execute() {
  ServerSecurityFeature* security =
      application_features::ApplicationServer::getFeature<ServerSecurityFeature>(
          "ServerSecurity");
  TRI_ASSERT(security != nullptr);
  
  if (!security->canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN); 
    return RestStatus::DONE;
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
  result.add("version", VPackValue(ARANGODB_VERSION));

  result.add("pid", VPackValue(Thread::currentProcessId()));

#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  if (application_features::ApplicationServer::server != nullptr) {
    auto server = application_features::ApplicationServer::server->getFeature<ServerFeature>(
        "Server");
    result.add("mode",
               VPackValue(server->operationModeString()));  // to be deprecated - 3.3 compat
    result.add("operationMode", VPackValue(server->operationModeString()));
  }

  std::string host = ServerState::instance()->getHost();

  if (!host.empty()) {
    result.add("host", VPackValue(host));
  }

  char const* hostname = getenv("HOSTNAME");

  if (hostname != nullptr) {
    result.add("hostname", VPackValue(hostname));
  }

  auto serverState = ServerState::instance();

  if (serverState != nullptr) {
    result.add("serverInfo", VPackValue(VPackValueType::Object));

    result.add("maintenance", VPackValue(serverState->isMaintenance()));
    result.add("role", VPackValue(ServerState::roleToString(serverState->getRole())));
    result.add("writeOpsEnabled",
               VPackValue(!serverState->readOnly()));  // to be deprecated - 3.3 compat
    result.add("readOnly", VPackValue(serverState->readOnly()));

    if (!serverState->isSingleServer()) {
      result.add("persistedId", VPackValue(serverState->getPersistedId()));

      if (!serverState->isAgent()) {
        result.add("address", VPackValue(serverState->getEndpoint()));
        result.add("serverId", VPackValue(serverState->getId()));

        result.add("state",
                   VPackValue(ServerState::stateToString(serverState->getState())));
      }
    }

    result.close();

    auto* agent = AgencyFeature::AGENT;

    if (agent != nullptr) {
      result.add("agent", VPackValue(VPackValueType::Object));

      result.add("term", VPackValue(agent->term()));
      result.add("id", VPackValue(agent->id()));
      result.add("endpoint", VPackValue(agent->endpoint()));
      result.add("leaderId", VPackValue(agent->leaderID()));
      result.add("leading", VPackValue(agent->leading()));

      result.close();
    }

    if (serverState->isCoordinator()) {
      result.add("coordinator", VPackValue(VPackValueType::Object));

      result.add("foxxmaster", VPackValue(serverState->getFoxxmaster()));
      result.add("isFoxxmaster", VPackValue(serverState->isFoxxmaster()));

      result.close();
    }

    auto manager = AgencyCommManager::MANAGER.get();

    if (manager != nullptr) {
      result.add("agency", VPackValue(VPackValueType::Object));

      {
        result.add("agencyComm", VPackValue(VPackValueType::Object));
        result.add("endpoints", VPackValue(VPackValueType::Array));

        for (auto ep : manager->endpoints()) {
          result.add(VPackValue(ep));
        }

        result.close();
        result.close();
      }

      result.close();
    }
  }

  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
