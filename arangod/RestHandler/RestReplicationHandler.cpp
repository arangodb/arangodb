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
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "HttpServer/HttpServer.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeys.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/TransactionContext.h"
#include "VocBase/compactor.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/server.h"
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

RestReplicationHandler::RestReplicationHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}

RestReplicationHandler::~RestReplicationHandler() {}

HttpHandler::status_t RestReplicationHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffix = _request->suffix();

  size_t const len = suffix.size();

  if (len >= 1) {
    std::string const& command = suffix[0];

    if (command == "logger-state") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == "logger-tick-ranges") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerTickRanges();
    } else if (command == "logger-first-tick") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerFirstTick();
    } else if (command == "logger-follow") {
      if (type != GeneralRequest::RequestType::GET &&
          type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerFollow();
    } else if (command == "determine-open-transactions") {
      if (type != GeneralRequest::RequestType::GET) {
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
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandBarrier();
    } else if (command == "inventory") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      if (type != GeneralRequest::RequestType::GET &&
          type != GeneralRequest::RequestType::POST &&
          type != GeneralRequest::RequestType::PUT &&
          type != GeneralRequest::RequestType::DELETE_REQ) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      if (type == GeneralRequest::RequestType::POST) {
        handleCommandCreateKeys();
      } else if (type == GeneralRequest::RequestType::GET) {
        handleCommandGetKeys();
      } else if (type == GeneralRequest::RequestType::PUT) {
        handleCommandFetchKeys();
      } else if (type == GeneralRequest::RequestType::DELETE_REQ) {
        handleCommandRemoveKeys();
      }
    } else if (command == "dump") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == "restore-collection") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == "restore-indexes") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == "restore-data") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleCommandRestoreDataCoordinator();
      } else {
        handleCommandRestoreData();
      }
    } else if (command == "sync") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandSync();
    } else if (command == "make-slave") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandMakeSlave();
    } else if (command == "server-id") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == "applier-config") {
      if (type == GeneralRequest::RequestType::GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != GeneralRequest::RequestType::PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == "applier-start") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandApplierStart();
    } else if (command == "applier-stop") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandApplierStop();
    } else if (command == "applier-state") {
      if (type == GeneralRequest::RequestType::DELETE_REQ) {
        handleCommandApplierDeleteState();
      } else {
        if (type != GeneralRequest::RequestType::GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == "clusterInventory") {
      if (type != GeneralRequest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(GeneralResponse::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else if (command == "addFollower") {
      if (type != GeneralRequest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(GeneralResponse::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandAddFollower();
      }
    } else if (command == "holdReadLockCollection") {
      if (!ServerState::instance()->isDBServer()) {
        generateError(GeneralResponse::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        if (type == GeneralRequest::RequestType::POST) {
          handleCommandHoldReadLockCollection();
        } else if (type == GeneralRequest::RequestType::PUT) {
          handleCommandCheckHoldReadLockCollection();
        } else if (type == GeneralRequest::RequestType::DELETE_REQ) {
          handleCommandCancelHoldReadLockCollection();
        } else if (type == GeneralRequest::RequestType::GET) {
          handleCommandGetIdForReadLockCollection();
        } else {
          goto BAD_CALL;
        }
      }
    } else {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER, "invalid command");
    }

    return status_t(HttpHandler::HANDLER_DONE);
  }

BAD_CALL:
  if (len != 1) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  } else {
    generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return status_t(HttpHandler::HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges, this is
/// because edges depend on vertices being there), then name
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::sortCollections(TRI_vocbase_col_t const* l,
                                             TRI_vocbase_col_t const* r) {
  if (l->_type != r->_type) {
    return l->_type < r->_type;
  }
  std::string const leftName = l->name();
  std::string const rightName = r->name();

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter a collection based on collection attributes
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::filterCollection(TRI_vocbase_col_t* collection,
                                              void* data) {
  if (collection->_type != (TRI_col_type_t)TRI_COL_TYPE_DOCUMENT &&
      collection->_type != (TRI_col_type_t)TRI_COL_TYPE_EDGE) {
    // invalid type
    return false;
  }

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
  if (_vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(GeneralResponse::ResponseCode::NOT_IMPLEMENTED,
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
  try {
    VPackBuilder builder;
    builder.add(VPackValue(VPackValueType::Object));  // Base

    arangodb::wal::LogfileManager::instance()->waitForSync(10.0);
    arangodb::wal::LogfileManagerState const s =
        arangodb::wal::LogfileManager::instance()->state();

    // "state" part
    builder.add("state", VPackValue(VPackValueType::Object));
    builder.add("running", VPackValue(true));
    builder.add("lastLogTick", VPackValue(std::to_string(s.lastCommittedTick)));
    builder.add("lastUncommittedLogTick", VPackValue(std::to_string(s.lastAssignedTick)));
    builder.add("totalEvents", VPackValue(s.numEvents + s.numEventsSync));
    builder.add("time", VPackValue(s.timeString));
    builder.close();

    // "server" part
    builder.add("server", VPackValue(VPackValueType::Object));
    builder.add("version", VPackValue(ARANGODB_VERSION));
    builder.add("serverId", VPackValue(std::to_string(TRI_GetIdServer())));
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

      builder.add("lastServedTick", VPackValue(std::to_string(std::get<2>(it))));

      builder.close();
    }
    builder.close();  // clients

    builder.close();  // base

    generateResult(GeneralResponse::ResponseCode::OK, builder.slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_tick_ranges
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerTickRanges() {
  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();
  try {
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
    generateResult(GeneralResponse::ResponseCode::OK, b.slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_first_tick
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFirstTick() {
  auto const& ranges = arangodb::wal::LogfileManager::instance()->ranges();

  try {
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
    generateResult(GeneralResponse::ResponseCode::OK, b.slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
  }
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

  if (type == GeneralRequest::RequestType::POST) {
    // create a new blocker
    std::shared_ptr<VPackBuilder> input = _request->toVelocyPack(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t id;
    int res = TRI_InsertBlockerCompactorVocBase(_vocbase, expires, &id);

    if (res != TRI_ERROR_NO_ERROR) {
      generateError(GeneralResponse::responseCode(res), res);
      return;
    }

    try {
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Object));
      b.add("id", VPackValue(std::to_string(id)));
      b.close();
      generateResult(GeneralResponse::ResponseCode::OK, b.slice());
    } catch (...) {
      generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  if (type == GeneralRequest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffix[1]));

    std::shared_ptr<VPackBuilder> input = _request->toVelocyPack(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    // now extend the blocker
    int res = TRI_TouchBlockerCompactorVocBase(_vocbase, id, expires);

    if (res == TRI_ERROR_NO_ERROR) {
      createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == GeneralRequest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffix[1]));

    int res = TRI_RemoveBlockerCompactorVocBase(_vocbase, id);

    if (res == TRI_ERROR_NO_ERROR) {
      createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED,
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

  if (type == GeneralRequest::RequestType::POST) {
    // create a new barrier

    std::shared_ptr<VPackBuilder> input = _request->toVelocyPack(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t minTick = 0;
    VPackSlice const v = input->slice().get("tick");

    if (v.isString()) {
      minTick = StringUtils::uint64(v.copyString());
    } else if (v.isNumber()) {
      minTick = v.getNumber<TRI_voc_tick_t>();
    }

    if (minTick == 0) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER, "invalid tick value");
      return;
    }

    TRI_voc_tick_t id =
        arangodb::wal::LogfileManager::instance()->addLogfileBarrier(minTick,
                                                                     ttl);

    try {
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Object));
      std::string const idString(std::to_string(id));
      b.add("id", VPackValue(idString));
      b.close();
      generateResult(GeneralResponse::ResponseCode::OK, b.slice());
    } catch (...) {
      generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  if (type == GeneralRequest::RequestType::PUT && len >= 2) {
    // extend an existing barrier
    TRI_voc_tick_t id = StringUtils::uint64(suffix[1]);

    std::shared_ptr<VPackBuilder> input = _request->toVelocyPack(&VPackOptions::Defaults);

    if (input == nullptr || !input->slice().isObject()) {
      generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    TRI_voc_tick_t minTick = 0;
    VPackSlice const v = input->slice().get("tick");

    if (v.isString()) {
      minTick = StringUtils::uint64(v.copyString());
    } else if (v.isNumber()) {
      minTick = v.getNumber<TRI_voc_tick_t>();
    }

    if (arangodb::wal::LogfileManager::instance()->extendLogfileBarrier(
            id, ttl, minTick)) {
      createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == GeneralRequest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing barrier
    TRI_voc_tick_t id = StringUtils::uint64(suffix[1]);

    if (arangodb::wal::LogfileManager::instance()->removeLogfileBarrier(id)) {
      createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == GeneralRequest::RequestType::GET) {
    // fetch all barriers
    auto ids = arangodb::wal::LogfileManager::instance()->getLogfileBarriers();

    try {
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Array));
      for (auto& it : ids) {
        b.add(VPackValue(std::to_string(it)));
      }
      b.close();
      generateResult(GeneralResponse::ResponseCode::OK, b.slice());
    } catch (...) {
      generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator() {
  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");

  if (DBserver.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "need \"DBserver\" parameter");
    return;
  }

  std::string const& dbname = _request->databaseName();

  auto headers = std::make_shared<std::unordered_map<std::string, std::string>>(
      arangodb::getForwardableRequestHeaders(_request));
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

  // Send a synchronous request to that shard using ClusterComm:
  auto res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                             _request->requestType(),
                             "/_db/" + StringUtils::urlEncode(dbname) +
                                 _request->requestPath() + params,
                             _request->body(), *headers, 300.0);

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    // there is no result
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_CLUSTER_CONNECTION_LOST,
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
  createResponse(static_cast<GeneralResponse::ResponseCode>(
      res->result->getHttpReturnCode()));
  _response->setContentType(res->result->getHeaderField(StaticStrings::ContentTypeHeader, dummy));
  _response->body().swap(&(res->result->getBody()));

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_returns
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFollow() {
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid from/to values");
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

  if (_request->requestType() == arangodb::GeneralRequest::RequestType::PUT) {
    std::string const& value5 = _request->value("firstRegularTick", found);

    if (found) {
      firstRegularTick =
          static_cast<TRI_voc_tick_t>(StringUtils::uint64(value5));
    }
    // copy default options
    VPackOptions options = VPackOptions::Defaults;
    options.checkAttributeUniqueness = true;
    std::shared_ptr<VPackBuilder> parsedRequest;
    try {
      parsedRequest = _request->toVelocyPack(&options);
    } catch (...) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }
    VPackSlice const slice = parsedRequest->slice();
    if (!slice.isArray()) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }

    for (auto const& id : VPackArrayIterator(slice)) {
      if (!id.isString()) {
        generateError(GeneralResponse::ResponseCode::BAD,
                      TRI_ERROR_HTTP_BAD_PARAMETER,
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
    TRI_vocbase_col_t* c =
        TRI_LookupCollectionByNameVocBase(_vocbase, value6);

    if (c == nullptr) {
      generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
      return;
    }

    cid = c->_cid;
  }

  if (barrierId > 0) {
    // extend the WAL logfile barrier
    arangodb::wal::LogfileManager::instance()->extendLogfileBarrier(
        barrierId, 180, tickStart);
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto transactionContext = std::make_shared<StandaloneTransactionContext>(_vocbase);

    // initialize the dump container
    TRI_replication_dump_t dump(transactionContext, static_cast<size_t>(determineChunkSize()),
                                includeSystem, cid);

    // and dump
    res = TRI_DumpLogReplication(&dump, transactionIds, firstRegularTick,
                                 tickStart, tickEnd, false);

    if (res == TRI_ERROR_NO_ERROR) {
      bool const checkMore = (dump._lastFoundTick > 0 &&
                              dump._lastFoundTick != state.lastCommittedTick);

      // generate the result
      size_t const length = TRI_LengthStringBuffer(dump._buffer);

      if (length == 0) {
        createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
      } else {
        createResponse(GeneralResponse::ResponseCode::OK);
      }

      _response->setContentType(HttpResponse::CONTENT_TYPE_DUMP);

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
        // transfer ownership of the buffer contents
        _response->body().set(dump._buffer);

        // to avoid double freeing
        TRI_StealStringBuffer(dump._buffer);
      }

      insertClient(dump._lastFoundTick);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid from/to values");
    return;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto transactionContext = std::make_shared<StandaloneTransactionContext>(_vocbase);

    // initialize the dump container
    TRI_replication_dump_t dump(transactionContext, static_cast<size_t>(determineChunkSize()), false,
                                0);

    // and dump
    res = TRI_DetermineOpenTransactionsReplication(&dump, tickStart, tickEnd);

    if (res == TRI_ERROR_NO_ERROR) {
      // generate the result
      size_t const length = TRI_LengthStringBuffer(dump._buffer);

      if (length == 0) {
        createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
      } else {
        createResponse(GeneralResponse::ResponseCode::OK);
      }

      _response->setContentType(HttpResponse::CONTENT_TYPE_DUMP);

      _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                             dump._fromTickIncluded ? "true" : "false");

      _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                             StringUtils::itoa(dump._lastFoundTick));

      if (length > 0) {
        // transfer ownership of the buffer contents
        _response->body().set(dump._buffer);

        // to avoid double freeing
        TRI_StealStringBuffer(dump._buffer);
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
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
  try {
    collectionsBuilder = TRI_InventoryCollectionsVocBase(
        _vocbase, tick, &filterCollection, (void*)&includeSystem, true,
        RestReplicationHandler::sortCollections);
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
    builder.add("lastUncommittedLogTick", VPackValue(std::to_string(s.lastAssignedTick)));
    builder.add("totalEvents", VPackValue(s.numEvents + s.numEventsSync));
    builder.add("time", VPackValue(s.timeString));
    builder.close();  // state

    std::string const tickString(std::to_string(tick));
    builder.add("tick", VPackValue(tickString));
    builder.close();  // Toplevel

    generateResult(GeneralResponse::ResponseCode::OK, builder.slice());
  } catch (std::bad_alloc&) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
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

  {
    std::string prefix("Plan/Collections/");
    prefix.append(dbName);

    AgencyCommLocker locker("Plan", "READ");
    if (!locker.successful()) {
      generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN);
    } else {
      result = _agency.getValues(prefix);
      if (!result.successful()) {
        generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                      TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
      } else {
        VPackSlice colls = result.slice()[0].get(std::vector<std::string>(
              {_agency.prefix(), "Plan", "Collections", dbName}));
        if (!colls.isObject()) {
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
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
          generateResult(GeneralResponse::ResponseCode::OK,
                         resultBuilder.slice());
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from VelocyPack TODO: MOVE
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t RestReplicationHandler::getCid(VPackSlice const& slice) const {
  if (!slice.isObject()) {
    return 0;
  }
  VPackSlice const id = slice.get("cid");
  if (id.isString()) {
    return StringUtils::uint64(id.copyString());
  } else if (id.isNumber()) {
    return static_cast<TRI_voc_cid_t>(id.getNumericValue<uint64_t>());
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided TODO: MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::createCollection(VPackSlice const& slice,
                                             TRI_vocbase_col_t** dst,
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
    cid = getCid(slice);

    if (cid == 0) {
      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          slice, "type", (int)TRI_COL_TYPE_DOCUMENT));

  TRI_vocbase_col_t* col = nullptr;

  if (cid > 0) {
    col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
  }

  if (col != nullptr && (TRI_col_type_t)col->_type == (TRI_col_type_t)type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  VocbaseCollectionInfo params(_vocbase, name.c_str(), slice);
  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(params.doCompact() ==
             arangodb::basics::VelocyPackHelper::getBooleanValue(
                 slice, "doCompact", true));
  TRI_ASSERT(params.waitForSync() ==
             arangodb::basics::VelocyPackHelper::getBooleanValue(
                 slice, "waitForSync", _vocbase->_settings.defaultWaitForSync));
  TRI_ASSERT(params.isVolatile() ==
             arangodb::basics::VelocyPackHelper::getBooleanValue(
                 slice, "isVolatile", false));
  TRI_ASSERT(params.isSystem() == (name[0] == '_'));
  TRI_ASSERT(params.indexBuckets() ==
             arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
                 slice, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS));
  TRI_voc_cid_t planId = 0;
  VPackSlice const planIdSlice = slice.get("planId");
  if (planIdSlice.isNumber()) {
    planId =
        static_cast<TRI_voc_cid_t>(planIdSlice.getNumericValue<uint64_t>());
  } else if (planIdSlice.isString()) {
    std::string tmp = planIdSlice.copyString();
    planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
  }

  if (planId > 0) {
    TRI_ASSERT(params.planId() == planId);
  } else {
    TRI_ASSERT(params.planId() == 0);
  }

  col = TRI_CreateCollectionVocBase(_vocbase, params, cid, true);

  if (col == nullptr) {
    return TRI_errno();
  }

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
    parsedRequest = _request->toVelocyPack(&options);
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid JSON");
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

  std::string errorMsg;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreCollectionCoordinator(slice, overwrite, recycleIds,
                                              force, numberOfShards, errorMsg);
  } else {
    res =
        processRestoreCollection(slice, overwrite, recycleIds, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
  } else {
    try {
      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("result", VPackValue(true));
      result.close();
      generateResult(GeneralResponse::ResponseCode::OK, result.slice());
    } catch (...) {
      generateOOMError();
    }
  }
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
    parsedRequest = _request->toVelocyPack(&options);
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid JSON");
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
    generateError(GeneralResponse::responseCode(res), res);
  } else {
    try {
      VPackBuilder result;
      result.openObject();
      result.add("result", VPackValue(true));
      result.close();
      generateResult(GeneralResponse::ResponseCode::OK, result.slice());
    } catch (...) {
      generateOOMError();
    }
  }
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

  TRI_vocbase_col_t* col = nullptr;

  if (reuseId) {
    VPackSlice const idString = parameters.get("cid");

    if (!idString.isString()) {
      errorMsg = "collection id is missing";

      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }

    std::string tmp = idString.copyString();

    TRI_voc_cid_t cid = StringUtils::uint64(tmp.c_str(), tmp.length());

    // first look up the collection by the cid
    col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
  }

  if (col == nullptr) {
    // not found, try name next
    col = TRI_LookupCollectionByNameVocBase(_vocbase, name);
  }

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      int res = TRI_DropCollectionVocBase(_vocbase, col, true);

      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->_cid, TRI_TRANSACTION_WRITE);
        trx.addHint(TRI_TRANSACTION_HINT_RECOVERY, false); // to turn off waitForSync!

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
    uint64_t numberOfShards, std::string& errorMsg) {
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

  std::string dbName = _vocbase->_name;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  // drop an existing collection if it exists
  if (!col->empty()) {
    if (dropExisting) {
      int res = ci->dropCollectionCoordinator(dbName, col->id_as_string(),
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
  }

  // now re-create the collection
  // dig out number of shards:
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

  TRI_ASSERT(numberOfShards > 0);

  try {
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
    if (!shards.isObject()) {
      // if no shards were given, create a random list of shards
      auto dbServers = ci->getCurrentDBServers();

      if (dbServers.empty()) {
        errorMsg = "no database servers found in cluster";
        return TRI_ERROR_INTERNAL;
      }

      std::random_shuffle(dbServers.begin(), dbServers.end());

      uint64_t const id = ci->uniqid(1 + numberOfShards);
      toMerge.add("shards", VPackValue(VPackValueType::Object));
      for (uint64_t i = 0; i < numberOfShards; ++i) {
        // shard id
        toMerge.add(
            VPackValue(std::string("s" + StringUtils::itoa(id + 1 + i))));

        // server ids
        toMerge.add(VPackValue(VPackValueType::Array));
        toMerge.add(VPackValue(dbServers[i % dbServers.size()]));
        toMerge.close();  // server ids
      }
      toMerge.close();  // end of shards
    }

    // Now put in the primary and an edge index if needed:
    toMerge.add("indexes", VPackValue(VPackValueType::Array));

    // create a dummy primary index
    {
      TRI_document_collection_t* doc = nullptr;
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

      return res;
    }
  } catch (std::bad_alloc const&) {
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
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

    TRI_document_collection_t* document = guard.collection()->_collection;

    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), document->_info.id(), TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "unable to start transaction: " + std::string(TRI_errno_string(res));
      THROW_ARANGO_EXCEPTION(res);
    }

    for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
      arangodb::Index* idx = nullptr;

      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}

      res = TRI_FromVelocyPackIndexDocumentCollection(&trx, document, idxDef,
                                                      &idx);

      if (res == TRI_ERROR_NOT_IMPLEMENTED) {
        continue;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg =
            "could not create index: " + std::string(TRI_errno_string(res));
        break;
      } else {
        TRI_ASSERT(idx != nullptr);

        res = TRI_SaveIndex(document, idx, true);

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

  std::string dbName = _vocbase->_name;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  if (col->empty()) {
    errorMsg = "could not find collection '" + name + "'";
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int res = TRI_ERROR_NO_ERROR;
  for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
    VPackBuilder tmp;
    res = ci->ensureIndexCoordinator(dbName, col->id_as_string(), idxDef, true,
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
                             std::string& rev,
                             VPackBuilder& builder, VPackSlice& doc,
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
        }
        else if (useRevision && doc.hasKey(StaticStrings::RevString)) {
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
  std::string const invalidMsg = "received invalid JSON data for collection " +
                                 collectionName;

  VPackBuilder builder;
  VPackBuilder oldBuilder;

  std::string const& bodyStr = _request->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

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

      oldBuilder.clear();
      oldBuilder.openObject();
      oldBuilder.add(StaticStrings::KeyString, VPackValue(key));
      oldBuilder.close();

      res = applyCollectionDumpMarker(trx, resolver, collectionName, type,
                                      oldBuilder.slice(), doc, errorMsg);

      if (res != TRI_ERROR_NO_ERROR && !force) {
        return res;
      }
    }

    ptr = pos + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreData(
    CollectionNameResolver const& resolver, TRI_voc_cid_t cid, bool useRevision,
    bool force, std::string& errorMsg) {
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), cid, TRI_TRANSACTION_WRITE);
  trx.addHint(TRI_TRANSACTION_HINT_RECOVERY, false); // to turn off waitForSync!

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to start transaction: " + std::string(TRI_errno_string(res));

    return res;
  }

  res = processRestoreDataBatch(trx, resolver, trx.name(), useRevision,
                                force, errorMsg);

  res = trx.finish(res);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData() {
  std::string const& value1 = _request->value("collection");

  if (value1.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  CollectionNameResolver resolver(_vocbase);

  TRI_voc_cid_t cid = resolver.getCollectionIdLocal(value1);

  if (cid == 0) {
    std::string msg = "invalid collection parameter: '";
    msg += value1;
    msg += "', cid is 0";
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, msg);
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
    try {
      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("result", VPackValue(true));
      result.close();
      generateResult(GeneralResponse::ResponseCode::OK, result.slice());
    } catch (...) {
      generateOOMError();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreDataCoordinator() {
  std::string const& name = _request->value("collection");

  if (name.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid collection parameter");
    return;
  }

  std::string dbName = _vocbase->_name;
  std::string errorMsg;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  if (col->empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
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

  std::string const& bodyStr = _request->body();
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
        res = ci->getResponsibleShard(col->id_as_string(), doc, true,
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
        auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
        size_t j = it->second;
        auto body = std::make_shared<std::string const>(bufs[j]->c_str(),
                                                        bufs[j]->length());
        cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                         arangodb::GeneralRequest::RequestType::PUT,
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
        if (result.answer_code == GeneralResponse::ResponseCode::OK ||
            result.answer_code == GeneralResponse::ResponseCode::CREATED) {
          // copy default options
          VPackOptions options = VPackOptions::Defaults;
          options.checkAttributeUniqueness = true;
          std::shared_ptr<VPackBuilder> parsedAnswer;
          try {
            parsedAnswer = result.answer->toVelocyPack(&options);
          } catch (VPackException const& e) {
            // Only log this error and try the next doc
            LOG(DEBUG) << "failed to parse json object: '" << e.what() << "'";
            continue;
          }

          VPackSlice const answer = parsedAnswer->slice();
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
        } else if (result.answer_code ==
                   GeneralResponse::ResponseCode::SERVER_ERROR) {
          // copy default options
          VPackOptions options = VPackOptions::Defaults;
          options.checkAttributeUniqueness = true;
          std::shared_ptr<VPackBuilder> parsedAnswer;
          try {
            parsedAnswer = result.answer->toVelocyPack(&options);
          } catch (VPackException const& e) {
            // Only log this error and try the next doc
            LOG(DEBUG) << "failed to parse json object: '" << e.what() << "'";
            continue;
          }

          VPackSlice const answer = parsedAnswer->slice();
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
    generateError(GeneralResponse::responseCode(res), res, errorMsg);
    return;
  }
  try {
    VPackBuilder result;
    result.openObject();
    result.add("result", VPackValue(true));
    result.close();
    generateResult(GeneralResponse::ResponseCode::OK, result.slice());
  } catch (...) {
    generateOOMError();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCreateKeys() {
  std::string const& collection = _request->value("collection");

  if (collection.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid collection parameter");
    return;
  }

  TRI_voc_tick_t tickEnd = UINT64_MAX;

  // determine end tick for keys
  bool found;
  std::string const& value = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  TRI_vocbase_col_t* c =
      TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == nullptr) {
    generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::CollectionGuard guard(_vocbase, c->_cid, false);

    TRI_vocbase_col_t* col = guard.collection();
    TRI_ASSERT(col != nullptr);

    // turn off the compaction for the collection
    TRI_voc_tick_t id;
    res = TRI_InsertBlockerCompactorVocBase(_vocbase, 1200.0, &id);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // initialize a container with the keys
    auto keys =
        std::make_unique<CollectionKeys>(_vocbase, col->_name, id, 300.0);

    std::string const idString(std::to_string(keys->id()));

    keys->create(tickEnd);
    size_t const count = keys->count();

    auto keysRepository = _vocbase->_collectionKeys;

    try {
      keysRepository->store(keys.get());
      keys.release();
    } catch (...) {
      throw;
    }

    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(idString));
    result.add("count", VPackValue(count));
    result.close();
    generateResult(GeneralResponse::ResponseCode::OK, result.slice());
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
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

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto keysRepository = _vocbase->_collectionKeys;
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

        auto result = collectionKeys->hashChunk(static_cast<size_t>(from), static_cast<size_t>(to));

        // Add a chunk
        b.add(VPackValue(VPackValueType::Object));
        b.add("low", VPackValue(std::get<0>(result)));
        b.add("high", VPackValue(std::get<1>(result)));
        b.add("hash", VPackValue(std::to_string(std::get<2>(result))));
        b.close();
      }
      b.close();

      collectionKeys->release();
      generateResult(GeneralResponse::ResponseCode::OK, b.slice());
    } catch (...) {
      collectionKeys->release();
      throw;
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (std::exception const&) {
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandFetchKeys() {
  std::vector<std::string> const& suffix = _request->suffix();
  
  if (suffix.size() != 2) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid 'type' value");
    return;
  }

  std::string const& id = suffix[1];

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto keysRepository = _vocbase->_collectionKeys;
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
      std::shared_ptr<TransactionContext> transactionContext = StandaloneTransactionContext::Create(_vocbase);

      VPackBuilder resultBuilder(transactionContext->getVPackOptions());
      resultBuilder.openArray();

      if (keys) {
        collectionKeys->dumpKeys(resultBuilder, chunk, static_cast<size_t>(chunkSize));
      } else {
        bool success;
        std::shared_ptr<VPackBuilder> parsedIds =
            parseVelocyPackBody(&VPackOptions::Defaults, success);
        if (!success) {
          // error already created
          collectionKeys->release();
          return;
        }
        collectionKeys->dumpDocs(resultBuilder, chunk, static_cast<size_t>(chunkSize), parsedIds->slice());
      }

      resultBuilder.close();

      collectionKeys->release();

      generateResult(GeneralResponse::ResponseCode::OK, resultBuilder.slice(), transactionContext);
      return;
    } catch (...) {
      collectionKeys->release();
      throw;
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (std::exception const&) {
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(res != TRI_ERROR_NO_ERROR);
  generateError(GeneralResponse::responseCode(res), res);
}

void RestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  std::string const& id = suffix[1];

  auto keys = _vocbase->_collectionKeys;
  TRI_ASSERT(keys != nullptr);

  auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = keys->remove(collectionKeysId);

  if (!found) {
    generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                  TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add("id", VPackValue(id)); // id as a string
  resultBuilder.add("error", VPackValue(false));
  resultBuilder.add("code", VPackValue(static_cast<int>(GeneralResponse::ResponseCode::ACCEPTED)));
  resultBuilder.close();

  generateResult(GeneralResponse::ResponseCode::ACCEPTED, resultBuilder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_dump
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump() {
  std::string const& collection = _request->value("collection");

  if (collection.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid collection parameter");
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "invalid from/to values");
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

  TRI_vocbase_col_t* c =
      TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == nullptr) {
    generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  LOG(TRACE) << "requested collection dump for collection '" << collection
             << "', tickStart: " << tickStart << ", tickEnd: " << tickEnd;

  int res = TRI_ERROR_NO_ERROR;

  try {
    if (flush) {
      arangodb::wal::LogfileManager::instance()->flush(true, true, false);

      // additionally wait for the collector
      if (flushWait > 0) {
        arangodb::wal::LogfileManager::instance()->waitForCollectorQueue(
            c->_cid, static_cast<double>(flushWait));
      }
    }

    arangodb::CollectionGuard guard(_vocbase, c->_cid, false);

    TRI_vocbase_col_t* col = guard.collection();
    TRI_ASSERT(col != nullptr);

    auto transactionContext = std::make_shared<StandaloneTransactionContext>(_vocbase);

    // initialize the dump container
    TRI_replication_dump_t dump(transactionContext, static_cast<size_t>(determineChunkSize()),
                                includeSystem, 0);
    
    if (compat28) {
      dump._compat28 = true;
    }

    res = TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd, withTicks);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // generate the result
    size_t const length = TRI_LengthStringBuffer(dump._buffer);

    if (length == 0) {
      createResponse(GeneralResponse::ResponseCode::NO_CONTENT);
    } else {
      createResponse(GeneralResponse::ResponseCode::OK);
    }

    _response->setContentType(HttpResponse::CONTENT_TYPE_DUMP);

    // set headers
    _response->setHeaderNC(
        TRI_REPLICATION_HEADER_CHECKMORE,
        ((dump._hasMore || dump._bufferFull) ? "true" : "false"));

    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                           StringUtils::itoa(dump._lastFoundTick));

    // transfer ownership of the buffer contents
    _response->body().set(dump._buffer);

    // avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
  }
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }

  std::string const database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->_name);
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  // forget about any existing replication applier configuration
  int res = _vocbase->_replicationApplier->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
    return;
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
    generateError(GeneralResponse::responseCode(res), res, errorMsg);
    return;
  }

  res = TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
    return;
  }

  res = _vocbase->_replicationApplier->start(lastLogTick, true, barrierId);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
    return;
  }

  try {
    std::shared_ptr<VPackBuilder> result =
        _vocbase->_replicationApplier->toVelocyPack();
    generateResult(GeneralResponse::ResponseCode::OK, result->slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }

  std::string const database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->_name);
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
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
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
        
  // wait until all data in current logfile got synced
  arangodb::wal::LogfileManager::instance()->waitForSync(5.0);

  InitialSyncer syncer(_vocbase, &config, restrictCollections, restrictType,
                       verbose);

  int res = TRI_ERROR_NO_ERROR;
  std::string errorMsg = "";

  try {
    res = syncer.run(errorMsg, incremental);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    errorMsg = ex.message();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    errorMsg = ex.what();
  } catch (...) {
    errorMsg = "caught unknown exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res, errorMsg);
    return;
  }

  try {
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
    generateResult(GeneralResponse::ResponseCode::OK, result.slice());
  } catch (arangodb::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.message());
  } catch (std::exception const& ex) {
    int res = TRI_ERROR_INTERNAL;
    generateError(HttpResponse::responseCode(res), res, ex.what());
  } catch (...) {
    int res = TRI_ERROR_INTERNAL;
    generateError(HttpResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_serverID
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandServerId() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    std::string const serverId = StringUtils::itoa(TRI_GetIdServer());
    result.add("serverId", VPackValue(serverId));
    result.close();
    generateResult(GeneralResponse::ResponseCode::OK, result.slice());
  } catch (...) {
    generateOOMError();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetConfig() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  TRI_replication_applier_configuration_t config;

  {
    READ_LOCKER(readLocker, _vocbase->_replicationApplier->_statusLock);
    config.update(&_vocbase->_replicationApplier->_configuration);
  }
  try {
    std::shared_ptr<VPackBuilder> configBuilder = config.toVelocyPack(false);
    generateResult(GeneralResponse::ResponseCode::OK, configBuilder->slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

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
    READ_LOCKER(readLocker, _vocbase->_replicationApplier->_statusLock);
    config.update(&_vocbase->_replicationApplier->_configuration);
  }

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (!endpoint.empty()) {
    config._endpoint = endpoint;
  }

  config._database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->_name);

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
      TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
    return;
  }

  handleCommandApplierGetConfig();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_start
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStart() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

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
      _vocbase->_replicationApplier->start(initialTick, useTick, barrierId);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION ||
        res == TRI_ERROR_REPLICATION_RUNNING) {
      generateError(GeneralResponse::ResponseCode::BAD, res);
    } else {
      generateError(GeneralResponse::ResponseCode::SERVER_ERROR, res);
    }
    return;
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_stop
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStop() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  int res = _vocbase->_replicationApplier->stop(true);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, res);
    return;
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetState() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  try {
    std::shared_ptr<VPackBuilder> result =
        _vocbase->_replicationApplier->toVelocyPack();
    generateResult(GeneralResponse::ResponseCode::OK, result->slice());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierDeleteState() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  int res = _vocbase->_replicationApplier->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, res);
    return;
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
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attributes 'followerId' and 'shard'");
    return;
  }
  VPackSlice const followerId = body.get("followerId");
  VPackSlice const shard = body.get("shard");
  if (!followerId.isString() || !shard.isString()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId' and 'shard' attributes must be strings");
    return;
  }

  auto col = TRI_LookupCollectionByNameVocBase(_vocbase, shard.copyString());
  if (col == nullptr) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  TRI_document_collection_t* docColl = col->_collection;
  if (docColl == nullptr) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  TRI_ASSERT(docColl != nullptr);
  docColl->followers()->add(followerId.copyString());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(GeneralResponse::ResponseCode::OK, b.slice());
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
    generateError(
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attributes 'collection', 'ttl' and 'id'");
    return;
  }
  VPackSlice const collection = body.get("collection");
  VPackSlice const ttlSlice = body.get("ttl");
  VPackSlice const idSlice = body.get("id");
  if (!collection.isString() || !ttlSlice.isNumber() || !idSlice.isString()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'collection' must be a string and 'ttl' a number and "
                  "'id' a string");
    return;
  }
  std::string id = idSlice.copyString();

  auto col = TRI_LookupCollectionByNameVocBase(_vocbase, collection.copyString());
  if (col == nullptr) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
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

  TRI_document_collection_t* docColl = col->_collection;
  if (docColl == nullptr) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  auto trxContext = StandaloneTransactionContext::Create(_vocbase);
  SingleCollectionTransaction trx(trxContext, col->_cid, TRI_TRANSACTION_READ);
  trx.addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);
  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
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

  generateResult(GeneralResponse::ResponseCode::OK, b.slice());
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
    generateError(
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'id' needs to be a string");
    return;
  }
  std::string id = idSlice.copyString();

  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it == _holdReadLockJobs.end()) {
      generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                    TRI_ERROR_HTTP_NOT_FOUND,
                    "no hold read lock job found for 'id'");
      return;
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(GeneralResponse::ResponseCode::OK, b.slice());
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
    generateError(
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
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

  generateResult(GeneralResponse::ResponseCode::OK, b.slice());
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

  generateResult(GeneralResponse::ResponseCode::OK, b.slice());
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
