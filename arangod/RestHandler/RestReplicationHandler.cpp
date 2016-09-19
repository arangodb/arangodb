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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestReplicationHandler.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "GeneralServer/GeneralServer.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeys.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/TransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/ticks.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

uint64_t const RestReplicationHandler::defaultChunkSize = 128 * 1024;
uint64_t const RestReplicationHandler::maxChunkSize = 128 * 1024 * 1024;

RestReplicationHandler::RestReplicationHandler(GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestReplicationHandler::~RestReplicationHandler() {}

RestHandler::status RestReplicationHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffix = _request->suffix();

  size_t const len = suffix.size();

  if (len >= 1) {
    std::string const& command = suffix[0];

    if (command == "logger-state") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == "logger-tick-ranges") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status::DONE;
      }
      handleCommandLoggerTickRanges();
    } else if (command == "logger-first-tick") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status::DONE;
      }
      handleCommandLoggerFirstTick();
    } else if (command == "logger-follow") {
      if (type != rest::RequestType::GET && type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status::DONE;
      }
      handleCommandLoggerFollow();
    } else if (command == "determine-open-transactions") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandDetermineOpenTransactions();
    } else if (command == "batch") {
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == "barrier") {
      if (isCoordinatorError()) {
        return status::DONE;
      }
      handleCommandBarrier();
    } else if (command == "inventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      if (type != rest::RequestType::GET && type != rest::RequestType::POST &&
          type != rest::RequestType::PUT &&
          type != rest::RequestType::DELETE_REQ) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status::DONE;
      }

      if (type == rest::RequestType::POST) {
        handleCommandCreateKeys();
      } else if (type == rest::RequestType::GET) {
        handleCommandGetKeys();
      } else if (type == rest::RequestType::PUT) {
        handleCommandFetchKeys();
      } else if (type == rest::RequestType::DELETE_REQ) {
        handleCommandRemoveKeys();
      }
    } else if (command == "dump") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == "restore-collection") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == "restore-indexes") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == "restore-data") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleCommandRestoreDataCoordinator();
      } else {
        handleCommandRestoreData();
      }
    } else if (command == "sync") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status::DONE;
      }

      handleCommandSync();
    } else if (command == "make-slave") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status::DONE;
      }

      handleCommandMakeSlave();
    } else if (command == "server-id") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == "applier-config") {
      if (type == rest::RequestType::GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != rest::RequestType::PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == "applier-start") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status::DONE;
      }

      handleCommandApplierStart();
    } else if (command == "applier-stop") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status::DONE;
      }

      handleCommandApplierStop();
    } else if (command == "applier-state") {
      if (type == rest::RequestType::DELETE_REQ) {
        handleCommandApplierDeleteState();
      } else {
        if (type != rest::RequestType::GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == "clusterInventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else if (command == "addFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandAddFollower();
      }
    } else if (command == "removeFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandRemoveFollower();
      }
    } else if (command == "holdReadLockCollection") {
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        if (type == rest::RequestType::POST) {
          handleCommandHoldReadLockCollection();
        } else if (type == rest::RequestType::PUT) {
          handleCommandCheckHoldReadLockCollection();
        } else if (type == rest::RequestType::DELETE_REQ) {
          handleCommandCancelHoldReadLockCollection();
        } else if (type == rest::RequestType::GET) {
          handleCommandGetIdForReadLockCollection();
        } else {
          goto BAD_CALL;
        }
      }
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }

    return status::DONE;
  }

BAD_CALL:
  if (len != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return status::DONE;
}

