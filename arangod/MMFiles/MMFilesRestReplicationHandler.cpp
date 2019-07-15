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

#include "MMFilesRestReplicationHandler.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollectionKeys.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/mmfiles-replication-dump.h"
#include "Replication/utilities.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

MMFilesRestReplicationHandler::MMFilesRestReplicationHandler(GeneralRequest* request,
                                                             GeneralResponse* response)
    : RestReplicationHandler(request, response) {}

MMFilesRestReplicationHandler::~MMFilesRestReplicationHandler() = default;

/// @brief insert the applier action into an action list
void MMFilesRestReplicationHandler::insertClient(TRI_voc_tick_t lastServedTick) {
  TRI_server_id_t const clientId = StringUtils::uint64(_request->value("serverId"));
  SyncerId const syncerId = SyncerId::fromRequest(*_request);

  _vocbase.replicationClients().track(syncerId, clientId, lastServedTick,
                                      replutils::BatchInfo::DefaultTimeout);
}

// prevents datafiles from being removed while dumping the contents
void MMFilesRestReplicationHandler::handleCommandBatch() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new blocker
    std::shared_ptr<VPackBuilder> input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl",
                                                           replutils::BatchInfo::DefaultTimeout);

    TRI_voc_tick_t id;
    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    int res = engine->insertCompactionBlocker(&_vocbase, ttl, id);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(id)));
    b.add("lastTick", VPackValue("0"));  // not known yet
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    auto input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl",
                                                           replutils::BatchInfo::DefaultTimeout);

    // now extend the blocker
    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    int res = engine->extendCompactionBlocker(&_vocbase, id, ttl);

    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    int res = engine->removeCompactionBlocker(&_vocbase, id);

    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

