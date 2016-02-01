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
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/JsonHelper.h"
#include "Basics/Logger.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterComm.h"
#include "HttpServer/HttpServer.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeys.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/transactions.h"
#include "VocBase/compactor.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/server.h"
#include "VocBase/update-policy.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
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
  HttpRequest::HttpRequestType const type = _request->requestType();

  std::vector<std::string> const& suffix = _request->suffix();

  size_t const len = suffix.size();

  if (len >= 1) {
    std::string const& command = suffix[0];

    if (command == "logger-state") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == "logger-tick-ranges") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerTickRanges();
    } else if (command == "logger-first-tick") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerFirstTick();
    } else if (command == "logger-follow") {
      if (type != HttpRequest::HTTP_REQUEST_GET &&
          type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }
      handleCommandLoggerFollow();
    } else if (command == "determine-open-transactions") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandDetermineOpenTransactions();
    } else if (command == "batch") {
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == "inventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      if (type != HttpRequest::HTTP_REQUEST_GET &&
          type != HttpRequest::HTTP_REQUEST_POST &&
          type != HttpRequest::HTTP_REQUEST_PUT &&
          type != HttpRequest::HTTP_REQUEST_DELETE) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      if (type == HttpRequest::HTTP_REQUEST_POST) {
        handleCommandCreateKeys();
      } else if (type == HttpRequest::HTTP_REQUEST_GET) {
        handleCommandGetKeys();
      } else if (type == HttpRequest::HTTP_REQUEST_PUT) {
        handleCommandFetchKeys();
      } else if (type == HttpRequest::HTTP_REQUEST_DELETE) {
        handleCommandRemoveKeys();
      }
    } else if (command == "dump") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == "restore-collection") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == "restore-indexes") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == "restore-data") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleCommandRestoreDataCoordinator();
      } else {
        handleCommandRestoreData();
      }
    } else if (command == "sync") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandSync();
    } else if (command == "make-slave") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandMakeSlave();
    } else if (command == "server-id") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == "applier-config") {
      if (type == HttpRequest::HTTP_REQUEST_GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != HttpRequest::HTTP_REQUEST_PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == "applier-start") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandApplierStart();
    } else if (command == "applier-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(HttpHandler::HANDLER_DONE);
      }

      handleCommandApplierStop();
    } else if (command == "applier-state") {
      if (type == HttpRequest::HTTP_REQUEST_DELETE) {
        handleCommandApplierDeleteState();
      } else {
        if (type != HttpRequest::HTTP_REQUEST_GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == "clusterInventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(HttpResponse::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }

    return status_t(HttpHandler::HANDLER_DONE);
  }

BAD_CALL:
  if (len != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  } else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,
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
    generateError(HttpResponse::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
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
  char const* value = _request->value("serverId", found);

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
  char const* value = _request->value("chunkSize", found);

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
    VPackBuilder json;
    json.add(VPackValue(VPackValueType::Object));  // Base

    arangodb::wal::LogfileManagerState const&& s =
        arangodb::wal::LogfileManager::instance()->state();
    std::string const lastTickString(StringUtils::itoa(s.lastTick));

    // "state" part
    json.add("state", VPackValue(VPackValueType::Object));
    json.add("running", VPackValue(true));
    json.add("lastLogTick", VPackValue(lastTickString));
    json.add("totalEvents", VPackValue(s.numEvents));
    json.add("time", VPackValue(s.timeString));
    json.close();

    // "server" part
    char* serverIdString = TRI_StringUInt64(TRI_GetIdServer());

    json.add("server", VPackValue(VPackValueType::Object));
    json.add("version", VPackValue(TRI_VERSION));
    json.add("serverId", VPackValue(serverIdString));
    TRI_FreeString(TRI_CORE_MEM_ZONE, serverIdString);
    json.close();

    // "clients" part
    json.add("clients", VPackValue(VPackValueType::Array));
    auto allClients = _vocbase->getReplicationClients();
    for (auto& it : allClients) {
      // One client
      json.add(VPackValue(VPackValueType::Object));
      serverIdString = TRI_StringUInt64(std::get<0>(it));
      json.add("serverId", VPackValue(serverIdString));
      TRI_FreeString(TRI_CORE_MEM_ZONE, serverIdString);

      char buffer[21];
      TRI_GetTimeStampReplication(std::get<1>(it), &buffer[0], sizeof(buffer));
      json.add("time", VPackValue(buffer));

      auto tickString = std::to_string(std::get<2>(it));
      json.add("lastServedTick", VPackValue(tickString));

      json.close();
    }
    json.close();  // clients

    json.close();  // base

    VPackSlice slice = json.slice();
    generateResult(slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
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

      char buffer[21];
      size_t len;
      len = TRI_StringUInt64InPlace(it.tickMin, (char*)&buffer);
      b.add("tickMin", VPackValue(std::string(&buffer[0], len)));

      len = TRI_StringUInt64InPlace(it.tickMax, (char*)&buffer);
      b.add("tickMax", VPackValue(std::string(&buffer[0], len)));

      b.close();
    }

    b.close();
    generateResult(b.slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
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
    generateResult(b.slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_batch_replication
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_batch_replication
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_delete_batch_replication
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandBatch() {
  // extract the request type
  HttpRequest::HttpRequestType const type = _request->requestType();
  std::vector<std::string> const& suffix = _request->suffix();
  size_t const len = suffix.size();

  TRI_ASSERT(len >= 1);

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    // create a new blocker

    std::unique_ptr<TRI_json_t> input(_request->toJson(nullptr));

    if (input == nullptr) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = JsonHelper::getNumericValue<double>(input.get(), "ttl", 0);

    TRI_voc_tick_t id;
    int res = TRI_InsertBlockerCompactorVocBase(_vocbase, expires, &id);

    if (res != TRI_ERROR_NO_ERROR) {
      generateError(HttpResponse::responseCode(res), res);
      return;
    }

    try {
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Object));
      std::string const idString(std::to_string(id));
      b.add("id", VPackValue(idString));
      b.close();
      VPackSlice s = b.slice();
      generateResult(s);
    } catch (...) {
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  if (type == HttpRequest::HTTP_REQUEST_PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id = (TRI_voc_tick_t)StringUtils::uint64(suffix[1]);

    std::unique_ptr<TRI_json_t> input(_request->toJson(nullptr));

    if (input == nullptr) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = JsonHelper::getNumericValue<double>(input.get(), "ttl", 0);

    // now extend the blocker
    int res = TRI_TouchBlockerCompactorVocBase(_vocbase, id, expires);

    if (res == TRI_ERROR_NO_ERROR) {
      createResponse(HttpResponse::NO_CONTENT);
    } else {
      generateError(HttpResponse::responseCode(res), res);
    }
    return;
  }

  if (type == HttpRequest::HTTP_REQUEST_DELETE && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id = (TRI_voc_tick_t)StringUtils::uint64(suffix[1]);

    int res = TRI_RemoveBlockerCompactorVocBase(_vocbase, id);

    if (res == TRI_ERROR_NO_ERROR) {
      createResponse(HttpResponse::NO_CONTENT);
    } else {
      generateError(HttpResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(HttpResponse::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator() {
  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");
  if (DBserver.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "need \"DBserver\" parameter");
    return;
  }

  std::string const& dbname = _request->databaseName();

  auto headers = std::make_shared<std::map<std::string, std::string>>(
      arangodb::getForwardableRequestHeaders(_request));
  std::map<std::string, std::string> values = _request->values();
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
  auto res = cc->syncRequest(
      "", TRI_NewTickServer(), "server:" + DBserver, _request->requestType(),
      "/_db/" + StringUtils::urlEncode(dbname) + _request->requestPath() +
          params,
      std::string(_request->body(), _request->bodySize()), *headers, 300.0);

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(HttpResponse::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || !res->result->isComplete()) {
      // there is no result
      generateError(HttpResponse::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                    "lost connection within cluster");
      return;
    }
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result.
    // We intentionally fall through here.
  }

  bool dummy;
  createResponse(static_cast<HttpResponse::HttpResponseCode>(
      res->result->getHttpReturnCode()));
  _response->setContentType(res->result->getHeaderField("content-type", dummy));
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
  arangodb::wal::LogfileManagerState state =
      arangodb::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = state.lastDataTick;
  TRI_voc_tick_t firstRegularTick = 0;

  bool found;
  char const* value;

  value = _request->value("from", found);
  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  // determine end tick for dump
  value = _request->value("to", found);
  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  bool includeSystem = true;
  value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  // grab list of transactions from the body value
  std::unordered_set<TRI_voc_tid_t> transactionIds;

  if (_request->requestType() ==
      arangodb::rest::HttpRequest::HTTP_REQUEST_PUT) {
    value = _request->value("firstRegularTick", found);
    if (found) {
      firstRegularTick =
          static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
    }
    VPackOptions options;
    options.checkAttributeUniqueness = true;
    std::shared_ptr<VPackBuilder> parsedRequest;
    try {
      parsedRequest = _request->toVelocyPack(&options);
    } catch (...) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }
    VPackSlice const slice = parsedRequest->slice();
    if (!slice.isArray()) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return;
    }

    for (auto const& id : VPackArrayIterator(slice)) {
      if (id.isString()) {
        generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "invalid body value. expecting array of ids");
        return;
      }
      transactionIds.emplace(StringUtils::uint64(id.copyString()));
    }
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    // initialize the dump container
    TRI_replication_dump_t dump(_vocbase, (size_t)determineChunkSize(),
                                includeSystem);

    // and dump
    res = TRI_DumpLogReplication(&dump, transactionIds, firstRegularTick,
                                 tickStart, tickEnd, false);

    if (res == TRI_ERROR_NO_ERROR) {
      bool const checkMore = (dump._lastFoundTick > 0 &&
                              dump._lastFoundTick != state.lastDataTick);

      // generate the result
      size_t const length = TRI_LengthStringBuffer(dump._buffer);

      if (length == 0) {
        createResponse(HttpResponse::NO_CONTENT);
      } else {
        createResponse(HttpResponse::OK);
      }

      _response->setContentType("application/x-arango-dump; charset=utf-8");

      // set headers
      _response->setHeader(TRI_REPLICATION_HEADER_CHECKMORE,
                           strlen(TRI_REPLICATION_HEADER_CHECKMORE),
                           checkMore ? "true" : "false");

      _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED,
                           strlen(TRI_REPLICATION_HEADER_LASTINCLUDED),
                           StringUtils::itoa(dump._lastFoundTick));

      _response->setHeader(TRI_REPLICATION_HEADER_LASTTICK,
                           strlen(TRI_REPLICATION_HEADER_LASTTICK),
                           StringUtils::itoa(state.lastTick));

      _response->setHeader(TRI_REPLICATION_HEADER_ACTIVE,
                           strlen(TRI_REPLICATION_HEADER_ACTIVE), "true");

      _response->setHeader(TRI_REPLICATION_HEADER_FROMPRESENT,
                           strlen(TRI_REPLICATION_HEADER_FROMPRESENT),
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
    generateError(HttpResponse::responseCode(res), res);
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
  arangodb::wal::LogfileManagerState state =
      arangodb::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = state.lastDataTick;

  bool found;
  char const* value;

  value = _request->value("from", found);
  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  // determine end tick for dump
  value = _request->value("to", found);
  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    // initialize the dump container
    TRI_replication_dump_t dump(_vocbase, (size_t)determineChunkSize(), false);

    // and dump
    res = TRI_DetermineOpenTransactionsReplication(&dump, tickStart, tickEnd);

    if (res == TRI_ERROR_NO_ERROR) {
      // generate the result
      size_t const length = TRI_LengthStringBuffer(dump._buffer);

      if (length == 0) {
        createResponse(HttpResponse::NO_CONTENT);
      } else {
        createResponse(HttpResponse::OK);
      }

      _response->setContentType("application/x-arango-dump; charset=utf-8");

      _response->setHeader(
          TRI_CHAR_LENGTH_PAIR(TRI_REPLICATION_HEADER_FROMPRESENT),
          dump._fromTickIncluded ? "true" : "false");
      _response->setHeader(
          TRI_CHAR_LENGTH_PAIR(TRI_REPLICATION_HEADER_LASTTICK),
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
    generateError(HttpResponse::responseCode(res), res);
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
  char const* value = _request->value("includeSystem", found);

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

    arangodb::wal::LogfileManagerState const&& s =
        arangodb::wal::LogfileManager::instance()->state();

    builder.add("running", VPackValue(true));
    auto logTickString = std::to_string(s.lastTick);
    builder.add("lastLogTick", VPackValue(logTickString));

    builder.add("totalEvents", VPackValue(s.numEvents));
    builder.add("time", VPackValue(s.timeString));
    builder.close();  // state

    std::string const tickString(std::to_string(tick));
    builder.add("tick", VPackValue(tickString));
    builder.close();  // Toplevel

    generateResult(builder.slice());
  } catch (std::bad_alloc&) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandClusterInventory() {
  std::string const& dbName = _request->databaseName();
  std::string value;
  bool found;
  bool includeSystem = true;

  value = _request->value("includeSystem", found);
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
      generateError(HttpResponse::SERVER_ERROR,
                    TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN);
    } else {
      result = _agency.getValues(prefix, false);
      if (!result.successful()) {
        generateError(HttpResponse::SERVER_ERROR,
                      TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
      } else {
        if (!result.parse(prefix + "/", false)) {
          generateError(HttpResponse::SERVER_ERROR,
                        TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
        } else {
          VPackBuilder resultBuilder;
          {
            VPackObjectBuilder b1(&resultBuilder);
            resultBuilder.add(VPackValue("collections"));
            {
              VPackArrayBuilder b2(&resultBuilder);
              for (auto const& it : result._values) {
                VPackSlice const subResultSlice = it.second._vpack->slice();
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
          generateResult(HttpResponse::OK, resultBuilder.slice());
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

  const TRI_col_type_e type = static_cast<TRI_col_type_e>(
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
    planId = static_cast<TRI_voc_cid_t>(
        TRI_UInt64String2(tmp.c_str(), tmp.length()));
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

  VPackOptions options;
  options.checkAttributeUniqueness = true;

  try {
    parsedRequest = _request->toVelocyPack(&options);
  } catch (...) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool found;
  char const* value;

  bool overwrite = false;
  value = _request->value("overwrite", found);

  if (found) {
    overwrite = StringUtils::boolean(value);
  }

  bool recycleIds = false;
  value = _request->value("recycleIds", found);
  if (found) {
    recycleIds = StringUtils::boolean(value);
  }

  bool force = false;
  value = _request->value("force", found);
  if (found) {
    force = StringUtils::boolean(value);
  }

  uint64_t numberOfShards = 0;
  value = _request->value("numberOfShards", found);
  if (found) {
    numberOfShards = StringUtils::uint64(value);
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
    generateError(HttpResponse::responseCode(res), res);
  } else {
    try {
      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("result", VPackValue(true));
      result.close();
      VPackSlice s = result.slice();
      generateResult(s);
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

  VPackOptions options;
  options.checkAttributeUniqueness = true;

  try {
    parsedRequest = _request->toVelocyPack(&options);
  } catch (...) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool found;
  bool force = false;
  char const* value = _request->value("force", found);
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
    generateError(HttpResponse::responseCode(res), res);
  } else {
    try {
      VPackBuilder result;
      result.openObject();
      result.add("result", VPackValue(true));
      result.close();
      VPackSlice s = result.slice();
      generateResult(s);
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
    col = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
  }

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      int res = TRI_DropCollectionVocBase(_vocbase, col, true);

      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        SingleCollectionWriteTransaction<UINT64_MAX> trx(
            new StandaloneTransactionContext(), _vocbase, col->_cid);

        res = trx.begin();
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = trx.truncate(false);
        res = trx.finish(res);

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
      toMerge.add(VPackValue(std::string(TRI_VOC_ATTRIBUTE_KEY)));
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

    SingleCollectionWriteTransaction<UINT64_MAX> trx(
        new StandaloneTransactionContext(), _vocbase, document->_info.id());

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

  size_t const n = indexes.length();

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
                                     arangodb::Index::Compare, tmp,
                                     errorMsg, 3600.0);
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
    arangodb::Transaction* trx, CollectionNameResolver const& resolver,
    TRI_transaction_collection_t* trxCollection,
    TRI_replication_operation_e type, const TRI_voc_key_t key,
    const TRI_voc_rid_t rid, VPackSlice const& slice, std::string& errorMsg) {
  if (type == REPLICATION_MARKER_DOCUMENT || type == REPLICATION_MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_ASSERT(!slice.isNone());
    TRI_ASSERT(slice.isObject());

    TRI_document_collection_t* document =
        trxCollection->_collection->_collection;
    TRI_memory_zone_t* zone =
        document->getShaper()
            ->memoryZone();  // PROTECTED by trx in trxCollection
    TRI_shaped_json_t* shaped =
        TRI_ShapedJsonVelocyPack(document->getShaper(), slice,
                                 true);  // PROTECTED by trx in trxCollection

    if (shaped == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    try {
      TRI_doc_mptr_copy_t mptr;

      int res = TRI_ReadShapedJsonDocumentCollection(trx, trxCollection, key,
                                                     &mptr, false);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert

        if (type == REPLICATION_MARKER_EDGE) {
          // edge
          if (document->_info.type() != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
            errorMsg = "expecting edge collection, got document collection";
          } else {
            res = TRI_ERROR_NO_ERROR;

            std::string const from =
                arangodb::basics::VelocyPackHelper::getStringValue(
                    slice, TRI_VOC_ATTRIBUTE_FROM, "");
            std::string const to =
                arangodb::basics::VelocyPackHelper::getStringValue(
                    slice, TRI_VOC_ATTRIBUTE_TO, "");

            // parse _from
            TRI_document_edge_t edge;
            if (!DocumentHelper::parseDocumentId(
                    resolver, from.c_str(), edge._fromCid, &edge._fromKey)) {
              res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              errorMsg = std::string("handle bad or collection unknown '") +
                         from.c_str() + "'";
            }

            // parse _to
            if (!DocumentHelper::parseDocumentId(resolver, to.c_str(),
                                                 edge._toCid, &edge._toKey)) {
              res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              errorMsg = std::string("handle bad or collection unknown '") +
                         to.c_str() + "'";
            }

            if (res == TRI_ERROR_NO_ERROR) {
              res = TRI_InsertShapedJsonDocumentCollection(
                  trx, trxCollection, key, rid, nullptr, &mptr, shaped, &edge,
                  false, false, true);
            }
          }
        } else {
          // document
          if (document->_info.type() != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
            errorMsg = std::string(TRI_errno_string(res)) +
                       ": expecting document collection, got edge collection";
          } else {
            res = TRI_InsertShapedJsonDocumentCollection(
                trx, trxCollection, key, rid, nullptr, &mptr, shaped, nullptr,
                false, false, true);
          }
        }
      } else {
        // update

        // init the update policy
        TRI_doc_update_policy_t policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr);
        res = TRI_UpdateShapedJsonDocumentCollection(
            trx, trxCollection, key, rid, nullptr, &mptr, shaped, &policy,
            false, false);
      }

      TRI_FreeShapedJson(zone, shaped);

      return res;
    } catch (arangodb::basics::Exception const& ex) {
      TRI_FreeShapedJson(zone, shaped);
      return ex.code();
    } catch (...) {
      TRI_FreeShapedJson(zone, shaped);
      return TRI_ERROR_INTERNAL;
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}
    // init the update policy
    TRI_doc_update_policy_t policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr);

    int res = TRI_ERROR_INTERNAL;

    try {
      res = TRI_RemoveShapedJsonDocumentCollection(
          trx, trxCollection, key, rid, nullptr, &policy, false, false);

      if (res != TRI_ERROR_NO_ERROR &&
          res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "document removal operation failed: " +
                 std::string(TRI_errno_string(res));
    }

    return res;
  }

  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
}

static int restoreDataParser(char const* ptr, char const* pos,
                             std::string const& invalidMsg, bool useRevision,
                             std::string& errorMsg, std::string& key,
                             VPackBuilder& builder, VPackSlice& doc,
                             TRI_voc_rid_t& rid,
                             TRI_replication_operation_e& type) {
  builder.clear();

  try {
    std::string line(ptr, pos);
    VPackParser parser(builder);
    parser.parse(line);
  } catch (VPackException const&) {
    // Could not parse the given string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (std::exception const&) {
    // Could not even build the string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (...) {
    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  }

  type = REPLICATION_INVALID;

  for (auto const& pair : VPackObjectIterator(slice)) {
    if (!pair.key.isString()) {
      errorMsg = invalidMsg;

      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }

    std::string const attributeName = pair.key.copyString();

    if (attributeName == "type") {
      if (pair.value.isNumber()) {
        type = static_cast<TRI_replication_operation_e>(
            pair.value.getNumericValue<int>());
      }
    }

    else if (attributeName == "key") {
      if (pair.value.isString()) {
        key = pair.value.copyString();
      }
    }

    else if (useRevision && attributeName == "rev") {
      if (pair.value.isString()) {
        rid = StringUtils::uint64(pair.value.copyString());
      }
    }

    else if (attributeName == "data") {
      if (pair.value.isObject()) {
        doc = pair.value;
      }
    }
  }

  if (key.empty()) {
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreDataBatch(
    arangodb::Transaction* trx, CollectionNameResolver const& resolver,
    TRI_transaction_collection_t* trxCollection, bool useRevision, bool force,
    std::string& errorMsg) {
  std::string const invalidMsg = "received invalid JSON data for collection " +
                                 StringUtils::itoa(trxCollection->_cid);

  VPackBuilder builder;

  char const* ptr = _request->body();
  char const* end = ptr + _request->bodySize();

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
      VPackSlice doc;
      TRI_voc_rid_t rid = 0;
      TRI_replication_operation_e type = REPLICATION_INVALID;

      int res = restoreDataParser(ptr, pos, invalidMsg, useRevision, errorMsg,
                                  key, builder, doc, rid, type);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      res = applyCollectionDumpMarker(trx, resolver, trxCollection, type,
                                      (const TRI_voc_key_t)key.c_str(), rid,
                                      doc, errorMsg);

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
  SingleCollectionWriteTransaction<UINT64_MAX> trx(
      new StandaloneTransactionContext(), _vocbase, cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to start transaction: " + std::string(TRI_errno_string(res));

    return res;
  }

  TRI_transaction_collection_t* trxCollection = trx.trxCollection();

  if (trxCollection == nullptr) {
    res = TRI_ERROR_INTERNAL;
    errorMsg =
        "unable to start transaction: " + std::string(TRI_errno_string(res));
  } else {
    // waitForSync disabled here. use for initial replication, too
    // TODO: sync at end of trx
    trxCollection->_waitForSync = false;

    res = processRestoreDataBatch(&trx, resolver, trxCollection, useRevision,
                                  force, errorMsg);
  }

  res = trx.finish(res);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData() {
  char const* value = _request->value("collection");

  if (value == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  CollectionNameResolver resolver(_vocbase);

  TRI_voc_cid_t cid = resolver.getCollectionId(value);

  if (cid == 0) {
    std::string msg = "invalid collection parameter: '";
    msg += value;
    msg += "', cid is 0";
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, msg);
    return;
  }

  bool recycleIds = false;
  value = _request->value("recycleIds");
  if (value != nullptr) {
    recycleIds = StringUtils::boolean(value);
  }

  bool force = false;
  value = _request->value("force");
  if (value != nullptr) {
    force = StringUtils::boolean(value);
  }

  std::string errorMsg;

  int res = processRestoreData(resolver, cid, recycleIds, force, errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      generateError(HttpResponse::responseCode(res), res);
    } else {
      generateError(HttpResponse::responseCode(res), res,
                    std::string(TRI_errno_string(res)) + ": " + errorMsg);
    }
  } else {
    try {
      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("result", VPackValue(true));
      result.close();
      VPackSlice s = result.slice();
      generateResult(s);
    } catch (...) {
      generateOOMError();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreDataCoordinator() {
  char const* name = _request->value("collection");

  if (name == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  std::string dbName = _vocbase->_name;
  std::string errorMsg;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  if (col->empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
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

  char const* ptr = _request->body();
  char const* end = ptr + _request->bodySize();

  int res = TRI_ERROR_NO_ERROR;

  while (ptr < end) {
    char const* pos = strchr(ptr, '\n');

    if (pos == 0) {
      pos = end;
    } else {
      *((char*)pos) = '\0';
    }

    if (pos - ptr > 1) {
      // found something
      //
      std::string key;
      VPackSlice doc;
      TRI_voc_rid_t rid = 0;
      TRI_replication_operation_e type = REPLICATION_INVALID;

      res = restoreDataParser(ptr, pos, invalidMsg, false, errorMsg, key,
                              builder, doc, rid, type);
      if (res != TRI_ERROR_NO_ERROR) {
        // We might need to clean up buffers
        break;
      }

      if (!doc.isNone() && type != REPLICATION_MARKER_REMOVE) {
        ShardID responsibleShard;
        bool usesDefaultSharding;
        std::unique_ptr<TRI_json_t> tmp(
            arangodb::basics::VelocyPackHelper::velocyPackToJson(doc));
        res = ci->getResponsibleShard(col->id_as_string(), tmp.get(), true,
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

    char const* value;
    std::string forceopt;
    value = _request->value("force");
    if (value != nullptr) {
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
        auto headers = std::make_unique<std::map<std::string, std::string>>();
        size_t j = it->second;
        auto body = std::make_shared<std::string const>(bufs[j]->c_str(),
                                                        bufs[j]->length());
        cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                         arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
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
        if (result.answer_code == arangodb::rest::HttpResponse::OK ||
            result.answer_code == arangodb::rest::HttpResponse::CREATED) {
          VPackOptions options;
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
                   arangodb::rest::HttpResponse::SERVER_ERROR) {
          VPackOptions options;
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
          LOG(ERR) << "Bad answer code from shard: " << result.answer_code;
        }
      } else {
        LOG(ERR) << "Bad status from DBServer: " << result.status << ", msg: " << result.errorMessage.c_str() << ", shard: " << result.shardID.c_str();
        if (result.status >= CL_COMM_SENT) {
          if (result.result.get() == nullptr) {
            LOG(ERR) << "result.result is nullptr";
          } else {
            auto msg = result.result->getResultTypeMessage();
            LOG(ERR) << "Bad HTTP return code: " << result.result->getHttpReturnCode() << ", msg: " << msg.c_str();
            auto body = result.result->getBodyVelocyPack();
            msg = body->toString();
            LOG(ERR) << "Body: " << msg.c_str();
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
    generateError(HttpResponse::responseCode(res), res, errorMsg);
    return;
  }
  try {
    VPackBuilder result;
    result.openObject();
    result.add("result", VPackValue(true));
    result.close();
    generateResult(result.slice());
  } catch (...) {
    generateOOMError();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCreateKeys() {
  char const* collection = _request->value("collection");

  if (collection == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  TRI_voc_tick_t tickEnd = UINT64_MAX;

  // determine end tick for keys
  bool found;
  char const* value = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  TRI_vocbase_col_t* c =
      TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == nullptr) {
    generateError(HttpResponse::NOT_FOUND,
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

    auto keysRepository = static_cast<arangodb::CollectionKeysRepository*>(
        _vocbase->_collectionKeys);

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
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/replication/keys/<keys-id>");
    return;
  }

  static TRI_voc_tick_t const DefaultChunkSize = 5000;
  TRI_voc_tick_t chunkSize = DefaultChunkSize;

  // determine chunk size
  bool found;
  char const* value = _request->value("chunkSize", found);

  if (found) {
    chunkSize = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
    if (chunkSize < 100) {
      chunkSize = DefaultChunkSize;
    } else if (chunkSize > 20000) {
      chunkSize = 20000;
    }
  }

  std::string const& id = suffix[1];

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto keysRepository = static_cast<arangodb::CollectionKeysRepository*>(
        _vocbase->_collectionKeys);
    TRI_ASSERT(keysRepository != nullptr);

    auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
        arangodb::basics::StringUtils::uint64(id));

    auto collectionKeys = keysRepository->find(collectionKeysId);

    if (collectionKeys == nullptr) {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
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

        auto result = collectionKeys->hashChunk(from, to);

        // Add a chunk
        b.add(VPackValue(VPackValueType::Object));
        b.add("low", VPackValue(std::get<0>(result)));
        b.add("high", VPackValue(std::get<1>(result)));
        b.add("hash", VPackValue(std::to_string(std::get<2>(result))));
        b.close();
      }
      b.close();

      collectionKeys->release();
      VPackSlice s = b.slice();
      generateResult(s);
    } catch (...) {
      collectionKeys->release();
      throw;
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandFetchKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/replication/keys/<keys-id>");
    return;
  }

  static TRI_voc_tick_t const DefaultChunkSize = 5000;
  TRI_voc_tick_t chunkSize = DefaultChunkSize;

  // determine chunk size
  bool found;
  char const* value = _request->value("chunkSize", found);

  if (found) {
    chunkSize = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
    if (chunkSize < 100) {
      chunkSize = DefaultChunkSize;
    } else if (chunkSize > 20000) {
      chunkSize = 20000;
    }
  }

  value = _request->value("chunk", found);

  size_t chunk = 0;

  if (found) {
    chunk = static_cast<size_t>(StringUtils::uint64(value));
  }

  value = _request->value("type", found);

  bool keys = true;
  if (strcmp(value, "keys") == 0) {
    keys = true;
  } else if (strcmp(value, "docs") == 0) {
    keys = false;
  } else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid 'type' value");
    return;
  }

  std::string const& id = suffix[1];

  int res = TRI_ERROR_NO_ERROR;

  try {
    auto keysRepository = static_cast<arangodb::CollectionKeysRepository*>(
        _vocbase->_collectionKeys);
    TRI_ASSERT(keysRepository != nullptr);

    auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
        arangodb::basics::StringUtils::uint64(id));

    auto collectionKeys = keysRepository->find(collectionKeysId);

    if (collectionKeys == nullptr) {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
      return;
    }

    try {
      arangodb::basics::Json json(arangodb::basics::Json::Array, chunkSize);

      if (keys) {
        collectionKeys->dumpKeys(json, chunk, chunkSize);
      } else {
        bool success;
        VPackOptions options;
        std::shared_ptr<VPackBuilder> parsedIds =
            parseVelocyPackBody(&options, success);
        if (!success) {
          collectionKeys->release();
          return;
        }
        collectionKeys->dumpDocs(json, chunk, chunkSize, parsedIds->slice());
      }

      collectionKeys->release();

      generateResult(HttpResponse::OK, json.json());
    } catch (...) {
      collectionKeys->release();
      throw;
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
  }
}

void RestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  std::string const& id = suffix[1];

  auto keys = static_cast<arangodb::CollectionKeysRepository*>(
      _vocbase->_collectionKeys);
  TRI_ASSERT(keys != nullptr);

  auto collectionKeysId = static_cast<arangodb::CollectionKeysId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = keys->remove(collectionKeysId);

  if (!found) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  createResponse(HttpResponse::ACCEPTED);
  _response->setContentType("application/json; charset=utf-8");

  arangodb::basics::Json json(arangodb::basics::Json::Object);
  json.set("id", arangodb::basics::Json(id));  // id as a string!
  json.set("error", arangodb::basics::Json(false));
  json.set("code", arangodb::basics::Json(
                       static_cast<double>(_response->responseCode())));

  json.dump(_response->body());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_dump
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump() {
  char const* collection = _request->value("collection");

  if (collection == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // determine start tick for dump
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = static_cast<TRI_voc_tick_t>(UINT64_MAX);
  bool flush = true;  // flush WAL before dumping?
  bool withTicks = true;
  bool translateCollectionIds = true;
  bool failOnUnknown = false;
  uint64_t flushWait = 0;

  bool found;
  char const* value;

  // determine flush WAL value
  value = _request->value("flush", found);

  if (found) {
    flush = StringUtils::boolean(value);
  }

  // fail on unknown collection names referenced in edges
  value = _request->value("failOnUnknown", found);

  if (found) {
    failOnUnknown = StringUtils::boolean(value);
  }

  // determine flush WAL wait time value
  value = _request->value("flushWait", found);

  if (found) {
    flushWait = StringUtils::uint64(value);
    if (flushWait > 60) {
      flushWait = 60;
    }
  }

  // determine start tick for dump
  value = _request->value("from", found);

  if (found) {
    tickStart = (TRI_voc_tick_t)StringUtils::uint64(value);
  }

  // determine end tick for dump
  value = _request->value("to", found);

  if (found) {
    tickEnd = (TRI_voc_tick_t)StringUtils::uint64(value);
  }

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  bool includeSystem = true;

  value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  value = _request->value("ticks", found);

  if (found) {
    withTicks = StringUtils::boolean(value);
  }

  value = _request->value("translateIds", found);

  if (found) {
    translateCollectionIds = StringUtils::boolean(value);
  }

  TRI_vocbase_col_t* c =
      TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == nullptr) {
    generateError(HttpResponse::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  LOG(TRACE) << "requested collection dump for collection '" << collection << "', tickStart: " << tickStart << ", tickEnd: " << tickEnd;

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

    // initialize the dump container
    TRI_replication_dump_t dump(_vocbase, (size_t)determineChunkSize(),
                                includeSystem);

    res =
        TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd, withTicks,
                                      translateCollectionIds, failOnUnknown);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // generate the result
    size_t const length = TRI_LengthStringBuffer(dump._buffer);

    if (length == 0) {
      createResponse(HttpResponse::NO_CONTENT);
    } else {
      createResponse(HttpResponse::OK);
    }

    _response->setContentType("application/x-arango-dump; charset=utf-8");

    // set headers
    _response->setHeader(
        TRI_REPLICATION_HEADER_CHECKMORE,
        strlen(TRI_REPLICATION_HEADER_CHECKMORE),
        ((dump._hasMore || dump._bufferFull) ? "true" : "false"));

    _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED,
                         strlen(TRI_REPLICATION_HEADER_LASTINCLUDED),
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
    generateError(HttpResponse::responseCode(res), res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandMakeSlave() {
  bool success;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, success);
  if (!success) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  VPackSlice const body = parsedBody->slice();

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (endpoint.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
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
  TRI_InitConfigurationReplicationApplier(&defaults);

  // initialize target configuration
  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);

  config._endpoint =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._database =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, database.c_str(), database.size());
  config._username =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
  config._password =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, password.c_str(), password.size());
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
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  // forget about any existing replication applier configuration
  int res = _vocbase->_replicationApplier->forget();

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
    return;
  }

  // start initial synchronization
  TRI_voc_tick_t lastLogTick = 0;
  std::string errorMsg = "";
  {
    InitialSyncer syncer(_vocbase, &config, config._restrictCollections,
                         restrictType, false);

    res = TRI_ERROR_NO_ERROR;

    try {
      res = syncer.run(errorMsg, false);
    } catch (...) {
      errorMsg = "caught an exception";
      res = TRI_ERROR_INTERNAL;
    }

    lastLogTick = syncer.getLastLogTick();
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res, errorMsg);
    return;
  }

  res = TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
    return;
  }

  res = _vocbase->_replicationApplier->start(lastLogTick, true);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res);
    return;
  }

  try {
    std::shared_ptr<VPackBuilder> result =
        _vocbase->_replicationApplier->toVelocyPack();
    generateResult(result->slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_synchronize
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSync() {
  bool success;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, success);
  if (!success) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  VPackSlice const body = parsedBody->slice();

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (endpoint.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
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
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
  config._endpoint =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._database =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, database.c_str(), database.size());
  config._username =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
  config._password =
      TRI_DuplicateString(TRI_CORE_MEM_ZONE, password.c_str(), password.size());
  config._includeSystem = includeSystem;
  config._verbose = verbose;

  InitialSyncer syncer(_vocbase, &config, restrictCollections, restrictType,
                       verbose);

  int res = TRI_ERROR_NO_ERROR;
  std::string errorMsg = "";

  try {
    res = syncer.run(errorMsg, incremental);
  } catch (...) {
    errorMsg = "caught an exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::responseCode(res), res, errorMsg);
    return;
  }
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));

    result.add("collections", VPackValue(VPackValueType::Array));

    std::map<TRI_voc_cid_t, std::string>::const_iterator it;
    std::map<TRI_voc_cid_t, std::string> const& c =
        syncer.getProcessedCollections();

    for (it = c.begin(); it != c.end(); ++it) {
      std::string const cidString = StringUtils::itoa((*it).first);
      // Insert a collection
      result.add(VPackValue(VPackValueType::Object));
      result.add("id", VPackValue(cidString));
      result.add("name", VPackValue((*it).second));
      result.close();  // one collection
    }

    result.close();  // collections

    auto tickString = std::to_string(syncer.getLastLogTick());
    result.add("lastLogTick", VPackValue(tickString));

    result.close();  // base
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (...) {
    generateOOMError();
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
    VPackSlice s = result.slice();
    generateResult(s);
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
  TRI_InitConfigurationReplicationApplier(&config);

  {
    READ_LOCKER(readLocker, _vocbase->_replicationApplier->_statusLock);
    TRI_CopyConfigurationReplicationApplier(
        &_vocbase->_replicationApplier->_configuration, &config);
  }
  try {
    std::shared_ptr<VPackBuilder> configBuilder = config.toVelocyPack(false);
    generateResult(configBuilder->slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig() {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);

  bool success;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, success);

  if (!success) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  VPackSlice const body = parsedBody->slice();

  {
    READ_LOCKER(readLocker, _vocbase->_replicationApplier->_statusLock);
    TRI_CopyConfigurationReplicationApplier(
        &_vocbase->_replicationApplier->_configuration, &config);
  }

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");

  if (!endpoint.empty()) {
    if (config._endpoint != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._endpoint);
    }
    config._endpoint = TRI_DuplicateString(TRI_CORE_MEM_ZONE, endpoint.c_str(),
                                           endpoint.size());
  }

  std::string database =
      VelocyPackHelper::getStringValue(body, "database", _vocbase->_name);
  if (config._database != nullptr) {
    // free old value
    TRI_FreeString(TRI_CORE_MEM_ZONE, config._database);
  }
  config._database = TRI_DuplicateString(TRI_CORE_MEM_ZONE, database.c_str(),
                                         database.length());

  VPackSlice const username = body.get("username");
  if (username.isString()) {
    if (config._username != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._username);
    }
    std::string tmp = username.copyString();
    config._username =
        TRI_DuplicateString(TRI_CORE_MEM_ZONE, tmp.c_str(), tmp.length());
  }

  VPackSlice const password = body.get("password");
  if (password.isString()) {
    if (config._password != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._password);
    }
    std::string tmp = password.copyString();
    config._password =
        TRI_DuplicateString(TRI_CORE_MEM_ZONE, tmp.c_str(), tmp.length());
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
    generateError(HttpResponse::responseCode(res), res);
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
  char const* value = _request->value("from", found);

  TRI_voc_tick_t initialTick = 0;
  if (found) {
    // query parameter "from" specified
    initialTick = (TRI_voc_tick_t)StringUtils::uint64(value);
  }

  int res = _vocbase->_replicationApplier->start(initialTick, found);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION ||
        res == TRI_ERROR_REPLICATION_RUNNING) {
      generateError(HttpResponse::BAD, res);
    } else {
      generateError(HttpResponse::SERVER_ERROR, res);
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
    generateError(HttpResponse::SERVER_ERROR, res);
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
    generateResult(result->slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
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
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  handleCommandApplierGetState();
}
