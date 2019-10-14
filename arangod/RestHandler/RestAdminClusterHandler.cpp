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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminClusterHandler.h"

#include <chrono>

#include <velocypack/Iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include "Cluster/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Cluster/ResultT.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using namespace std::chrono_literals;

namespace {

struct agentConfigHealthResult {
  std::string endpoint, name;
  futures::Try<network::Response> response;
};

void buildHealthResult(VPackBuffer<uint8_t> &out, std::vector<futures::Try<agentConfigHealthResult>> const& config, VPackSlice store) {
  auto rootPath = arangodb::cluster::paths::root()->arango();

  VPackBuilder builder(out);
  VPackObjectBuilder ob(&builder);

  VPackObjectIterator memberIter(store.get(rootPath->supervision()->health()->vec()));
  for (auto member : memberIter) {
    std::string serverId = member.key.copyString();

    {
      VPackObjectBuilder ob(&builder, serverId);

      // add all fields to the object
      for (auto entry : VPackObjectIterator(member.value)) {
        builder.add(entry.key.stringRef(), entry.value);
      }

      if (serverId.substr(0, 4) == "PRMR") {
        builder.add("Role", VPackValue("DBServer"));
      } else if (serverId.substr(0, 4) == "CRDN") {
        builder.add("Role", VPackValue("Coodrinator"));
        builder.add("CanBeDeleted", VPackValue(member.value.get("Status").isEqualString("FAILED")));
      }
    }
  }

  for (auto& memberTry : config) {
    if (!memberTry.hasValue()) {
      continue; // should never happen
    }

    auto& member = memberTry.get();

    {
      VPackObjectBuilder ob(&builder, member.name);

      builder.add("Role", VPackValue("Agent"));
      builder.add("Endpoint", VPackValue(member.endpoint));
      builder.add("CanBeDeleted", VPackValue(false));

      // TODO missing LastAckedTime
      if (member.response.hasValue()) {
        auto& response = member.response.get();
        if (response.ok() && response.response->statusCode() == fuerte::StatusOK) {
          VPackSlice config = response.slice();
          builder.add("Engine", config.get("engine"));
          builder.add("Version", config.get("version"));
          builder.add("Leader", config.get("leaderId"));
        }
      } else {
        builder.add("Status", VPackValue("UNKNOWN"));
      }
    }
  }
}

}

RestAdminClusterHandler::RestAdminClusterHandler(application_features::ApplicationServer& server,
                                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestAdminClusterHandler::~RestAdminClusterHandler() {}

std::string const RestAdminClusterHandler::Health = "health";
std::string const RestAdminClusterHandler::NumberOfServers = "numberOfServers";

RestStatus RestAdminClusterHandler::execute() {

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  if (len == 1) {
    std::string const& command = suffixes.at(0);

    if (command == Health) {
      return handleHealth();
    } else if (command == NumberOfServers) {
      return handleNumberOfServers();
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("invalid command '") + command + "'");
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                "expecting URL /_api/replication/<command>");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleGetNumberOfServers() {

  auto targetPath = arangodb::cluster::paths::root()->arango()->target();
  AgencyReadTransaction trx(std::move(std::vector<std::string>{
    targetPath->numberOfDBServers()->str(),
    targetPath->numberOfCoordinators()->str(),
    targetPath->cleanedServers()->str()
  }));

  auto self(shared_from_this());

  return waitForFuture(AsyncAgencyComm().sendTransaction(10.0s, trx)
    .thenValue([this, self, targetPath] (AsyncAgencyCommResult &&result) {
      if (result.ok() && result.statusCode() == fuerte::StatusOK) {

        VPackBuffer<uint8_t> body;
        {
          VPackBuilder builder(body);
          VPackObjectBuilder ob(&builder);
          builder.add("numberOfDBServers", result.slice().at(0).get(targetPath->numberOfDBServers()->vec()));
          builder.add("numberOfCoordinators", result.slice().at(0).get(targetPath->numberOfCoordinators()->vec()));
          builder.add("cleanedServers", result.slice().at(0).get(targetPath->cleanedServers()->vec()));
        }

        resetResponse(rest::ResponseCode::OK);
        response()->setPayload(std::move(body), true);
      } else {
        generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, "agency communication failed");
      }
    }).thenError<VPackException>([this, self](VPackException const& e) {
      generateError(Result{e.errorCode(), e.what()});
    }).thenError<std::exception>([this, self](std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, e.what());
    }));
}