/// @brief add or remove a WAL logfile barrier
void MMFilesRestReplicationHandler::handleCommandBarrier() {
  // extract the request type
  auto const type = _request->requestType();
  std::vector<std::string> const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new barrier

    std::shared_ptr<VPackBuilder> input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl",
                                                           replutils::BarrierInfo::DefaultTimeout);

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
        MMFilesLogfileManager::instance()->addLogfileBarrier(_vocbase.id(), minTick, ttl);
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
    TRI_voc_tick_t id = StringUtils::uint64(suffixes[1]);

    std::shared_ptr<VPackBuilder> input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl",
                                                           replutils::BarrierInfo::DefaultTimeout);

    TRI_voc_tick_t minTick = 0;
    VPackSlice const v = input->slice().get("tick");

    if (v.isString()) {
      minTick = StringUtils::uint64(v.copyString());
    } else if (v.isNumber()) {
      minTick = v.getNumber<TRI_voc_tick_t>();
    }

    if (MMFilesLogfileManager::instance()->extendLogfileBarrier(id, ttl, minTick)) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing barrier
    TRI_voc_tick_t id = StringUtils::uint64(suffixes[1]);

    if (MMFilesLogfileManager::instance()->removeLogfileBarrier(id)) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::GET) {
    // fetch all barriers
    auto ids = MMFilesLogfileManager::instance()->getLogfileBarriers();

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
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

void MMFilesRestReplicationHandler::handleCommandLoggerFollow() {
  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }

  // determine start and end tick
  MMFilesLogfileManagerState state = MMFilesLogfileManager::instance()->state();
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

  // don't read over the last committed tick value, which we will return
  // as part of our response as well
  tickEnd = std::max(tickEnd, state.lastCommittedTick); 

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
      firstRegularTick = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value5));
    }
    // copy default options
    VPackOptions options = VPackOptions::Defaults;
    options.checkAttributeUniqueness = true;
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

  grantTemporaryRights();

  // extract collection
  TRI_voc_cid_t cid = 0;
  std::string const& value6 = _request->value("collection", found);

  if (found) {
    auto c = _vocbase.lookupCollection(value6);

    if (c == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      return;
    }

    cid = c->id();
  }

  if (barrierId > 0) {
    // extend the WAL logfile barrier
    MMFilesLogfileManager::instance()->extendLogfileBarrier(barrierId, 180, tickStart);
  }

  auto ctx = transaction::StandaloneContext::Create(_vocbase);

  // initialize the dump container
  MMFilesReplicationDumpContext dump(ctx, static_cast<size_t>(determineChunkSize()),
                                     includeSystem, cid, useVst);

  // and dump
  int res = MMFilesDumpLogReplication(&dump, transactionIds, firstRegularTick,
                                      tickStart, tickEnd, false);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(GeneralResponse::responseCode(res), res);
    return;
  }

  bool const checkMore =
      (dump._lastFoundTick > 0 && dump._lastFoundTick != state.lastCommittedTick);

  // generate the result
  size_t length = 0;

  if (useVst) {
    length = dump._slices.size();
  } else {
    length = TRI_LengthStringBuffer(dump._buffer);
  }

  if (length == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }
 
  // pull the latest state again, so that the last tick we hand out is always >=
  // the last included tick value in the results 
  while (state.lastCommittedTick < dump._lastFoundTick &&
         !application_features::ApplicationServer::isStopping()) {
    state = MMFilesLogfileManager::instance()->state();
    std::this_thread::sleep_for(std::chrono::microseconds(500));
  }

  // transfer ownership of the buffer contents
  _response->setContentType(rest::ContentType::DUMP);

  // set headers
  _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                         checkMore ? "true" : "false");
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                         StringUtils::itoa(dump._lastFoundTick));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick,
                         StringUtils::itoa(state.lastCommittedTick));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastScanned,
                         StringUtils::itoa(dump._lastScannedTick));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderActive,
                         "true");  // TODO remove
  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                         dump._fromTickIncluded ? "true" : "false");

  if (length > 0) {
    if (useVst) {
      for (auto message : dump._slices) {
        _response->addPayload(std::move(message), &dump._vpackOptions, true);
      }
    } else {
      HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());

      if (httpResponse == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid response type");
      }

      // transfer ownership of the buffer contents
      httpResponse->body().set(dump._buffer);

      // to avoid double freeing
      TRI_StealStringBuffer(dump._buffer);
    }
  }

  // insert the start tick (minus 1 to be on the safe side) as the
  // minimum tick we need to keep on the master. we cannot be sure
  // the master's response makes it to the slave safely, so we must
  // not insert the maximum of the WAL entries we sent. if we did,
  // and the response does not make it to the slave, the master will
  // note a higher tick than the slave will have received, which may
  // lead to the master eventually deleting a WAL section that the
  // slave will still request later
  insertClient(tickStart == 0 ? tickStart : tickStart - 1);
}

/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
void MMFilesRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  // determine start and end tick
  MMFilesLogfileManagerState const state = MMFilesLogfileManager::instance()->state();
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

  auto ctx = transaction::StandaloneContext::Create(_vocbase);

  // initialize the dump container
  MMFilesReplicationDumpContext dump(ctx, static_cast<size_t>(determineChunkSize()),
                                     false, 0);

  // and dump
  int res = MMFilesDetermineOpenTransactionsReplication(&dump, tickStart, tickEnd);

  if (res != TRI_ERROR_NO_ERROR) {
    std::string const err = "failed to determine open transactions";
    LOG_TOPIC("5b093", ERR, Logger::REPLICATION) << err;
    generateError(rest::ResponseCode::BAD, res, err);
    return;
  }

  // generate the result
  size_t const length = TRI_LengthStringBuffer(dump._buffer);

  if (length == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }

  HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
  if (_response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
  }

  _response->setContentType(rest::ContentType::DUMP);

  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                         dump._fromTickIncluded ? "true" : "false");

  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick,
                         StringUtils::itoa(dump._lastFoundTick));

  if (length > 0) {
    // transfer ownership of the buffer contents
    httpResponse->body().set(dump._buffer);

    // to avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
  }
}

