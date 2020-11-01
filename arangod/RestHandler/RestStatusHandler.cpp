////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#if defined(TRI_HAVE_POSIX_THREADS)
#include <unistd.h>
#endif

#if defined(USE_MEMORY_PROFILE)
#include <jemalloc/jemalloc.h>
#include <velocypack/StringRef.h>
#endif

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestStatusHandler::RestStatusHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestStatusHandler::execute() {
  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->parsedValue("overview", false)) {
    return executeOverview();
  } else if (_request->parsedValue("memory", false)) {
    return executeMemoryProfile();
  } else {
    return executeStandard(security);
  }
}

RestStatus RestStatusHandler::executeStandard(ServerSecurityFeature& security) {
  VPackBuilder result;
  result.openObject();
  result.add("server", VPackValue("arango"));
  result.add("version", VPackValue(ARANGODB_VERSION));

  result.add("pid", VPackValue(static_cast<TRI_vpack_pid_t>(Thread::currentProcessId())));

#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  auto& serverFeature = server().getFeature<ServerFeature>();
  result.add("mode", VPackValue(serverFeature.operationModeString()));  // to be deprecated - 3.3 compat
  result.add("operationMode", VPackValue(serverFeature.operationModeString()));
  result.add("foxxApi", VPackValue(!security.isFoxxApiDisabled()));

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

    auto manager = AsyncAgencyCommManager::INSTANCE.get();

    if (manager != nullptr) {
      result.add("agency", VPackValue(VPackValueType::Object));

      {
        result.add("agencyComm", VPackValue(VPackValueType::Object));
        result.add("endpoints", VPackValue(VPackValueType::Array));

        for (auto const& ep : manager->endpoints()) {
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

RestStatus RestStatusHandler::executeOverview() {
  VPackBuilder result;

  result.openObject();
  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("platform", VPackValue(TRI_PLATFORM));

#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  result.add("engine", VPackValue(engine.typeName()));

  StringBuffer buffer;

  buffer.appendText("1-");

  auto serverState = ServerState::instance();

  if (serverState != nullptr) {
    auto role = serverState->getRole();
    result.add("role", VPackValue(ServerState::roleToString(role)));

    if (role == ServerState::ROLE_COORDINATOR) {
      AgencyCache& agencyCache = server().getFeature<ClusterFeature>().agencyCache();
      auto [b, i] = agencyCache.get("arango/Plan");
    
      VPackSlice planSlice = b->slice().get(std::vector<std::string>{AgencyCommHelper::path(), "Plan"});

      if (planSlice.isObject()) {
        if (planSlice.hasKey("Coordinators")) {
          auto coordinators =  planSlice.get("Coordinators");
          buffer.appendHex(static_cast<uint32_t>(VPackObjectIterator(coordinators).size()));
          buffer.appendText("-");
        }
        if (planSlice.hasKey("DBServers")) {
          auto dbservers = planSlice.get("DBServers");
          buffer.appendHex(static_cast<uint32_t>(VPackObjectIterator(dbservers).size()));
        }
      } else {
        buffer.appendHex(static_cast<uint32_t>(0xFFFF));
        buffer.appendText("-");
        buffer.appendHex(static_cast<uint32_t>(1));
      }
    } else if (role == ServerState::ROLE_DBSERVER) {
      buffer.appendHex(static_cast<uint32_t>(0xFFFF));
      buffer.appendText("-");
      buffer.appendHex(static_cast<uint32_t>(2));
    } else if (role == ServerState::ROLE_AGENT) {
      buffer.appendHex(static_cast<uint32_t>(0xFFFF));
      buffer.appendText("-");
      buffer.appendHex(static_cast<uint32_t>(3));
    } else if (role == ServerState::ROLE_SINGLE) {
      buffer.appendHex(static_cast<uint32_t>(0xFFFF));
      buffer.appendText("-");
      buffer.appendHex(static_cast<uint32_t>(4));
    } else {
      buffer.appendHex(static_cast<uint32_t>(0xFFFF));
      buffer.appendText("-");
      buffer.appendHex(static_cast<uint32_t>(5));
    }
  } else {
    buffer.appendHex(static_cast<uint32_t>(0xFFFF));
    buffer.appendText("-");
    buffer.appendHex(static_cast<uint32_t>(6));
  }

  buffer.appendText("-");

  {
    std::string const& browserStr = _request->header("user-agent");

    if (!browserStr.empty()) {
      buffer.appendText(browserStr);
    } else {
      buffer.appendText("unknown browser");
    }
  }

  int res = TRI_DeflateStringBuffer(buffer.stringBuffer(), buffer.size());

  if (res != TRI_ERROR_NO_ERROR) {
    result.add("hash", VPackValue(buffer.c_str()));
  } else {
    std::string deflated(buffer.c_str(), buffer.size());
    auto encoded = StringUtils::encodeBase64(deflated);
    result.add("hash", VPackValue(encoded));
  }

  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}

RestStatus RestStatusHandler::executeMemoryProfile() {
#if defined(USE_MEMORY_PROFILE)
  long err;
  std::string filename;
  std::string msg;
  int res = TRI_GetTempName(nullptr, filename, true, err, msg);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(rest::ResponseCode::INTERNAL_ERROR, res, msg);
  } else {
    char const* f = fileName.c_str();
    try {
      mallctl("prof.dump", NULL, NULL, &f, sizeof(const char *));
      std::string const content = FileUtils::slurp(fileName);
      TRI_UnlinkFile(f);

      resetResponse(rest::ResponseCode::OK);

      _response->setContentType(rest::ContentType::TEXT);
      _response->addRawPayload(velocypack::StringRef(content));
    } catch (...) {
      TRI_UnlinkFile(f);
      throw;
    }
  }
#else
  generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
		"memory profiles not enabled at compile time");
#endif

  return RestStatus::DONE;
}
