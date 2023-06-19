////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestDumpHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/RequestLane.h"
#include "Inspection/VPack.h"
#include "RocksDBEngine/RocksDBDumpManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <cstdint>
#include <vector>

using namespace arangodb;
using namespace arangodb::rest;

RestDumpHandler::RestDumpHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

// main function that dispatches the different routes and commands
RestStatus RestDumpHandler::execute() {
  if (!ServerState::instance()->isDBServer()) {
    generateError(Result(TRI_ERROR_HTTP_NOT_IMPLEMENTED,
                         "api only expected to be called on dbservers"));
    return RestStatus::DONE;
  }

  Result res = validateRequest();
  if (res.fail()) {
    generateError(std::move(res));
    return RestStatus::DONE;
  }

  auto type = _request->requestType();
  // already validated by validateRequest()
  TRI_ASSERT(type == rest::RequestType::DELETE_REQ ||
             type == rest::RequestType::POST);

  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  if (type == rest::RequestType::DELETE_REQ) {
    // already validated by validateRequest()
    TRI_ASSERT(len == 1);
    // end a dump
    handleCommandDumpFinished();
  } else if (type == rest::RequestType::POST) {
    if (len == 1) {
      TRI_ASSERT(suffixes[0] == "start");
      // start a dump
      handleCommandDumpStart();
    } else if (len == 2) {
      TRI_ASSERT(suffixes[0] == "next");
      // fetch next data from a dump
      handleCommandDumpNext();
    } else {
      // unreachable. already validated by validateRequest()
      ADB_UNREACHABLE;
    }
  } else {
    // unreachable. already validated by validateRequest()
    ADB_UNREACHABLE;
  }

  return RestStatus::DONE;
}

RequestLane RestDumpHandler::lane() const {
  auto type = _request->requestType();

  if (type == rest::RequestType::DELETE_REQ) {
    // delete should be prioritized, because it frees
    // up resources
    return RequestLane::CLUSTER_INTERNAL;
  }
  return RequestLane::SERVER_REPLICATION;
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestDumpHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  Result res = validateRequest();
  if (res.fail()) {
    return ResultT<std::pair<std::string, bool>>::error(std::move(res));
  }

  if (ServerState::instance()->isCoordinator()) {
    ServerID const& DBserver = _request->value("dbserver");
    if (!DBserver.empty()) {
      // if DBserver property present, remove auth header
      _request->addHeader(StaticStrings::DumpAuthUser, _request->user());
      return std::make_pair(DBserver, true);
    }

    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_BAD_PARAMETER, "need a 'dbserver' parameter");
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

void RestDumpHandler::handleCommandDumpStart() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return;
  }

  auto database = _request->databaseName();
  auto user = getAuthorizedUser();

  RocksDBDumpContextOptions opts;
  velocypack::deserializeUnsafe(body, opts);

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  auto guard = manager->createContext(std::move(opts), user, database);

  resetResponse(rest::ResponseCode::CREATED);
  _response->setHeaderNC(StaticStrings::DumpId, guard->id());
}

void RestDumpHandler::handleCommandDumpNext() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto const& suffixes = _request->suffixes();
  // checked before
  TRI_ASSERT(suffixes.size() == 2);
  auto const& id = _request->suffixes()[1];

  auto database = _request->databaseName();
  auto user = getAuthorizedUser();

  auto batchId = _request->parsedValue<uint64_t>("batchId");
  if (!batchId.has_value()) {
    generateError(Result(TRI_ERROR_BAD_PARAMETER, "expecting 'batchId'"));
    return;
  }

  auto lastBatch = _request->parsedValue<uint64_t>("lastBatch");

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  // find() will throw in case the context cannot be found or the user does not
  // match.
  auto context = manager->find(id, database, user);
  context->extendLifetime();

  auto batch = context->next(*batchId, lastBatch);
  auto counts = context->getBlockCounts();

  if (batch == nullptr) {
    // all batches have been received
    return generateOk(rest::ResponseCode::NO_CONTENT, VPackSlice::noneSlice());
  }

  // output the batch value
  _response->setHeaderNC(StaticStrings::DumpShardId, std::string{batch->shard});
  _response->setHeaderNC(StaticStrings::DumpBlockCounts,
                         std::to_string(counts));
  _response->setContentType(rest::ContentType::DUMP);
  _response->addRawPayload(batch->content);
  _response->setGenerateBody(true);
  _response->setResponseCode(rest::ResponseCode::OK);

  // prolong lifetime of context, so that it is still there for follow-up
  // requests.
  context->extendLifetime();
}

void RestDumpHandler::handleCommandDumpFinished() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto const& suffixes = _request->suffixes();
  // checked before
  TRI_ASSERT(suffixes.size() == 1);
  auto const& id = _request->suffixes()[0];

  auto database = _request->databaseName();
  auto user = getAuthorizedUser();

  auto& engine =
      server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* manager = engine.dumpManager();
  // will throw if dump context is not found or cannot be accessed
  manager->remove(id, database, user);

  generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
}

std::string RestDumpHandler::getAuthorizedUser() const {
  bool headerExtracted;
  auto user = _request->header(StaticStrings::DumpAuthUser, headerExtracted);
  if (!headerExtracted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "missing authorization header");
  }
  return user;
}

Result RestDumpHandler::validateRequest() {
  auto type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  if (type == rest::RequestType::DELETE_REQ) {
    if (len != 1) {
      return {TRI_ERROR_BAD_PARAMETER, "expecting DELETE /_api/dump/<id>"};
    }
    return {};
  }

  if (type == rest::RequestType::POST) {
    if ((len == 1 && suffixes[0] != "start") ||
        (len == 2 && suffixes[0] != "next") || len < 1 || len > 2) {
      return {TRI_ERROR_BAD_PARAMETER,
              "expecting POST /_api/dump/start or /_api/dump/next/<id>"};
    }

    if (suffixes[0] == "start") {
      bool parseSuccess = false;
      VPackSlice body = this->parseVPackBody(parseSuccess);
      if (!parseSuccess) {  // error message generated in parseVPackBody
        return {TRI_ERROR_BAD_PARAMETER};
      }

      // validate permissions for all participating shards
      RocksDBDumpContextOptions opts;
      velocypack::deserializeUnsafe(body, opts);

      auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
      ExecContext& exec =
          *static_cast<ExecContext*>(_request->requestContext());

      for (auto const& it : opts.shards) {
        // get collection name
        auto collectionName = ci.getCollectionNameForShard(it);
        if (!exec.canUseCollection(_request->databaseName(), collectionName,
                                   auth::Level::RO)) {
          return {TRI_ERROR_FORBIDDEN};
        }
      }
    }

    return {};
  }

  // invalid HTTP method
  return {TRI_ERROR_HTTP_METHOD_NOT_ALLOWED};
}