RestStatus RestAdminClusterHandler::handlePutNumberOfServers() {

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  bool parseSuccess;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return RestStatus::DONE;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, "object expected");
    return RestStatus::DONE;
  }

  std::vector<AgencyOperation> ops;
  auto targetPath = arangodb::cluster::paths::root()->arango()->target();

  VPackSlice numberOfCoordinators = body.get("numberOfCoordinators");
  if (numberOfCoordinators.isNumber()) {
    ops.emplace_back(targetPath->numberOfCoordinators()->str(), AgencyValueOperationType::SET, numberOfCoordinators);
  } else if (!numberOfCoordinators.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, "numberOfCoordinators: number expected");
    return RestStatus::DONE;
  }

  VPackSlice numberOfDBServers = body.get("numberOfDBServers");
  if (numberOfDBServers.isNumber()) {
    ops.emplace_back(targetPath->numberOfDBServers()->str(), AgencyValueOperationType::SET, numberOfDBServers);
  } else if (!numberOfDBServers.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, "numberOfDBServers: number expected");
    return RestStatus::DONE;
  }

  VPackSlice cleanedServers = body.get("cleanedServers");
  if (cleanedServers.isArray()) {

    bool allStrings = true;
    for (auto server : VPackArrayIterator(cleanedServers)) {
      if (false == server.isString()) {
        allStrings = false;
      }
    }

    if (allStrings) {
      ops.emplace_back(targetPath->cleanedServers()->str(), AgencyValueOperationType::SET, cleanedServers);
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, "cleanedServers: array of strings expected");
      return RestStatus::DONE;
    }
  } else if (!cleanedServers.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, "cleanedServers: array expected");
    return RestStatus::DONE;
  }

  auto self(shared_from_this());
  AgencyWriteTransaction trx(std::move(ops));
  return waitForFuture(AsyncAgencyComm().sendTransaction(20s, std::move(trx))
    .thenValue([this, self](AsyncAgencyCommResult &&result) {
      if (result.ok() && result.statusCode() == fuerte::StatusOK) {
        resetResponse(rest::ResponseCode::OK);
      } else {
        generateError(result.asResult());
      }
    }).thenError<VPackException>([this, self](VPackException const& e) {
      generateError(Result{e.errorCode(), e.what()});
    }).thenError<std::exception>([this, self](std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, e.what());
    }));
}

RestStatus RestAdminClusterHandler::handleNumberOfServers() {

  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN, "only allowed on coordinators");
    return RestStatus::DONE;
  }

  switch (request()->requestType()) {
    case rest::RequestType::GET:
      return handleGetNumberOfServers();
    case rest::RequestType::PUT:
      return handlePutNumberOfServers();
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

RestStatus RestAdminClusterHandler::handleHealth() {

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != rest::RequestType::GET) {

  }

  if (!ServerState::instance()->isCoordinator() && !ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN, "only allowed on single server and coordinators");
    return RestStatus::DONE;
  }

  // TODO handle timeout parameter

  auto self(shared_from_this());

  // query the agency config
  auto fConfig = AsyncAgencyComm().sendWithFailover(fuerte::RestVerb::Get, "/_api/agency/config", 60.0s, VPackBuffer<uint8_t>())
    .thenValue([this, self] (AsyncAgencyCommResult &&result) {

      if (result.fail() || result.statusCode() != fuerte::StatusOK) {
        THROW_ARANGO_EXCEPTION(result.asResult());
      }

      // now connect to all the members and ask for their engine and version
      std::vector<futures::Future<::agentConfigHealthResult>> fs;

      auto* pool = server().getFeature<NetworkFeature>().pool();
      for (auto member : VPackObjectIterator(result.slice().get(std::vector<std::string>{"configuration", "pool"}))) {

        std::string endpoint = member.value.copyString();
        std::string memberName = member.key.copyString();

        auto future = network::sendRequest(pool, endpoint,
          fuerte::RestVerb::Get, "/_api/agency/config", VPackBuffer<uint8_t>(), 5s)
        .then([endpoint = std::move(endpoint), memberName = std::move(memberName)](futures::Try<network::Response> &&resp) {
          return futures::makeFuture(::agentConfigHealthResult{
            std::move(endpoint), std::move(memberName), std::move(resp)});
        });

        fs.emplace_back(std::move(future));
      }

      return futures::collectAll(fs);
    });

  // query information from the store
  auto rootPath = arangodb::cluster::paths::root()->arango();
  AgencyReadTransaction trx(std::move(std::vector<std::string>{rootPath->cluster()->str(), rootPath->supervision()->health()->str()}));
  auto fStore = AsyncAgencyComm().sendTransaction(60.0s, trx);

  return waitForFuture(futures::collect(std::move(fConfig), std::move(fStore)).thenValue(
    [this, self, rootPath] (auto&& result) {
      auto &configResult = std::get<0>(result);
      auto &storeResult = std::get<1>(result);
      if (storeResult.ok() && storeResult.statusCode() == fuerte::StatusOK) {

        VPackBuffer<uint8_t> responseBody;
        ::buildHealthResult (responseBody, configResult, storeResult.slice().at(0));
        resetResponse(rest::ResponseCode::OK);
        response()->setPayload(std::move(responseBody), true);
      } else {
        generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, "agency communication failed");
      }
    }).thenError<VPackException>([this, self](VPackException const& e) {
      generateError(Result{e.errorCode(), e.what()});
    }).thenError<std::exception>([this, self](std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, e.what());
    }));
}
