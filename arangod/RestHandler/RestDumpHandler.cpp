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

#include <absl/strings/str_cat.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <cstdint>
#include <vector>

using namespace arangodb;
using namespace arangodb::rest;

RestDumpHandler::RestDumpHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _clusterInfo(server.getFeature<ClusterFeature>().clusterInfo()) {
  if (ServerState::instance()->isDBServer() ||
      ServerState::instance()->isSingleServer()) {
    _dumpManager = server.getFeature<EngineSelectorFeature>()
                       .engine<RocksDBEngine>()
                       .dumpManager();
  }
}

// main function that dispatches the different routes and commands
RestStatus RestDumpHandler::execute() {
  if (!ServerState::instance()->isDBServer() &&
      !ServerState::instance()->isSingleServer()) {
    generateError(Result(
        TRI_ERROR_HTTP_NOT_IMPLEMENTED,
        "API only expected to be called on single server and DBServers"));
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
      // if DBserver property present, add user header
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

  auto useVPack = _request->parsedValue("useVPack", false);

  // adjust permissions in single server case, so that the behavior
  // is identical to non-parallel dumps
  ExecContextSuperuserScope escope(ExecContext::current().isAdminUser() &&
                                   ServerState::instance()->isSingleServer());

  auto guard =
      _dumpManager->createContext(std::move(opts), user, database, useVPack);

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

  // find() will throw in case the context cannot be found or the user does
  // not match.
  auto context = _dumpManager->find(id, database, user);
  // immediately prolong lifetime of context, so it doesn't get invalidated
  // while we are using it.
  context->extendLifetime();

  auto batch = context->next(*batchId, lastBatch);
  auto counts = context->getBlockCounts();

  if (batch == nullptr) {
    // all batches have been received
    return resetResponse(rest::ResponseCode::NO_CONTENT);
  }

  // output the batch value
  _response->setAllowCompression(
      rest::ResponseCompressionType::kAllowCompression);
  _response->setHeaderNC(StaticStrings::DumpShardId, std::string{batch->shard});
  _response->setHeaderNC(StaticStrings::DumpBlockCounts,
                         std::to_string(counts));
  _response->setContentType(rest::ContentType::DUMP);
  _response->addRawPayload(batch->content());
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

  // will throw if dump context is not found or cannot be accessed
  _dumpManager->remove(id, database, user);

  generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
}

std::string RestDumpHandler::getAuthorizedUser() const {
  if (ServerState::instance()->isSingleServer()) {
    // single server case
    return _request->user();
  }

  // cluster case
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

      if (!ServerState::instance()->isDBServer()) {
        // make this version of dump compatible with the previous version of
        // arangodump. the previous version assumed that as long as you are
        // an admin user, you can dump every collection
        ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());

        // validate permissions for all participating shards
        RocksDBDumpContextOptions opts;
        velocypack::deserializeUnsafe(body, opts);

        for (auto const& it : opts.shards) {
          // get collection name
          std::string collectionName;
          if (ServerState::instance()->isSingleServer()) {
            collectionName = it;
          } else {
            if (auto maybeShardID = ShardID::shardIdFromString(it);
                maybeShardID.ok()) {
              // If we are called without a shard, leave collectionName empty.
              collectionName =
                  _clusterInfo.getCollectionNameForShard(maybeShardID.get());
            }
          }
          if (!ExecContext::current().canUseCollection(
                  _request->databaseName(), collectionName, auth::Level::RO)) {
            return {TRI_ERROR_FORBIDDEN,
                    absl::StrCat("insufficient permissions to access shard ",
                                 it, " of collection ", collectionName)};
          }
        }
      }
    }

    return {};
  }

  // invalid HTTP method
  return {TRI_ERROR_HTTP_METHOD_NOT_ALLOWED};
}