void MMFilesRestReplicationHandler::handleCommandInventory() {
  TRI_voc_tick_t tick = TRI_CurrentTickServer();

  // include system collections?
  bool includeSystem = _request->parsedValue("includeSystem", true);
  bool includeFoxxQueues = _request->parsedValue("includeFoxxQueues", false);

  // produce inventory for all databases?
  bool global = _request->parsedValue("global", false);

  if (global && _request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
        "global inventory can only be created from within _system database");
    return;
  }
  
  auto nameFilter = [&](LogicalCollection const* collection) {
    std::string const& cname = collection->name();
    if (!includeSystem && TRI_vocbase_t::IsSystemName(cname)) {
      // exclude all system collections
      return false;
    }

    if (TRI_ExcludeCollectionReplication(cname, includeSystem, includeFoxxQueues)) {
      // collection is excluded from replication
      return false;
    }

    // all other cases should be included
    return true;
  };

  VPackBuilder builder;
  builder.openObject();

  // collections and indexes
  if (global) {
    builder.add(VPackValue("databases"));
    DatabaseFeature::DATABASE->inventory(builder, tick, nameFilter);
  } else {
    // add collections and views
    grantTemporaryRights();
    _vocbase.inventory(builder, tick, nameFilter);
    TRI_ASSERT(builder.hasKey("collections") && builder.hasKey("views"));
  }

  // "state"
  builder.add("state", VPackValue(VPackValueType::Object));

  MMFilesLogfileManagerState const s = MMFilesLogfileManager::instance()->state();

  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(s.lastCommittedTick)));
  builder.add("lastUncommittedLogTick", VPackValue(std::to_string(s.lastAssignedTick)));
  builder.add("totalEvents", VPackValue(s.numEvents + s.numEventsSync));
  builder.add("time", VPackValue(s.timeString));
  builder.close();  // state

  std::string const tickString(std::to_string(tick));
  builder.add("tick", VPackValue(tickString));
  builder.close();  // top level

  generateResult(rest::ResponseCode::OK, builder.slice());
}

/// @brief produce list of keys for a specific collection
void MMFilesRestReplicationHandler::handleCommandCreateKeys() {
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

  auto c = _vocbase.lookupCollection(collection);

  if (c == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  auto guard = std::make_unique<arangodb::CollectionGuard>(&_vocbase, c->id(), false);

  arangodb::LogicalCollection* col = guard->collection();
  TRI_ASSERT(col != nullptr);

  // turn off the compaction for the collection
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  TRI_voc_tick_t id;
  int res = engine->insertCompactionBlocker(&_vocbase, 1200.0, id);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // initialize a container with the keys
  auto keys = std::make_unique<MMFilesCollectionKeys>(_vocbase, std::move(guard), id, 900.0);

  std::string const idString(std::to_string(keys->id()));

  keys->create(tickEnd);

  size_t const count = keys->count();
  auto keysRepository = _vocbase.collectionKeys();

  keysRepository->store(std::move(keys));

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("id", VPackValue(idString));
  result.add("count", VPackValue(count));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

/// @brief returns all key ranges
void MMFilesRestReplicationHandler::handleCommandGetKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
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

  std::string const& id = suffixes[1];

  auto keysRepository = _vocbase.collectionKeys();
  TRI_ASSERT(keysRepository != nullptr);

  auto collectionKeysId =
      static_cast<CollectionKeysId>(arangodb::basics::StringUtils::uint64(id));
  auto collectionKeys = keysRepository->find(collectionKeysId);

  if (collectionKeys == nullptr) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                  TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }
    
  TRI_DEFER(collectionKeys->release());

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

  generateResult(rest::ResponseCode::OK, b.slice());
}

