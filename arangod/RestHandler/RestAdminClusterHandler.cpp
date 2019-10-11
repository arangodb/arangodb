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
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using namespace std::chrono_literals;

namespace {

void buildHealthResult(VPackBuffer<uint8_t> &out, VPackSlice config, VPackSlice store) {

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

  // TODO: add agents
}


}

RestAdminClusterHandler::RestAdminClusterHandler(application_features::ApplicationServer& server,
                                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestAdminClusterHandler::~RestAdminClusterHandler() {}


std::string const RestAdminClusterHandler::Health = "health";

RestStatus RestAdminClusterHandler::execute() {

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  if (len == 1) {
    std::string const& command = suffixes.at(0);

    if (command == Health) {
      return handleHealth();
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("invalid command '") + command + "'");
    }
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                "expecting URL /_api/replication/<command>");
  return RestStatus::DONE;
}

RestStatus RestAdminClusterHandler::handleHealth() {

  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isCoordinator() && !ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN, "only allowed on single server and coordinators");
    return RestStatus::DONE;
  }


  // query the agency config
  auto fConfig = AsyncAgencyComm().sendWithFailover(fuerte::RestVerb::Get, "/_api/agency/config", 60.0s, VPackBuffer<uint8_t>());

  // query information from the store
  auto rootPath = arangodb::cluster::paths::root()->arango();
  AgencyReadTransaction trx(std::move(std::vector<std::string>{rootPath->cluster()->str(), rootPath->supervision()->health()->str()}));
  auto fStore = AsyncAgencyComm().sendTransaction(60.0s, trx);


  auto self(shared_from_this());
  return waitForFuture(futures::collect(std::move(fConfig), std::move(fStore)).thenValue(
    [this, self, rootPath] (auto&& result) {
      auto &configResult = std::get<0>(result);
      auto &storeResult = std::get<1>(result);
      if (configResult.ok() && configResult.statusCode() == fuerte::StatusOK
        && storeResult.ok() && storeResult.statusCode() == fuerte::StatusOK) {

        VPackBuffer<uint8_t> responseBody;
        ::buildHealthResult (responseBody, configResult.slice(), storeResult.slice().at(0));
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