/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges, this is
/// because edges depend on vertices being there), then name
bool RestReplicationHandler::sortCollections(
    arangodb::LogicalCollection const* l,
    arangodb::LogicalCollection const* r) {
  if (l->type() != r->type()) {
    return l->type() < r->type();
  }
  std::string const leftName = l->name();
  std::string const rightName = r->name();

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

/// @brief filter a collection based on collection attributes
bool RestReplicationHandler::filterCollection(
    arangodb::LogicalCollection* collection, void* data) {
  bool includeSystem = *((bool*)data);

  std::string const collectionName(collection->name());

  if (!includeSystem && collectionName[0] == '_') {
    // exclude all system collections
    return false;
  }

  if (TRI_ExcludeCollectionReplication(collectionName.c_str(), includeSystem)) {
    // collection is excluded from replication
    return false;
  }

  // all other cases should be included
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isCoordinatorError() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the applier action into an action list
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::insertClient(TRI_voc_tick_t lastServedTick) {
  bool found;
  std::string const& value = _request->value("serverId", found);

  if (found) {
    TRI_server_id_t serverId = (TRI_server_id_t)StringUtils::uint64(value);

    if (serverId > 0) {
      _vocbase->updateReplicationClient(serverId, lastServedTick);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the chunk size
////////////////////////////////////////////////////////////////////////////////

uint64_t RestReplicationHandler::determineChunkSize() const {
  // determine chunk size
  uint64_t chunkSize = defaultChunkSize;

  bool found;
  std::string const& value = _request->value("chunkSize", found);

  if (found) {
    // query parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);

    // don't allow overly big allocations
    if (chunkSize > maxChunkSize) {
      chunkSize = maxChunkSize;
    }
  }

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_return_state
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerState() {
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));  // Base

  arangodb::wal::LogfileManager::instance()->waitForSync(10.0);
  arangodb::wal::LogfileManagerState const s =
      arangodb::wal::LogfileManager::instance()->state();

  // "state" part
  builder.add("state", VPackValue(VPackValueType::Object));
  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(s.lastCommittedTick)));
  builder.add("lastUncommittedLogTick",
              VPackValue(std::to_string(s.lastAssignedTick)));
  builder.add("totalEvents", VPackValue(s.numEvents + s.numEventsSync));
  builder.add("time", VPackValue(s.timeString));
  builder.close();

  // "server" part
  builder.add("server", VPackValue(VPackValueType::Object));
  builder.add("version", VPackValue(ARANGODB_VERSION));
  builder.add("serverId",
              VPackValue(std::to_string(ServerIdFeature::getId())));
  builder.close();

  // "clients" part
  builder.add("clients", VPackValue(VPackValueType::Array));
  auto allClients = _vocbase->getReplicationClients();
  for (auto& it : allClients) {
    // One client
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("serverId", VPackValue(std::to_string(std::get<0>(it))));

    char buffer[21];
    TRI_GetTimeStampReplication(std::get<1>(it), &buffer[0], sizeof(buffer));
    builder.add("time", VPackValue(buffer));

    builder.add("lastServedTick",
                VPackValue(std::to_string(std::get<2>(it))));

    builder.close();
  }
  builder.close();  // clients

  builder.close();  // base

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_tick_ranges
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerTickRanges() {
  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();
  VPackBuilder b;
  b.add(VPackValue(VPackValueType::Array));

  for (auto& it : ranges) {
    b.add(VPackValue(VPackValueType::Object));
    b.add("datafile", VPackValue(it.filename));
    b.add("status", VPackValue(it.state));
    b.add("tickMin", VPackValue(std::to_string(it.tickMin)));
    b.add("tickMax", VPackValue(std::to_string(it.tickMax)));
    b.close();
  }

  b.close();
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_first_tick
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFirstTick() {
  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();

  VPackBuilder b;
  b.add(VPackValue(VPackValueType::Object));
  TRI_voc_tick_t tick = UINT64_MAX;

  for (auto& it : ranges) {
    if (it.tickMin == 0) {
      continue;
    }

    if (it.tickMin < tick) {
      tick = it.tickMin;
    }
  }

  if (tick == UINT64_MAX) {
    b.add("firstTick", VPackValue(VPackValueType::Null));
  } else {
    auto tickString = std::to_string(tick);
    b.add("firstTick", VPackValue(tickString));
  }
  b.close();
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_delete_batch_replication
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandBatch() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffix = _request->suffix();
  size_t const len = suffix.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new blocker
    std::shared_ptr<VPackBuilder> input =
        _request->toVelocyPackBuilderPtr(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires =
        VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t id;
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    int res = engine->insertCompactionBlocker(_vocbase, expires, id);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(id)));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffix[1]));

    auto input = _request->toVelocyPackBuilderPtr(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires =
        VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    // now extend the blocker
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    int res = engine->extendCompactionBlocker(_vocbase, id, expires);

    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffix[1]));

    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    int res = engine->removeCompactionBlocker(_vocbase, id);

    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add or remove a WAL logfile barrier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandBarrier() {
  // extract the request type
  auto const type = _request->requestType();
  std::vector<std::string> const& suffix = _request->suffix();
  size_t const len = suffix.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new barrier

    std::shared_ptr<VPackBuilder> input =
        _request->toVelocyPackBuilderPtr(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl =
        VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t minTick = 0;
    VPackSlice const v = input->slice().get("tick");

    if (v.isString()) {
      minTick = StringUtils::uint64(v.copyString());
    } else if (v.isNumber()) {
      minTick = v.getNumber<TRI_voc_tick_t>();
    }

    if (minTick == 0) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid tick value");
      return;
    }

    TRI_voc_tick_t id =
        arangodb::wal::LogfileManager::instance()->addLogfileBarrier(minTick,
                                                                     ttl);

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    std::string const idString(std::to_string(id));
    b.add("id", VPackValue(idString));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing barrier
    TRI_voc_tick_t id = StringUtils::uint64(suffix[1]);

    std::shared_ptr<VPackBuilder> input =
        _request->toVelocyPackBuilderPtr(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl =
        VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t minTick = 0;
    VPackSlice const v = input->slice().get("tick");

    if (v.isString()) {
      minTick = StringUtils::uint64(v.copyString());
    } else if (v.isNumber()) {
      minTick = v.getNumber<TRI_voc_tick_t>();
    }

    if (arangodb::wal::LogfileManager::instance()->extendLogfileBarrier(
            id, ttl, minTick)) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing barrier
    TRI_voc_tick_t id = StringUtils::uint64(suffix[1]);

    if (arangodb::wal::LogfileManager::instance()->removeLogfileBarrier(id)) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::GET) {
    // fetch all barriers
    auto ids = arangodb::wal::LogfileManager::instance()->getLogfileBarriers();

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Array));
    for (auto& it : ids) {
      b.add(VPackValue(std::to_string(it)));
    }
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator() {
  bool useVpp = false;
  if (_request->transportType() == Endpoint::TransportType::VPP) {
    useVpp = true;
  }
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");

  if (DBserver.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "need \"DBserver\" parameter");
    return;
  }

  std::string const& dbname = _request->databaseName();

  auto headers = std::make_shared<std::unordered_map<std::string, std::string>>(
      arangodb::getForwardableRequestHeaders(_request.get()));
  std::unordered_map<std::string, std::string> values = _request->values();
  std::string params;

  for (auto const& i : values) {
    if (i.first != "DBserver") {
      if (params.empty()) {
        params.push_back('?');
      } else {
        params.push_back('&');
      }
      params.append(StringUtils::urlEncode(i.first));
      params.push_back('=');
      params.append(StringUtils::urlEncode(i.second));
    }
  }

  // Set a few variables needed for our work:
  ClusterComm* cc = ClusterComm::instance();

  std::unique_ptr<ClusterCommResult> res;
  if (!useVpp) {
    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
    if (httpRequest == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    // Send a synchronous request to that shard using ClusterComm:
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), *headers, 300.0);
  } else {
    // do we need to handle multiple payloads here - TODO
    // here we switch vorm vpp to http?!
    // i am not allowed to change cluster comm!
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          _request->payload().toJson(), *headers, 300.0);
  }

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    // there is no result
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                  "lost connection within cluster");
    return;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    TRI_ASSERT(nullptr != res->result && res->result->isComplete());
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result.
    // We intentionally fall through here.
  }

  bool dummy;
  resetResponse(
      static_cast<rest::ResponseCode>(res->result->getHttpReturnCode()));

  _response->setContentType(
      res->result->getHeaderField(StaticStrings::ContentTypeHeader, dummy));

  if (!useVpp) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    httpResponse->body().swap(&(res->result->getBody()));
  } else {
    // TODO copy all payloads
    VPackSlice slice = res->result->getBodyVelocyPack()->slice();
    _response->setPayload(slice, true);  // do we need to generate the body?!
  }

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_returns
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFollow() {
  bool useVpp = false;
  if (_request->transportType() == Endpoint::TransportType::VPP) {
    useVpp = true;
  }

  // determine start and end tick
  arangodb::wal::LogfileManagerState const state =
      arangodb::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = UINT64_MAX;
  TRI_voc_tick_t firstRegularTick = 0;

  bool found;
  std::string const& value1 = _request->value("from", found);

  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
  }

  // determine end tick for dump
  std::string const& value2 = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  // check if a barrier id was specified in request
  TRI_voc_tid_t barrierId = 0;
  std::string const& value3 = _request->value("barrier", found);

  if (found) {
    barrierId = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value3));
  }

  bool includeSystem = true;
  std::string const& value4 = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value4);
  }

  // grab list of transactions from the body value
  std::unordered_set<TRI_voc_tid_t> transactionIds;

  if (_request->requestType() == arangodb::rest::RequestType::PUT) {
    std::string const& value5 = _request->value("firstRegularTick", found);

    if (found) {
      firstRegularTick =
          static_cast<TRI_voc_tick_t>(StringUtils::uint64(value5));
    }
    // copy default options
    VPackOptions options = VPackOptions::Defaults;
    options.checkAttributeUniqueness = true;
    std::shared_ptr<VPackBuilder> parsedRequest;
    VPackSlice slice;
    try {
      slice = _request->payload(&options);
    } catch (...) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }
    if (!slice.isArray()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }

    for (auto const& id : VPackArrayIterator(slice)) {
      if (!id.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "invalid body value. expecting array of ids");
        return;
      }
      transactionIds.emplace(StringUtils::uint64(id.copyString()));
    }
  }

  // extract collection
  TRI_voc_cid_t cid = 0;
  std::string const& value6 = _request->value("collection", found);

  if (found) {
    arangodb::LogicalCollection* c = _vocbase->lookupCollection(value6);

    if (c == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
      return;
    }

    cid = c->cid();
  }

  if (barrierId > 0) {
    // extend the WAL logfile barrier
    arangodb::wal::LogfileManager::instance()->extendLogfileBarrier(
        barrierId, 180, tickStart);
  }

  auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(_vocbase);

  // initialize the dump container
  TRI_replication_dump_t dump(transactionContext,
                              static_cast<size_t>(determineChunkSize()),
                              includeSystem, cid, useVpp);

  // and dump
  int res = TRI_DumpLogReplication(&dump, transactionIds, firstRegularTick,
                                tickStart, tickEnd, false);

  if (res == TRI_ERROR_NO_ERROR) {
    bool const checkMore = (dump._lastFoundTick > 0 &&
                            dump._lastFoundTick != state.lastCommittedTick);

    // generate the result
    size_t length = 0;
    if (useVpp) {
      length = dump._slices.size();
    } else {
      length = TRI_LengthStringBuffer(dump._buffer);
    }

    if (length == 0) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      resetResponse(rest::ResponseCode::OK);
    }

    // transfer ownership of the buffer contents
    _response->setContentType(rest::ContentType::DUMP);

    // set headers
    _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                            checkMore ? "true" : "false");

    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                            StringUtils::itoa(dump._lastFoundTick));

    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                            StringUtils::itoa(state.lastCommittedTick));

    _response->setHeaderNC(TRI_REPLICATION_HEADER_ACTIVE, "true");

    _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                            dump._fromTickIncluded ? "true" : "false");

    if (length > 0) {
      if (useVpp) {
        for (auto message : dump._slices) {
          _response->addPayload(std::move(message), &dump._vpackOptions, true);
        }
      } else {
        HttpResponse* httpResponse =
            dynamic_cast<HttpResponse*>(_response.get());

        if (httpResponse == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
        }

        if (length > 0) {
          // transfer ownership of the buffer contents
          httpResponse->body().set(dump._buffer);

          // to avoid double freeing
          TRI_StealStringBuffer(dump._buffer);
        }
      }
    }

    insertClient(dump._lastFoundTick);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDetermineOpenTransactions() {
  // determine start and end tick
  arangodb::wal::LogfileManagerState const state =
      arangodb::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = state.lastCommittedTick;

  bool found;
  std::string const& value1 = _request->value("from", found);

  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
  }

  // determine end tick for dump
  std::string const& value2 = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(_vocbase);

  // initialize the dump container
  TRI_replication_dump_t dump(transactionContext,
                              static_cast<size_t>(determineChunkSize()),
                              false, 0);

  // and dump
  int res = TRI_DetermineOpenTransactionsReplication(&dump, tickStart, tickEnd);

  if (res == TRI_ERROR_NO_ERROR) {
    // generate the result
    size_t const length = TRI_LengthStringBuffer(dump._buffer);

    if (length == 0) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      resetResponse(rest::ResponseCode::OK);
    }

    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    _response->setContentType(rest::ContentType::DUMP);

    _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                            dump._fromTickIncluded ? "true" : "false");

    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                            StringUtils::itoa(dump._lastFoundTick));

    if (length > 0) {
      // transfer ownership of the buffer contents
      httpResponse->body().set(dump._buffer);

      // to avoid double freeing
      TRI_StealStringBuffer(dump._buffer);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_inventory
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory() {
  TRI_voc_tick_t tick = TRI_CurrentTickServer();

  // include system collections?
  bool includeSystem = true;
  bool found;
  std::string const& value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  // collections and indexes
  std::shared_ptr<VPackBuilder> collectionsBuilder;
  collectionsBuilder =
      _vocbase->inventory(tick, &filterCollection, (void*)&includeSystem,
                          true, RestReplicationHandler::sortCollections);
  VPackSlice const collections = collectionsBuilder->slice();

  TRI_ASSERT(collections.isArray());

  VPackBuilder builder;
  builder.openObject();

  // add collections data
  builder.add("collections", collections);

  // "state"
  builder.add("state", VPackValue(VPackValueType::Object));

  arangodb::wal::LogfileManagerState const s =
      arangodb::wal::LogfileManager::instance()->state();

  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(s.lastCommittedTick)));
  builder.add("lastUncommittedLogTick",
              VPackValue(std::to_string(s.lastAssignedTick)));
  builder.add("totalEvents", VPackValue(s.numEvents + s.numEventsSync));
  builder.add("time", VPackValue(s.timeString));
  builder.close();  // state

  std::string const tickString(std::to_string(tick));
  builder.add("tick", VPackValue(tickString));
  builder.close();  // Toplevel

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandClusterInventory() {
  std::string const& dbName = _request->databaseName();
  bool found;
  bool includeSystem = true;

  std::string const& value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  AgencyComm _agency;
  AgencyCommResult result;

  std::string prefix("Plan/Collections/");
  prefix.append(dbName);

  result = _agency.getValues(prefix);
  if (!result.successful()) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
  } else {
    VPackSlice colls = result.slice()[0].get(std::vector<std::string>(
        {_agency.prefix(), "Plan", "Collections", dbName}));
    if (!colls.isObject()) {
      generateError(rest::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
    } else {
      VPackBuilder resultBuilder;
      {
        VPackObjectBuilder b1(&resultBuilder);
        resultBuilder.add(VPackValue("collections"));
        {
          VPackArrayBuilder b2(&resultBuilder);
          for (auto const& p : VPackObjectIterator(colls)) {
            VPackSlice const subResultSlice = p.value;
            if (subResultSlice.isObject()) {
              if (includeSystem ||
                  !arangodb::basics::VelocyPackHelper::getBooleanValue(
                      subResultSlice, "isSystem", true)) {
                VPackObjectBuilder b3(&resultBuilder);
                resultBuilder.add("indexes", subResultSlice.get("indexes"));
                resultBuilder.add("parameters", subResultSlice);
              }
            }
          }
        }
        TRI_voc_tick_t tick = TRI_CurrentTickServer();
        auto tickString = std::to_string(tick);
        resultBuilder.add("tick", VPackValue(tickString));
        resultBuilder.add("state", VPackValue("unused"));
      }
      generateResult(rest::ResponseCode::OK, resultBuilder.slice());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided TODO: MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::createCollection(VPackSlice const& slice,
                                             arangodb::LogicalCollection** dst,
                                             bool reuseId) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!slice.isObject()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(slice, "name", "");

  if (name.empty()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_voc_cid_t cid = 0;

  if (reuseId) {
    cid = arangodb::basics::VelocyPackHelper::extractIdValue(slice);

    if (cid == 0) {
      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          slice, "type", (int)TRI_COL_TYPE_DOCUMENT));

  arangodb::LogicalCollection* col = nullptr;

  if (cid > 0) {
    col = _vocbase->lookupCollection(cid);
  }

  if (col != nullptr && col->type() == type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  try {
    col = _vocbase->createCollection(slice, cid, true);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT(col != nullptr);

  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(col->doCompact() ==
             arangodb::basics::VelocyPackHelper::getBooleanValue(
                 slice, "doCompact", true));
  TRI_ASSERT(
      col->waitForSync() ==
      arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync",
          application_features::ApplicationServer::getFeature<DatabaseFeature>(
              "Database")
              ->waitForSync()));
  TRI_ASSERT(col->isVolatile() ==
             arangodb::basics::VelocyPackHelper::getBooleanValue(
                 slice, "isVolatile", false));
  TRI_ASSERT(col->isSystem() == (name[0] == '_'));
  TRI_ASSERT(
      col->indexBuckets() ==
      arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
          slice, "indexBuckets", DatabaseFeature::defaultIndexBuckets()));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_voc_cid_t planId = 0;
  VPackSlice const planIdSlice = slice.get("planId");
  if (planIdSlice.isNumber()) {
    planId =
        static_cast<TRI_voc_cid_t>(planIdSlice.getNumericValue<uint64_t>());
  } else if (planIdSlice.isString()) {
    std::string tmp = planIdSlice.copyString();
    planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
  } else if (planIdSlice.isNone()) {
    // There is no plan ID it has to be equal to collection id
    planId = col->cid();
  }

  TRI_ASSERT(col->planId() == planId);
#endif

  if (dst != nullptr) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreCollection() {
  std::shared_ptr<VPackBuilder> parsedRequest;

  // copy default options
  VPackOptions options = VPackOptions::Defaults;
  options.checkAttributeUniqueness = true;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr(&options);
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool overwrite = false;

  bool found;
  std::string const& value1 = _request->value("overwrite", found);

  if (found) {
    overwrite = StringUtils::boolean(value1);
  }

  bool recycleIds = false;
  std::string const& value2 = _request->value("recycleIds", found);

  if (found) {
    recycleIds = StringUtils::boolean(value2);
  }

  bool force = false;
  std::string const& value3 = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value3);
  }

  uint64_t numberOfShards = 0;
  std::string const& value4 = _request->value("numberOfShards", found);

  if (found) {
    numberOfShards = StringUtils::uint64(value4);
  }

  uint64_t replicationFactor = 1;
  std::string const& value5 = _request->value("replicationFactor", found);

  if (found) {
    replicationFactor = StringUtils::uint64(value5);
  }

  std::string errorMsg;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreCollectionCoordinator(slice, overwrite, recycleIds,
                                              force, numberOfShards, errorMsg,
                                              replicationFactor);
  } else {
    res =
        processRestoreCollection(slice, overwrite, recycleIds, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  } 
  
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreIndexes() {
  std::shared_ptr<VPackBuilder> parsedRequest;

  // copy default options
  VPackOptions options = VPackOptions::Defaults;
  options.checkAttributeUniqueness = true;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr(&options);
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool found;
  bool force = false;
  std::string const& value = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value);
  }

  std::string errorMsg;
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(slice, force, errorMsg);
  } else {
    res = processRestoreIndexes(slice, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
    
  VPackBuilder result;
  result.openObject();
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreCollection(
    VPackSlice const& collection, bool dropExisting, bool reuseId, bool force,
    std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  arangodb::LogicalCollection* col = nullptr;

  if (reuseId) {
    TRI_voc_cid_t const cid =
        arangodb::basics::VelocyPackHelper::extractIdValue(parameters);

    if (cid == 0) {
      errorMsg = "collection id is missing";

      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }

    // first look up the collection by the cid
    col = _vocbase->lookupCollection(cid);
  }

  if (col == nullptr) {
    // not found, try name next
    col = _vocbase->lookupCollection(name);
  }

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      int res = _vocbase->dropCollection(col, true, true);

      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        SingleCollectionTransaction trx(
            StandaloneTransactionContext::Create(_vocbase), col->cid(),
            TRI_TRANSACTION_WRITE);
        trx.addHint(TRI_TRANSACTION_HINT_RECOVERY,
                    false);  // to turn off waitForSync!

        res = trx.begin();
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        OperationOptions options;
        OperationResult opRes = trx.truncate(name, options);

        res = trx.finish(opRes.code);

        return res;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop collection '" + name + "': " +
                   std::string(TRI_errno_string(res));

        return res;
      }
    } else {
      int res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg = "unable to create collection '" + name + "': " +
                 std::string(TRI_errno_string(res));

      return res;
    }
  }

  // now re-create the collection
  int res = createCollection(parameters, &col, reuseId);

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to create collection: " + std::string(TRI_errno_string(res));

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreCollectionCoordinator(
    VPackSlice const& collection, bool dropExisting, bool reuseId, bool force,
    uint64_t numberOfShards, std::string& errorMsg,
    uint64_t replicationFactor) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  std::string dbName = _vocbase->name();

  ClusterInfo* ci = ClusterInfo::instance();

  try {
    // in a cluster, we only look up by name:
    std::shared_ptr<LogicalCollection> col = ci->getCollection(dbName, name);

    // drop an existing collection if it exists
    if (dropExisting) {
      int res = ci->dropCollectionCoordinator(dbName, col->cid_as_string(),
                                              errorMsg, 0.0);
      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped
        res = truncateCollectionOnCoordinator(dbName, name);
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg =
              "unable to truncate collection (dropping is forbidden): " + name;
          return res;
        }
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop collection '" + name + "': " +
                   std::string(TRI_errno_string(res));

        return res;
      }
    } else {
      int res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg = "unable to create collection '" + name + "': " +
                 std::string(TRI_errno_string(res));

      return res;
    }
  } catch (...) {
  }

  // now re-create the collection
  // dig out number of shards, explicit attribute takes precedence:
  VPackSlice const numberOfShardsSlice = parameters.get("numberOfShards");
  if (numberOfShardsSlice.isInteger()) {
    numberOfShards = numberOfShardsSlice.getNumericValue<uint64_t>();
  } else {
    VPackSlice const shards = parameters.get("shards");
    if (shards.isObject()) {
      numberOfShards = static_cast<uint64_t>(shards.length());
    } else {
      // "shards" not specified
      // now check if numberOfShards property was given
      if (numberOfShards == 0) {
        // We take one shard if no value was given
        numberOfShards = 1;
      }
    }
  }

  TRI_ASSERT(numberOfShards > 0);

  VPackSlice const replFactorSlice = parameters.get("replicationFactor");
  if (replFactorSlice.isInteger()) {
    replicationFactor =
        replFactorSlice.getNumericValue<decltype(replicationFactor)>();
  }
  if (replicationFactor == 0) {
    replicationFactor = 1;
  }

  TRI_voc_tick_t newIdTick = ci->uniqid(1);
  VPackBuilder toMerge;
  std::string&& newId = StringUtils::itoa(newIdTick);
  toMerge.openObject();
  toMerge.add("id", VPackValue(newId));

  // shard keys
  VPackSlice const shardKeys = parameters.get("shardKeys");
  if (!shardKeys.isObject()) {
    // set default shard key
    toMerge.add("shardKeys", VPackValue(VPackValueType::Array));
    toMerge.add(VPackValue(StaticStrings::KeyString));
    toMerge.close();  // end of shardKeys
  }

  // shards
  std::vector<std::string> dbServers;  // will be filled
  std::map<std::string, std::vector<std::string>> shardDistribution =
      arangodb::distributeShards(numberOfShards, replicationFactor,
                                  dbServers);
  if (shardDistribution.empty()) {
    errorMsg = "no database servers found in cluster";
    return TRI_ERROR_INTERNAL;
  }

  toMerge.add(VPackValue("shards"));
  {
    VPackObjectBuilder guard(&toMerge);
    for (auto const& p : shardDistribution) {
      toMerge.add(VPackValue(p.first));
      {
        VPackArrayBuilder guard2(&toMerge);
        for (std::string const& s : p.second) {
          toMerge.add(VPackValue(s));
        }
      }
    }
  }
  toMerge.add("replicationFactor", VPackValue(replicationFactor));

  // Now put in the primary and an edge index if needed:
  toMerge.add("indexes", VPackValue(VPackValueType::Array));

  // create a dummy primary index
  {
    arangodb::LogicalCollection* doc = nullptr;
    std::unique_ptr<arangodb::PrimaryIndex> primaryIndex(
        new arangodb::PrimaryIndex(doc));
    toMerge.openObject();
    primaryIndex->toVelocyPack(toMerge, false);
    toMerge.close();
  }

  VPackSlice const type = parameters.get("type");
  TRI_col_type_e collectionType;
  if (type.isNumber()) {
    collectionType = static_cast<TRI_col_type_e>(type.getNumericValue<int>());
  } else {
    errorMsg = "collection type not given or wrong";
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (collectionType == TRI_COL_TYPE_EDGE) {
    // create a dummy edge index
    std::unique_ptr<arangodb::EdgeIndex> edgeIndex(
        new arangodb::EdgeIndex(newIdTick, nullptr));
    toMerge.openObject();
    edgeIndex->toVelocyPack(toMerge, false);
    toMerge.close();
  }

  toMerge.close();  // indexes
  toMerge.close();  // TopLevel
  VPackSlice const sliceToMerge = toMerge.slice();

  VPackBuilder mergedBuilder =
      VPackCollection::merge(parameters, sliceToMerge, false);
  VPackSlice const merged = mergedBuilder.slice();

  int res = ci->createCollectionCoordinator(dbName, newId, numberOfShards,
                                            merged, errorMsg, 0.0);
  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to create collection: " + std::string(TRI_errno_string(res));
  }
    
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexes(VPackSlice const& collection,
                                                  bool force,
                                                  std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackValueLength const n = indexes.length();

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  READ_LOCKER(readLocker, _vocbase->_inventoryLock);

  // look up the collection
  try {
    CollectionGuard guard(_vocbase, name.c_str());

    LogicalCollection* collection = guard.collection();

    SingleCollectionTransaction trx(
        StandaloneTransactionContext::Create(_vocbase), collection->cid(),
        TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "unable to start transaction: " + std::string(TRI_errno_string(res));
      THROW_ARANGO_EXCEPTION(res);
    }

    for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
      std::shared_ptr<arangodb::Index> idx;

      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}

      res = collection->restoreIndex(&trx, idxDef, idx);

      if (res == TRI_ERROR_NOT_IMPLEMENTED) {
        continue;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg =
            "could not create index: " + std::string(TRI_errno_string(res));
        break;
      } else {
        TRI_ASSERT(idx != nullptr);

        res = collection->saveIndex(idx.get(), true);

        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg =
              "could not save index: " + std::string(TRI_errno_string(res));
          break;
        }
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    errorMsg =
        "could not create index: " + std::string(TRI_errno_string(ex.code()));
  } catch (...) {
    errorMsg = "could not create index: unknown error";
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexesCoordinator(
    VPackSlice const& collection, bool force, std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }
  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  size_t const n = static_cast<size_t>(indexes.length());

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  std::string dbName = _vocbase->name();

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> col;
  try {
    col = ci->getCollection(dbName, name);
  } catch (...) {
    errorMsg = "could not find collection '" + name + "'";
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  TRI_ASSERT(col != nullptr);

  int res = TRI_ERROR_NO_ERROR;
  for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
    VPackSlice type = idxDef.get("type");
    if (type.isString() &&
        (type.copyString() == "primary" || type.copyString() == "edge")) {
      // must ignore these types of indexes during restore
      continue;
    }

    VPackBuilder tmp;
    res = ci->ensureIndexCoordinator(dbName, col->cid_as_string(), idxDef, true,
                                     arangodb::Index::Compare, tmp, errorMsg,
                                     3600.0);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "could not create index: " + std::string(TRI_errno_string(res));
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump or the continuous log
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::applyCollectionDumpMarker(
    arangodb::Transaction& trx, CollectionNameResolver const& resolver,
    std::string const& collectionName, TRI_replication_operation_e type,
    VPackSlice const& old, VPackSlice const& slice, std::string& errorMsg) {
  if (type == REPLICATION_MARKER_DOCUMENT) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_ASSERT(slice.isObject());

    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;

    try {
      OperationResult opRes = trx.insert(collectionName, slice, options);

      if (opRes.code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        // must update
        opRes = trx.update(collectionName, slice, options);
      }

      return opRes.code;
    } catch (arangodb::basics::Exception const& ex) {
      return ex.code();
    } catch (...) {
      return TRI_ERROR_INTERNAL;
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}

    try {
      OperationOptions options;
      options.silent = true;
      options.ignoreRevs = true;
      options.waitForSync = false;
      OperationResult opRes = trx.remove(collectionName, old, options);

      if (!opRes.successful() &&
          opRes.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        return TRI_ERROR_NO_ERROR;
      }

      return opRes.code;
    } catch (arangodb::basics::Exception const& ex) {
      return ex.code();
    } catch (...) {
      return TRI_ERROR_INTERNAL;
    }
  }

  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
}

static int restoreDataParser(char const* ptr, char const* pos,
                             std::string const& invalidMsg, bool useRevision,
                             std::string& errorMsg, std::string& key,
                             std::string& rev, VPackBuilder& builder,
                             VPackSlice& doc,
                             TRI_replication_operation_e& type) {
  builder.clear();

  try {
    VPackParser parser(builder);
    parser.parse(ptr, static_cast<size_t>(pos - ptr));
  } catch (VPackException const&) {
    // Could not parse the given string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (std::exception const&) {
    // Could not even build the string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  }

  type = REPLICATION_INVALID;

  for (auto const& pair : VPackObjectIterator(slice, true)) {
    if (!pair.key.isString()) {
      errorMsg = invalidMsg;

      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }

    std::string const attributeName = pair.key.copyString();

    if (attributeName == "type") {
      if (pair.value.isNumber()) {
        int v = pair.value.getNumericValue<int>();
        if (v == 2301) {
          // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(v);
        }
      }
    }

    else if (attributeName == "data") {
      if (pair.value.isObject()) {
        doc = pair.value;

        if (doc.hasKey(StaticStrings::KeyString)) {
          key = doc.get(StaticStrings::KeyString).copyString();
        } else if (useRevision && doc.hasKey(StaticStrings::RevString)) {
          rev = doc.get(StaticStrings::RevString).copyString();
        }
      }
    }

    else if (attributeName == "key") {
      if (key.empty()) {
        key = pair.value.copyString();
      }
    }
  }

  if (type == REPLICATION_MARKER_DOCUMENT && !doc.isObject()) {
    errorMsg = "got document marker without contents";
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (key.empty()) {
    LOG(ERR) << "GOT EXCEPTION 5";
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreDataBatch(
    arangodb::Transaction& trx, CollectionNameResolver const& resolver,
    std::string const& collectionName, bool useRevision, bool force,
    std::string& errorMsg) {
  std::string const invalidMsg =
      "received invalid JSON data for collection " + collectionName;

  VPackBuilder builder;

  HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
  if (httpRequest == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::string const& bodyStr = httpRequest->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

  VPackBuilder allMarkers;
  VPackValueLength currentPos = 0;
  std::unordered_map<std::string, VPackValueLength> latest;

  // First parse and collect all markers, we assemble everything in one
  // large builder holding an array. We keep for each key the latest
  // entry.

  {
    VPackArrayBuilder guard(&allMarkers);
    while (ptr < end) {
      char const* pos = strchr(ptr, '\n');

      if (pos == nullptr) {
        pos = end;
      } else {
        *((char*)pos) = '\0';
      }

      if (pos - ptr > 1) {
        // found something
        std::string key;
        std::string rev;
        VPackSlice doc;
        TRI_replication_operation_e type = REPLICATION_INVALID;

        int res = restoreDataParser(ptr, pos, invalidMsg, useRevision, errorMsg,
                                    key, rev, builder, doc, type);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        // Put into array of all parsed markers:
        allMarkers.add(builder.slice());
        auto it = latest.find(key);
        if (it != latest.end()) {
          // Already found, overwrite:
          it->second = currentPos;
        } else {
          latest.emplace(std::make_pair(key, currentPos));
        }
        ++currentPos;
      }

      ptr = pos + 1;
    }
  }

  // First remove all keys of which the last marker we saw was a deletion
  // marker:
  VPackSlice allMarkersSlice = allMarkers.slice();
  VPackBuilder oldBuilder;
  {
    VPackArrayBuilder guard(&oldBuilder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_REMOVE) {
        oldBuilder.add(VPackValue(p.first));  // Add _key
      } else if (type != REPLICATION_MARKER_DOCUMENT) {
        errorMsg = "unexpected marker type " + StringUtils::itoa(type);
        return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
      }
    }
  }

  // Note that we ignore individual errors here, as long as the main
  // operation did not fail. In particular, we intentionally ignore
  // individual "DOCUMENT NOT FOUND" errors, because they can happen!
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    OperationResult opRes =
        trx.remove(collectionName, oldBuilder.slice(), options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  // Now try to insert all keys for which the last marker was a document
  // marker, note that these could still be replace markers!
  builder.clear();
  {
    VPackArrayBuilder guard(&builder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_DOCUMENT) {
        VPackSlice const doc = marker.get("data");
        TRI_ASSERT(doc.isObject());
        builder.add(doc);
      }
    }
  }

  VPackSlice requestSlice = builder.slice();
  OperationResult opRes;
  try {
    OperationOptions options;
    options.silent = false;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    opRes = trx.insert(collectionName, requestSlice, options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  // Now go through the individual results and check each error, if it was
  // TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, then we have to call
  // replace on the document:
  VPackSlice resultSlice = opRes.slice();
  VPackBuilder replBuilder;  // documents for replace operation
  {
    VPackArrayBuilder guard(&oldBuilder);
    VPackArrayBuilder guard2(&replBuilder);
    VPackArrayIterator itRequest(requestSlice);
    VPackArrayIterator itResult(resultSlice);

    while (itRequest.valid()) {
      VPackSlice result = *itResult;
      VPackSlice error = result.get("error");
      if (error.isTrue()) {
        error = result.get("errorNum");
        if (error.isNumber()) {
          int code = error.getNumericValue<int>();
          if (code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            replBuilder.add(*itRequest);
          } else {
            return code;
          }
        } else {
          return TRI_ERROR_INTERNAL;
        }
      }
      itRequest.next();
      itResult.next();
    }
  }
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    opRes = trx.replace(collectionName, replBuilder.slice(), options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreData(
    CollectionNameResolver const& resolver, TRI_voc_cid_t cid, bool useRevision,
    bool force, std::string& errorMsg) {
  SingleCollectionTransaction trx(
      StandaloneTransactionContext::Create(_vocbase), cid,
      TRI_TRANSACTION_WRITE);
  trx.addHint(TRI_TRANSACTION_HINT_RECOVERY,
              false);  // to turn off waitForSync!

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to start transaction: " + std::string(TRI_errno_string(res));

    return res;
  }

  res = processRestoreDataBatch(trx, resolver, trx.name(), useRevision, force,
                                errorMsg);

  res = trx.finish(res);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData() {
  std::string const& value1 = _request->value("collection");

  if (value1.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  CollectionNameResolver resolver(_vocbase);

  TRI_voc_cid_t cid = resolver.getCollectionIdLocal(value1);

  if (cid == 0) {
    std::string msg = "invalid collection parameter: '";
    msg += value1;
    msg += "', cid is 0";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return;
  }

  bool recycleIds = false;
  std::string const& value2 = _request->value("recycleIds");

  if (!value2.empty()) {
    recycleIds = StringUtils::boolean(value2);
  }

  bool force = false;
  std::string const& value3 = _request->value("force");

  if (!value3.empty()) {
    force = StringUtils::boolean(value3);
  }

  std::string errorMsg;

  int res = processRestoreData(resolver, cid, recycleIds, force, errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      generateError(GeneralResponse::responseCode(res), res);
    } else {
      generateError(GeneralResponse::responseCode(res), res,
                    std::string(TRI_errno_string(res)) + ": " + errorMsg);
    }
  } else {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("result", VPackValue(true));
    result.close();
    generateResult(rest::ResponseCode::OK, result.slice());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreDataCoordinator() {
  std::string const& name = _request->value("collection");

  if (name.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  std::string dbName = _vocbase->name();
  std::string errorMsg;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> col;
  try {
    col = ci->getCollection(dbName, name);
  } catch (...) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  // We need to distribute the documents we get over the shards:
  auto shardIdsMap = col->shardIds();

  std::unordered_map<std::string, size_t> shardTab;
  std::vector<std::string> shardIds;
  for (auto const& p : *shardIdsMap) {
    shardTab.emplace(p.first, shardIds.size());
    shardIds.push_back(p.first);
  }
  std::vector<StringBuffer*> bufs;
  for (size_t j = 0; j < shardIds.size(); j++) {
    auto b = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE);
    bufs.push_back(b.get());
    b.release();
  }

  std::string const invalidMsg =
      std::string("received invalid JSON data for collection ") + name;
  VPackBuilder builder;

  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
  if (httpRequest == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::string const& bodyStr = httpRequest->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

  int res = TRI_ERROR_NO_ERROR;

  while (ptr < end) {
    char const* pos = strchr(ptr, '\n');

    if (pos == 0) {
      pos = end;
    }

    if (pos - ptr > 1) {
      // found something
      //
      std::string key;
      std::string rev;
      VPackSlice doc;
      TRI_replication_operation_e type = REPLICATION_INVALID;

      res = restoreDataParser(ptr, pos, invalidMsg, false, errorMsg, key, rev,
                              builder, doc, type);
      if (res != TRI_ERROR_NO_ERROR) {
        // We might need to clean up buffers
        break;
      }

      if (!doc.isNone() && type != REPLICATION_MARKER_REMOVE) {
        ShardID responsibleShard;
        bool usesDefaultSharding;
        res = ci->getResponsibleShard(col->cid_as_string(), doc, true,
                                      responsibleShard, usesDefaultSharding);
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "error during determining responsible shard";
          res = TRI_ERROR_INTERNAL;
          break;
        } else {
          auto it2 = shardTab.find(responsibleShard);
          if (it2 == shardTab.end()) {
            errorMsg = "cannot find responsible shard";
            res = TRI_ERROR_INTERNAL;
            break;
          } else {
            bufs[it2->second]->appendText(ptr, pos - ptr);
            bufs[it2->second]->appendText("\n");
          }
        }
      } else if (type == REPLICATION_MARKER_REMOVE) {
        // A remove marker, this has to be appended to all!
        for (auto& buf : bufs) {
          buf->appendText(ptr, pos - ptr);
          buf->appendText("\n");
        }
      } else {
        // How very strange!
        errorMsg = invalidMsg;

        res = TRI_ERROR_HTTP_BAD_PARAMETER;
        break;
      }
    }

    ptr = pos + 1;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // Set a few variables needed for our work:
    ClusterComm* cc = ClusterComm::instance();

    // Send a synchronous request to that shard using ClusterComm:
    CoordTransactionID coordTransactionID = TRI_NewTickServer();

    std::string forceopt;
    std::string const& value = _request->value("force");

    if (!value.empty()) {
      bool force = StringUtils::boolean(value);

      if (force) {
        forceopt = "&force=true";
      }
    }

    for (auto const& p : *shardIdsMap) {
      auto it = shardTab.find(p.first);
      if (it == shardTab.end()) {
        errorMsg = "cannot find shard";
        res = TRI_ERROR_INTERNAL;
      } else {
        auto headers =
            std::make_unique<std::unordered_map<std::string, std::string>>();
        size_t j = it->second;
        auto body = std::make_shared<std::string const>(bufs[j]->c_str(),
                                                        bufs[j]->length());
        cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                         arangodb::rest::RequestType::PUT,
                         "/_db/" + StringUtils::urlEncode(dbName) +
                             "/_api/replication/restore-data?collection=" +
                             p.first + forceopt,
                         body, headers, nullptr, 300.0);
      }
    }

    // Now listen to the results:
    unsigned int count;
    unsigned int nrok = 0;
    for (count = (int)(*shardIdsMap).size(); count > 0; count--) {
      auto result = cc->wait("", coordTransactionID, 0, "", 0.0);
      if (result.status == CL_COMM_RECEIVED) {
        if (result.answer_code == rest::ResponseCode::OK ||
            result.answer_code == rest::ResponseCode::CREATED) {
          // copy default options
          VPackOptions options = VPackOptions::Defaults;
          options.checkAttributeUniqueness = true;

          VPackSlice answer;
          try {
            answer = result.answer->payload(&options);
          } catch (VPackException const& e) {
            // Only log this error and try the next doc
            LOG(DEBUG) << "failed to parse json object: '" << e.what() << "'";
            continue;
          }

          if (answer.isObject()) {
            VPackSlice const result = answer.get("result");
            if (result.isBoolean()) {
              if (result.getBoolean()) {
                nrok++;
              } else {
                LOG(ERR) << "some shard result not OK";
              }
            } else {
              VPackSlice const errorMessage = answer.get("errorMessage");
              if (errorMessage.isString()) {
                errorMsg.append(errorMessage.copyString());
                errorMsg.push_back(':');
              }
            }
          } else {
            LOG(ERR) << "result body is no object";
          }
        } else if (result.answer_code == rest::ResponseCode::SERVER_ERROR) {
          // copy default options
          VPackOptions options = VPackOptions::Defaults;
          options.checkAttributeUniqueness = true;
          VPackSlice answer;
          try {
            answer = result.answer->payload(&options);
          } catch (VPackException const& e) {
            // Only log this error and try the next doc
            LOG(DEBUG) << "failed to parse json object: '" << e.what() << "'";
            continue;
          }

          if (answer.isObject()) {
            VPackSlice const errorMessage = answer.get("errorMessage");
            if (errorMessage.isString()) {
              errorMsg.append(errorMessage.copyString());
              errorMsg.push_back(':');
            }
          }
        } else {
          LOG(ERR) << "Bad answer code from shard: " << (int)result.answer_code;
        }
      } else {
        LOG(ERR) << "Bad status from DBServer: " << result.status
                 << ", msg: " << result.errorMessage
                 << ", shard: " << result.shardID;
        if (result.status >= CL_COMM_SENT) {
          if (result.result.get() == nullptr) {
            LOG(ERR) << "result.result is nullptr";
          } else {
            auto msg = result.result->getResultTypeMessage();
            LOG(ERR) << "Bad HTTP return code: "
                     << result.result->getHttpReturnCode() << ", msg: " << msg;
            auto body = result.result->getBodyVelocyPack();
            msg = body->toString();
            LOG(ERR) << "Body: " << msg;
          }
        }
      }
    }

    if (nrok != shardIdsMap->size()) {
      errorMsg.append("some shard(s) produced error(s)");
      res = TRI_ERROR_INTERNAL;
    }
  }

  // Free all the string buffers:
  for (size_t j = 0; j < shardIds.size(); j++) {
    delete bufs[j];
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder result;
  result.openObject();
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCreateKeys() {
  std::string const& collection = _request->value("collection");

  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  TRI_voc_tick_t tickEnd = UINT64_MAX;

  // determine end tick for keys
  bool found;
  std::string const& value = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  arangodb::LogicalCollection* c = _vocbase->lookupCollection(collection);

  if (c == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  arangodb::CollectionGuard guard(_vocbase, c->cid(), false);

  arangodb::LogicalCollection* col = guard.collection();
  TRI_ASSERT(col != nullptr);

  // turn off the compaction for the collection
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_voc_tick_t id;
  int res = engine->insertCompactionBlocker(_vocbase, 1200.0, id);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // initialize a container with the keys
  auto keys =
      std::make_unique<CollectionKeys>(_vocbase, col->name(), id, 300.0);

  std::string const idString(std::to_string(keys->id()));

  keys->create(tickEnd);
  size_t const count = keys->count();

  auto keysRepository = _vocbase->collectionKeys();

  keysRepository->store(keys.get());
  keys.release();

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("id", VPackValue(idString));
  result.add("count", VPackValue(count));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/replication/keys/<keys-id>");
    return;
  }

  static uint64_t const DefaultChunkSize = 5000;
  uint64_t chunkSize = DefaultChunkSize;

  // determine chunk size
  bool found;
  std::string const& value = _request->value("chunkSize", found);

  if (found) {
    chunkSize = StringUtils::uint64(value);
    if (chunkSize < 100) {
      chunkSize = DefaultChunkSize;
    } else if (chunkSize > 20000) {
      chunkSize = 20000;
    }
  }

  std::string const& id = suffix[1];

  auto keysRepository = _vocbase->collectionKeys();
  TRI_ASSERT(keysRepository != nullptr);

  auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
      arangodb::basics::StringUtils::uint64(id));

  auto collectionKeys = keysRepository->find(collectionKeysId);

  if (collectionKeys == nullptr) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                  TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  try {
    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Array));

    TRI_voc_tick_t max = static_cast<TRI_voc_tick_t>(collectionKeys->count());

    for (TRI_voc_tick_t from = 0; from < max; from += chunkSize) {
      TRI_voc_tick_t to = from + chunkSize;

      if (to > max) {
        to = max;
      }

      auto result = collectionKeys->hashChunk(static_cast<size_t>(from),
                                              static_cast<size_t>(to));

      // Add a chunk
      b.add(VPackValue(VPackValueType::Object));
      b.add("low", VPackValue(std::get<0>(result)));
      b.add("high", VPackValue(std::get<1>(result)));
      b.add("hash", VPackValue(std::to_string(std::get<2>(result))));
      b.close();
    }
    b.close();

    collectionKeys->release();
    generateResult(rest::ResponseCode::OK, b.slice());
  } catch (...) {
    collectionKeys->release();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandFetchKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/replication/keys/<keys-id>");
    return;
  }

  static uint64_t const DefaultChunkSize = 5000;
  uint64_t chunkSize = DefaultChunkSize;

  // determine chunk size
  bool found;
  std::string const& value1 = _request->value("chunkSize", found);

  if (found) {
    chunkSize = StringUtils::uint64(value1);
    if (chunkSize < 100) {
      chunkSize = DefaultChunkSize;
    } else if (chunkSize > 20000) {
      chunkSize = 20000;
    }
  }

  std::string const& value2 = _request->value("chunk", found);

  size_t chunk = 0;

  if (found) {
    chunk = static_cast<size_t>(StringUtils::uint64(value2));
  }

  std::string const& value3 = _request->value("type", found);

  bool keys = true;
  if (value3 == "keys") {
    keys = true;
  } else if (value3 == "docs") {
    keys = false;
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid 'type' value");
    return;
  }

  std::string const& id = suffix[1];

  auto keysRepository = _vocbase->collectionKeys();
  TRI_ASSERT(keysRepository != nullptr);

  auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
      arangodb::basics::StringUtils::uint64(id));

  auto collectionKeys = keysRepository->find(collectionKeysId);

  if (collectionKeys == nullptr) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                  TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  try {
    std::shared_ptr<TransactionContext> transactionContext =
        StandaloneTransactionContext::Create(_vocbase);

    VPackBuilder resultBuilder(transactionContext->getVPackOptions());
    resultBuilder.openArray();

    if (keys) {
      collectionKeys->dumpKeys(resultBuilder, chunk,
                                static_cast<size_t>(chunkSize));
    } else {
      bool success;
      std::shared_ptr<VPackBuilder> parsedIds =
          parseVelocyPackBody(&VPackOptions::Defaults, success);
      if (!success) {
        // error already created
        collectionKeys->release();
        return;
      }
      collectionKeys->dumpDocs(resultBuilder, chunk,
                                static_cast<size_t>(chunkSize),
                                parsedIds->slice());
    }

    resultBuilder.close();

    collectionKeys->release();

    generateResult(rest::ResponseCode::OK, resultBuilder.slice(),
                    transactionContext);
    return;
  } catch (...) {
    collectionKeys->release();
    throw;
  }
}

void RestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  std::string const& id = suffix[1];

  auto keys = _vocbase->collectionKeys();
  TRI_ASSERT(keys != nullptr);

  auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = keys->remove(collectionKeysId);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add("id", VPackValue(id));  // id as a string
  resultBuilder.add("error", VPackValue(false));
  resultBuilder.add("code",
                    VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  resultBuilder.close();

  generateResult(rest::ResponseCode::ACCEPTED, resultBuilder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_dump
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump() {
  std::string const& collection = _request->value("collection");

  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // determine start tick for dump
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = static_cast<TRI_voc_tick_t>(UINT64_MAX);
  bool flush = true;  // flush WAL before dumping?
  bool withTicks = true;
  uint64_t flushWait = 0;

  // determine flush WAL value
  bool found;
  std::string const& value1 = _request->value("flush", found);

  if (found) {
    flush = StringUtils::boolean(value1);
  }

  // determine flush WAL wait time value
  std::string const& value3 = _request->value("flushWait", found);

  if (found) {
    flushWait = StringUtils::uint64(value3);
    if (flushWait > 60) {
      flushWait = 60;
    }
  }

  // determine start tick for dump
  std::string const& value4 = _request->value("from", found);

  if (found) {
    tickStart = (TRI_voc_tick_t)StringUtils::uint64(value4);
  }

  // determine end tick for dump
  std::string const& value5 = _request->value("to", found);

  if (found) {
    tickEnd = (TRI_voc_tick_t)StringUtils::uint64(value5);
  }

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  bool includeSystem = true;

  std::string const& value6 = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value6);
  }

  std::string const& value7 = _request->value("ticks", found);

  if (found) {
    withTicks = StringUtils::boolean(value7);
  }

  bool compat28 = false;
  std::string const& value8 = _request->value("compat28", found);

  if (found) {
    compat28 = StringUtils::boolean(value8);
  }

  arangodb::LogicalCollection* c = _vocbase->lookupCollection(collection);

  if (c == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  LOG(TRACE) << "requested collection dump for collection '" << collection
             << "', tickStart: " << tickStart << ", tickEnd: " << tickEnd;

  if (flush) {
    arangodb::wal::LogfileManager::instance()->flush(true, true, false);

    // additionally wait for the collector
    if (flushWait > 0) {
      arangodb::wal::LogfileManager::instance()->waitForCollectorQueue(
          c->cid(), static_cast<double>(flushWait));
    }
  }

  arangodb::CollectionGuard guard(_vocbase, c->cid(), false);

  arangodb::LogicalCollection* col = guard.collection();
  TRI_ASSERT(col != nullptr);

  auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(_vocbase);

  // initialize the dump container
  TRI_replication_dump_t dump(transactionContext,
                              static_cast<size_t>(determineChunkSize()),
                              includeSystem, 0);

  if (compat28) {
    dump._compat28 = true;
  }

  int res = TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd,
                                      withTicks);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // generate the result
  size_t const length = TRI_LengthStringBuffer(dump._buffer);

  if (length == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }

  // TODO needs to generalized
  auto response = dynamic_cast<HttpResponse*>(_response.get());

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  response->setContentType(rest::ContentType::DUMP);

  // set headers
  _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                          (dump._hasMore ? "true" : "false"));

  _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                          StringUtils::itoa(dump._lastFoundTick));

  // transfer ownership of the buffer contents
  response->body().set(dump._buffer);

  // avoid double freeing
  TRI_StealStringBuffer(dump._buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandMakeSlave() {
  bool success;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (endpoint.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }

  std::string const database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->name());
  std::string const username =
      VelocyPackHelper::getStringValue(body, "username", "");
  std::string const password =
      VelocyPackHelper::getStringValue(body, "password", "");
  std::string const restrictType =
      VelocyPackHelper::getStringValue(body, "restrictType", "");

  // initialize some defaults to copy from
  TRI_replication_applier_configuration_t defaults;

  // initialize target configuration
  TRI_replication_applier_configuration_t config;

  config._endpoint = endpoint;
  config._database = database;
  config._username = username;
  config._password = password;
  config._includeSystem =
      VelocyPackHelper::getBooleanValue(body, "includeSystem", true);
  config._requestTimeout = VelocyPackHelper::getNumericValue<double>(
      body, "requestTimeout", defaults._requestTimeout);
  config._connectTimeout = VelocyPackHelper::getNumericValue<double>(
      body, "connectTimeout", defaults._connectTimeout);
  config._ignoreErrors = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "ignoreErrors", defaults._ignoreErrors);
  config._maxConnectRetries = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "maxConnectRetries", defaults._maxConnectRetries);
  config._sslProtocol = VelocyPackHelper::getNumericValue<uint32_t>(
      body, "sslProtocol", defaults._sslProtocol);
  config._chunkSize = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "chunkSize", defaults._chunkSize);
  config._autoStart = true;
  config._adaptivePolling = VelocyPackHelper::getBooleanValue(
      body, "adaptivePolling", defaults._adaptivePolling);
  config._autoResync = VelocyPackHelper::getBooleanValue(body, "autoResync",
                                                         defaults._autoResync);
  config._verbose =
      VelocyPackHelper::getBooleanValue(body, "verbose", defaults._verbose);
  config._incremental = VelocyPackHelper::getBooleanValue(
      body, "incremental", defaults._incremental);
  config._useCollectionId = VelocyPackHelper::getBooleanValue(
      body, "useCollectionId", defaults._useCollectionId);
  config._requireFromPresent = VelocyPackHelper::getBooleanValue(
      body, "requireFromPresent", defaults._requireFromPresent);
  config._restrictType = VelocyPackHelper::getStringValue(
      body, "restrictType", defaults._restrictType);
  config._connectionRetryWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "connectionRetryWaitTime",
          static_cast<double>(defaults._connectionRetryWaitTime) /
              (1000.0 * 1000.0)));
  config._initialSyncMaxWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "initialSyncMaxWaitTime",
          static_cast<double>(defaults._initialSyncMaxWaitTime) /
              (1000.0 * 1000.0)));
  config._idleMinWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "idleMinWaitTime",
          static_cast<double>(defaults._idleMinWaitTime) / (1000.0 * 1000.0)));
  config._idleMaxWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "idleMaxWaitTime",
          static_cast<double>(defaults._idleMaxWaitTime) / (1000.0 * 1000.0)));
  config._autoResyncRetries = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "autoResyncRetries", defaults._autoResyncRetries);

  VPackSlice const restriction = body.get("restrictCollections");

  if (restriction.isArray()) {
    VPackValueLength const n = restriction.length();

    for (VPackValueLength i = 0; i < n; ++i) {
      VPackSlice const cname = restriction.at(i);
      if (cname.isString()) {
        config._restrictCollections.emplace(cname.copyString(), true);
      }
    }
  }

  // now the configuration is complete

  if ((restrictType.empty() && !config._restrictCollections.empty()) ||
      (!restrictType.empty() && config._restrictCollections.empty()) ||
      (!restrictType.empty() && restrictType != "include" &&
       restrictType != "exclude")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  // forget about any existing replication applier configuration
  int res = _vocbase->replicationApplier()->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // start initial synchronization
  TRI_voc_tick_t lastLogTick = 0;
  TRI_voc_tick_t barrierId = 0;
  std::string errorMsg = "";
  {
    InitialSyncer syncer(_vocbase, &config, config._restrictCollections,
                         restrictType, false);

    res = TRI_ERROR_NO_ERROR;

    try {
      res = syncer.run(errorMsg, false);

      // steal the barrier from the syncer
      barrierId = syncer.stealBarrier();
    } catch (...) {
      errorMsg = "caught an exception";
      res = TRI_ERROR_INTERNAL;
    }

    lastLogTick = syncer.getLastLogTick();
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, errorMsg);
    return;
  }

  res =
      TRI_ConfigureReplicationApplier(_vocbase->replicationApplier(), &config);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
    return;
  }

  res = _vocbase->replicationApplier()->start(lastLogTick, true, barrierId);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
    return;
  }

  std::shared_ptr<VPackBuilder> result =
      _vocbase->replicationApplier()->toVelocyPack();
  generateResult(rest::ResponseCode::OK, result->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_synchronize
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSync() {
  bool success;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }

  VPackSlice const body = parsedBody->slice();

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (endpoint.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }

  std::string const database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->name());
  std::string const username =
      VelocyPackHelper::getStringValue(body, "username", "");
  std::string const password =
      VelocyPackHelper::getStringValue(body, "password", "");
  bool const verbose =
      VelocyPackHelper::getBooleanValue(body, "verbose", false);
  bool const includeSystem =
      VelocyPackHelper::getBooleanValue(body, "includeSystem", true);
  bool const incremental =
      VelocyPackHelper::getBooleanValue(body, "incremental", false);
  bool const keepBarrier =
      VelocyPackHelper::getBooleanValue(body, "keepBarrier", false);
  bool const useCollectionId =
      VelocyPackHelper::getBooleanValue(body, "useCollectionId", true);

  std::unordered_map<std::string, bool> restrictCollections;
  VPackSlice const restriction = body.get("restrictCollections");

  if (restriction.isArray()) {
    for (VPackSlice const& cname : VPackArrayIterator(restriction)) {
      if (cname.isString()) {
        restrictCollections.insert(
            std::pair<std::string, bool>(cname.copyString(), true));
      }
    }
  }

  std::string restrictType =
      VelocyPackHelper::getStringValue(body, "restrictType", "");

  if ((restrictType.empty() && !restrictCollections.empty()) ||
      (!restrictType.empty() && restrictCollections.empty()) ||
      (!restrictType.empty() && restrictType != "include" &&
       restrictType != "exclude")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  TRI_replication_applier_configuration_t config;
  config._endpoint = endpoint;
  config._database = database;
  config._username = username;
  config._password = password;
  config._includeSystem = includeSystem;
  config._verbose = verbose;
  config._useCollectionId = useCollectionId;

  // wait until all data in current logfile got synced
  arangodb::wal::LogfileManager::instance()->waitForSync(5.0);

  InitialSyncer syncer(_vocbase, &config, restrictCollections, restrictType,
                       verbose);

  std::string errorMsg = "";

  /*int res = */syncer.run(errorMsg, incremental);

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));

  result.add("collections", VPackValue(VPackValueType::Array));

  for (auto const& it : syncer.getProcessedCollections()) {
    std::string const cidString = StringUtils::itoa(it.first);
    // Insert a collection
    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(cidString));
    result.add("name", VPackValue(it.second));
    result.close();  // one collection
  }

  result.close();  // collections

  auto tickString = std::to_string(syncer.getLastLogTick());
  result.add("lastLogTick", VPackValue(tickString));

  if (keepBarrier) {
    auto barrierId = std::to_string(syncer.stealBarrier());
    result.add("barrierId", VPackValue(barrierId));
  }

  result.close();  // base
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_serverID
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandServerId() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId());
  result.add("serverId", VPackValue(serverId));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetConfig() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  TRI_replication_applier_configuration_t config;

  {
    READ_LOCKER(readLocker, _vocbase->replicationApplier()->_statusLock);
    config.update(&_vocbase->replicationApplier()->_configuration);
  }
  std::shared_ptr<VPackBuilder> configBuilder = config.toVelocyPack(false);
  generateResult(rest::ResponseCode::OK, configBuilder->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  TRI_replication_applier_configuration_t config;

  bool success;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);

  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();

  {
    READ_LOCKER(readLocker, _vocbase->replicationApplier()->_statusLock);
    config.update(&_vocbase->replicationApplier()->_configuration);
  }

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (!endpoint.empty()) {
    config._endpoint = endpoint;
  }

  config._database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->name());

  VPackSlice const username = body.get("username");
  if (username.isString()) {
    config._username = username.copyString();
  }

  VPackSlice const password = body.get("password");
  if (password.isString()) {
    config._password = password.copyString();
  }

  config._requestTimeout = VelocyPackHelper::getNumericValue<double>(
      body, "requestTimeout", config._requestTimeout);
  config._connectTimeout = VelocyPackHelper::getNumericValue<double>(
      body, "connectTimeout", config._connectTimeout);
  config._ignoreErrors = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "ignoreErrors", config._ignoreErrors);
  config._maxConnectRetries = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "maxConnectRetries", config._maxConnectRetries);
  config._sslProtocol = VelocyPackHelper::getNumericValue<uint32_t>(
      body, "sslProtocol", config._sslProtocol);
  config._chunkSize = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "chunkSize", config._chunkSize);
  config._autoStart =
      VelocyPackHelper::getBooleanValue(body, "autoStart", config._autoStart);
  config._adaptivePolling = VelocyPackHelper::getBooleanValue(
      body, "adaptivePolling", config._adaptivePolling);
  config._autoResync =
      VelocyPackHelper::getBooleanValue(body, "autoResync", config._autoResync);
  config._includeSystem = VelocyPackHelper::getBooleanValue(
      body, "includeSystem", config._includeSystem);
  config._verbose =
      VelocyPackHelper::getBooleanValue(body, "verbose", config._verbose);
  config._incremental = VelocyPackHelper::getBooleanValue(body, "incremental",
                                                          config._incremental);
  config._requireFromPresent = VelocyPackHelper::getBooleanValue(
      body, "requireFromPresent", config._requireFromPresent);
  config._restrictType = VelocyPackHelper::getStringValue(body, "restrictType",
                                                          config._restrictType);
  config._connectionRetryWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "connectionRetryWaitTime",
          static_cast<double>(config._connectionRetryWaitTime) /
              (1000.0 * 1000.0)));
  config._initialSyncMaxWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "initialSyncMaxWaitTime",
          static_cast<double>(config._initialSyncMaxWaitTime) /
              (1000.0 * 1000.0)));
  config._idleMinWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "idleMinWaitTime",
          static_cast<double>(config._idleMinWaitTime) / (1000.0 * 1000.0)));
  config._idleMaxWaitTime = static_cast<uint64_t>(
      1000.0 * 1000.0 *
      VelocyPackHelper::getNumericValue<double>(
          body, "idleMaxWaitTime",
          static_cast<double>(config._idleMaxWaitTime) / (1000.0 * 1000.0)));
  config._autoResyncRetries = VelocyPackHelper::getNumericValue<uint64_t>(
      body, "autoResyncRetries", config._autoResyncRetries);

  VPackSlice const restriction = body.get("restrictCollections");
  if (restriction.isArray()) {
    config._restrictCollections.clear();
    for (VPackSlice const& collection : VPackArrayIterator(restriction)) {
      if (collection.isString()) {
        config._restrictCollections.emplace(
            std::make_pair(collection.copyString(), true));
      }
    }
  }

  int res =
      TRI_ConfigureReplicationApplier(_vocbase->replicationApplier(), &config);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  handleCommandApplierGetConfig();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_start
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStart() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  bool found;
  std::string const& value1 = _request->value("from", found);

  TRI_voc_tick_t initialTick = 0;
  bool useTick = false;

  if (found) {
    // query parameter "from" specified
    initialTick = (TRI_voc_tick_t)StringUtils::uint64(value1);
    useTick = true;
  }

  TRI_voc_tick_t barrierId = 0;
  std::string const& value2 = _request->value("barrierId", found);

  if (found) {
    // query parameter "barrierId" specified
    barrierId = (TRI_voc_tick_t)StringUtils::uint64(value2);
  }

  int res =
      _vocbase->replicationApplier()->start(initialTick, useTick, barrierId);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_stop
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStop() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  int res = _vocbase->replicationApplier()->stop(true);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetState() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  std::shared_ptr<VPackBuilder> result =
      _vocbase->replicationApplier()->toVelocyPack();
  generateResult(rest::ResponseCode::OK, result->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierDeleteState() {
  TRI_ASSERT(_vocbase->replicationApplier() != nullptr);

  int res = _vocbase->replicationApplier()->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower of a shard to the list of followers
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandAddFollower() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attributes 'followerId' and 'shard'");
    return;
  }
  VPackSlice const followerId = body.get("followerId");
  VPackSlice const shard = body.get("shard");
  if (!followerId.isString() || !shard.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId' and 'shard' attributes must be strings");
    return;
  }

  auto col = _vocbase->lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  col->followers()->add(followerId.copyString());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower of a shard from the list of followers
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRemoveFollower() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attributes 'followerId' and 'shard'");
    return;
  }
  VPackSlice const followerId = body.get("followerId");
  VPackSlice const shard = body.get("shard");
  if (!followerId.isString() || !shard.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId' and 'shard' attributes must be strings");
    return;
  }

  auto col = _vocbase->lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  col->followers()->remove(followerId.copyString());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hold a read lock on a collection to stop writes temporarily
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'collection', "
                  "'ttl' and 'id'");
    return;
  }
  VPackSlice const collection = body.get("collection");
  VPackSlice const ttlSlice = body.get("ttl");
  VPackSlice const idSlice = body.get("id");
  if (!collection.isString() || !ttlSlice.isNumber() || !idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'collection' must be a string and 'ttl' a number and "
                  "'id' a string");
    return;
  }
  std::string id = idSlice.copyString();

  auto col = _vocbase->lookupCollection(collection.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  double ttl = 0.0;
  if (ttlSlice.isInteger()) {
    try {
      ttl = static_cast<double>(ttlSlice.getInt());
    } catch (...) {
    }
  } else {
    try {
      ttl = ttlSlice.getDouble();
    } catch (...) {
    }
  }

  if (col->getStatusLocked() != TRI_VOC_COL_STATUS_LOADED) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  auto trxContext = StandaloneTransactionContext::Create(_vocbase);
  SingleCollectionTransaction trx(trxContext, col->cid(), TRI_TRANSACTION_READ);
  trx.addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);
  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_TRANSACTION_INTERNAL,
                  "cannot begin read transaction");
    return;
  }

  {
    CONDITION_LOCKER(locker, _condVar);
    _holdReadLockJobs.insert(id);
  }

  double now = TRI_microtime();
  double startTime = now;
  double endTime = startTime + ttl;

  {
    CONDITION_LOCKER(locker, _condVar);
    while (now < endTime) {
      _condVar.wait(100000);
      auto it = _holdReadLockJobs.find(id);
      if (it == _holdReadLockJobs.end()) {
        break;
      }
      now = TRI_microtime();
    }
    auto it = _holdReadLockJobs.find(id);
    if (it != _holdReadLockJobs.end()) {
      _holdReadLockJobs.erase(it);
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCheckHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'id' needs to be a string");
    return;
  }
  std::string id = idSlice.copyString();

  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it == _holdReadLockJobs.end()) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                    "no hold read lock job found for 'id'");
      return;
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCancelHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'id' needs to be a string");
    return;
  }
  std::string id = idSlice.copyString();

  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it != _holdReadLockJobs.end()) {
      _holdReadLockJobs.erase(it);
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetIdForReadLockCollection() {
  std::string id = std::to_string(TRI_NewTickServer());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("id", VPackValue(id));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief condition locker to wake up holdReadLockCollection jobs
//////////////////////////////////////////////////////////////////////////////

arangodb::basics::ConditionVariable RestReplicationHandler::_condVar;

//////////////////////////////////////////////////////////////////////////////
/// @brief global table of flags to cancel holdReadLockCollection jobs, if
/// the flag is set of the ID of a job, the job is cancelled
//////////////////////////////////////////////////////////////////////////////

std::unordered_set<std::string> RestReplicationHandler::_holdReadLockJobs;