/// @brief returns date for a key range
void MMFilesRestReplicationHandler::handleCommandFetchKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
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

  size_t offsetInChunk = 0;
  size_t maxChunkSize = SIZE_MAX;
  std::string const& value4 = _request->value("offset", found);
  if (found) {
    offsetInChunk = static_cast<size_t>(StringUtils::uint64(value4));
    // "offset" was introduced with ArangoDB 3.3. if the client sends it,
    // it means we can adapt the result size dynamically and the client
    // may refetch data for the same chunk
    maxChunkSize = 8 * 1024 * 1024;
    // if a client does not send an "offset" parameter at all, we are
    // not sure if it supports this protocol (3.2 and before) or not
  }

  std::string const& id = suffixes[1];

  auto keysRepository = _vocbase.collectionKeys();
  TRI_ASSERT(keysRepository != nullptr);

  auto collectionKeysId =
      static_cast<CollectionKeysId>(arangodb::basics::StringUtils::uint64(id));
  auto collectionKeys = keysRepository->find(collectionKeysId);

  if (collectionKeys == nullptr) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                  TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }
    
  TRI_DEFER(collectionKeys->release());

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  VPackBuilder resultBuilder(ctx->getVPackOptions());

  resultBuilder.openArray();

  if (keys) {
    collectionKeys->dumpKeys(resultBuilder, chunk, static_cast<size_t>(chunkSize));
  } else {
    bool success = false;
    VPackSlice parsedIds = this->parseVPackBody(success);

    if (!success) {
      // error already created
      return;
    }

    collectionKeys->dumpDocs(resultBuilder, chunk, static_cast<size_t>(chunkSize),
                              offsetInChunk, maxChunkSize, parsedIds);
  }

  resultBuilder.close();

  generateResult(rest::ResponseCode::OK, resultBuilder.slice(), ctx);
}

void MMFilesRestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  std::string const& id = suffixes[1];

  auto keys = _vocbase.collectionKeys();
  TRI_ASSERT(keys != nullptr);

  auto collectionKeysId =
      static_cast<CollectionKeysId>(arangodb::basics::StringUtils::uint64(id));
  bool found = keys->remove(collectionKeysId);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add("id", VPackValue(id));  // id as a string
  resultBuilder.add(StaticStrings::Error, VPackValue(false));
  resultBuilder.add(StaticStrings::Code,
                    VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  resultBuilder.close();

  generateResult(rest::ResponseCode::ACCEPTED, resultBuilder.slice());
}

void MMFilesRestReplicationHandler::handleCommandDump() {
  std::string const& collection = _request->value("collection");

  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // flush WAL before dumping?
  bool flush = _request->parsedValue("flush", true);

  // determine flush WAL wait time value
  uint64_t flushWait = _request->parsedValue("flushWait", static_cast<uint64_t>(0));
  if (flushWait > 300) {
    flushWait = 300;
  }

  // determine start tick for dump
  TRI_voc_tick_t tickStart =
      _request->parsedValue("from", static_cast<TRI_voc_tick_t>(0));

  // determine end tick for dump
  TRI_voc_tick_t tickEnd =
      _request->parsedValue("to", static_cast<TRI_voc_tick_t>(UINT64_MAX));

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  bool includeSystem = _request->parsedValue("includeSystem", true);
  bool withTicks = _request->parsedValue("ticks", true);

  grantTemporaryRights();

  auto c = _vocbase.lookupCollection(collection);

  if (c == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr &&
      !exec->canUseCollection(_vocbase.name(), c->name(), auth::Level::RO)) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return;
  }

  LOG_TOPIC("8311f", TRACE, arangodb::Logger::REPLICATION)
      << "requested collection dump for collection '" << collection
      << "', tickStart: " << tickStart << ", tickEnd: " << tickEnd;

  if (flush) {
    MMFilesLogfileManager::instance()->flush(true, true, false,
                                             static_cast<double>(flushWait), true);

    // additionally wait for the collector
    if (flushWait > 0) {
      MMFilesLogfileManager::instance()->waitForCollectorQueue(c->id(),
                                                               static_cast<double>(flushWait));
    }
  }

  arangodb::CollectionGuard guard(&_vocbase, c->id(), false);
  arangodb::LogicalCollection* col = guard.collection();
  TRI_ASSERT(col != nullptr);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  // initialize the dump container
  MMFilesReplicationDumpContext dump(ctx, static_cast<size_t>(determineChunkSize()),
                                     includeSystem, 0);

  int res = MMFilesDumpCollectionReplication(&dump, col, tickStart, tickEnd, withTicks);

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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
  }

  response->setContentType(rest::ContentType::DUMP);

  // set headers
  _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                         (dump._hasMore ? "true" : "false"));

  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                         StringUtils::itoa(dump._lastFoundTick));

  // transfer ownership of the buffer contents
  response->body().set(dump._buffer);

  // avoid double freeing
  TRI_StealStringBuffer(dump._buffer);
}
