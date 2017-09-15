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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRestReplicationHandler.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "GeneralServer/GeneralServer.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::rocksutils;

RocksDBRestReplicationHandler::RocksDBRestReplicationHandler(
    GeneralRequest* request, GeneralResponse* response)
    : RestReplicationHandler(request, response),
      _manager(globalRocksEngine()->replicationManager()) {}

RocksDBRestReplicationHandler::~RocksDBRestReplicationHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_return_state
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerState() {
  VPackBuilder builder;
  auto res = globalRocksEngine()->createLoggerState(_vocbase, builder);
  if (res.fail()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "failed to create logger-state"
                                          << res.errorMessage();
    generateError(rest::ResponseCode::BAD, res.errorNumber(),
                  res.errorMessage());
    return;
  }
  generateResult(rest::ResponseCode::OK, builder.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the available logfile range
/// @route GET logger-tick-ranges
/// @caller js/client/modules/@arangodb/replication.js
/// @response VPackArray, containing info about each datafile
///           * filename
///           * status
///           * tickMin - tickMax
//////////////////////////////////////////////////////////////////////////////
void RocksDBRestReplicationHandler::handleCommandLoggerTickRanges() {
  VPackBuilder b;
  Result res = globalRocksEngine()->createTickRanges(b);
  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, b.slice());
  } else {
    generateError(res);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the first tick available in a logfile
/// @route GET logger-first-tick
/// @caller js/client/modules/@arangodb/replication.js
/// @response VPackObject with minTick of LogfileManager->ranges()
//////////////////////////////////////////////////////////////////////////////
void RocksDBRestReplicationHandler::handleCommandLoggerFirstTick() {
  TRI_voc_tick_t tick = UINT64_MAX;
  Result res = EngineSelectorFeature::ENGINE->firstTick(tick);

  VPackBuilder b;
  b.add(VPackValue(VPackValueType::Object));
  if (tick == UINT64_MAX || res.fail()) {
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

void RocksDBRestReplicationHandler::handleCommandBatch() {
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

    RocksDBReplicationContext* ctx = _manager->createContext();
    if (ctx == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to create replication context");
    }

    // extract ttl
    if (input->slice().hasKey("ttl")){
      double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", RocksDBReplicationContext::DefaultTTL);
      ctx->adjustTtl(ttl);
    }

    // create transaction+snapshot
    RocksDBReplicationContextGuard(_manager, ctx);
    ctx->bind(_vocbase);

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(ctx->id())));  // id always string
    b.add("lastTick", VPackValue(std::to_string(ctx->lastTick())));
    b.close();

    // add client
    bool found;
    std::string const& value = _request->value("serverId", found);
    TRI_server_id_t serverId = 0;

    if (found) {
      serverId = (TRI_server_id_t)StringUtils::uint64(value);
    } else {
      serverId = ctx->id();
    }

    _vocbase->updateReplicationClient(serverId, ctx->lastTick());

    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    auto input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    int res = TRI_ERROR_NO_ERROR;
    bool busy;
    RocksDBReplicationContext* ctx = _manager->find(id, busy, expires);
    RocksDBReplicationContextGuard(_manager, ctx);
    if (busy) {
      res = TRI_ERROR_CURSOR_BUSY;
      generateError(GeneralResponse::responseCode(res), res);
      return;
    } else if (ctx == nullptr) {
      res = TRI_ERROR_CURSOR_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
      return;
    }

    // add client
    bool found;
    std::string const& value = _request->value("serverId", found);
    TRI_server_id_t serverId = 0;

    if (found) {
      serverId = (TRI_server_id_t)StringUtils::uint64(value);
    } else {
      serverId = ctx->id();
    }

    _vocbase->updateReplicationClient(serverId, ctx->lastTick());

    resetResponse(rest::ResponseCode::NO_CONTENT);
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    bool found = _manager->remove(id);
    // StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // int res = engine->removeCompactionBlocker(_vocbase, id);

    if (found) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_CURSOR_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_returns
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerFollow() {
  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }

  // determine start and end tick
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = UINT64_MAX;

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

  bool includeSystem = true;
  std::string const& value4 = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value4);
  }

  size_t chunkSize = 1024 * 1024;  // TODO: determine good default value?
  std::string const& value5 = _request->value("chunkSize", found);
  if (found) {
    chunkSize = static_cast<size_t>(StringUtils::uint64(value5));
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

  auto trxContext = transaction::StandaloneContext::Create(_vocbase);
  VPackBuilder builder(trxContext->getVPackOptions());
  builder.openArray();
  auto result = tailWal(_vocbase, tickStart, tickEnd, chunkSize, includeSystem,
                        cid, builder);
  builder.close();
  auto data = builder.slice();

  uint64_t const latest = latestSequenceNumber();

  if (result.fail()) {
    generateError(GeneralResponse::responseCode(result.errorNumber()),
                  result.errorNumber(), result.errorMessage());
    return;
  }

  bool const checkMore = (result.maxTick() > 0 && result.maxTick() < latest);

  // generate the result
  size_t length = data.length();

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
  _response->setHeaderNC(
      TRI_REPLICATION_HEADER_LASTINCLUDED,
      StringUtils::itoa((length == 0) ? 0 : result.maxTick()));
  _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                         StringUtils::itoa(latest));
  _response->setHeaderNC(TRI_REPLICATION_HEADER_ACTIVE, "true");
  _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                         result.fromTickIncluded() ? "true" : "false");

  if (length > 0) {
    if (useVst) {
      for (auto message : arangodb::velocypack::ArrayIterator(data)) {
        _response->addPayload(VPackSlice(message),
                              trxContext->getVPackOptions(), true);
      }
    } else {
      HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());

      if (httpResponse == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid response type");
      }

      basics::StringBuffer& buffer = httpResponse->body();
      arangodb::basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
      // note: we need the CustomTypeHandler here
      VPackDumper dumper(&adapter, trxContext->getVPackOptions());
      for (auto marker : arangodb::velocypack::ArrayIterator(data)) {
        dumper.dump(marker);
        httpResponse->body().appendChar('\n');
      }
    }
    // add client
    bool found;
    std::string const& value = _request->value("serverId", found);

    TRI_server_id_t serverId = 0;
    if (found) {
      serverId = (TRI_server_id_t)StringUtils::uint64(value);
    }
    _vocbase->updateReplicationClient(serverId, result.maxTick());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }
  //_response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
  // StringUtils::itoa(dump._lastFoundTick));
  _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK, "0");
  _response->setContentType(rest::ContentType::DUMP);
  //_response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
  // dump._fromTickIncluded ? "true" : "false");
  _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT, "true");
  VPackSlice slice = VelocyPackHelper::EmptyArrayValue();
  if (useVst) {
    _response->addPayload(slice, &VPackOptions::Defaults, false);
  } else {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());

    if (httpResponse == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }

    httpResponse->body().appendText(slice.toJson());
  }
  _response->setResponseCode(rest::ResponseCode::OK);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_inventory
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandInventory() {
  RocksDBReplicationContext* ctx = nullptr;
  bool found, busy;
  std::string batchId = _request->value("batchId", found);
  if (found) {
    ctx = _manager->find(StringUtils::uint64(batchId), busy);
  }
  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
    return;
  }
  if (busy || ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "context is busy or nullptr");
    return;
  }
  RocksDBReplicationContextGuard(_manager, ctx);

  TRI_voc_tick_t tick = TRI_CurrentTickServer();

  // include system collections?
  bool includeSystem = true;
  std::string const& value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>> result =
      ctx->getInventory(this->_vocbase, includeSystem);
  if (!result.first.ok()) {
    generateError(rest::ResponseCode::BAD, result.first.errorNumber(),
                  "inventory could not be created");
    return;
  }

  VPackSlice const collections = result.second->slice();
  TRI_ASSERT(collections.isArray());

  VPackBuilder builder;
  builder.openObject();

  // add collections data
  builder.add("collections", collections);

  // "state"
  builder.add("state", VPackValue(VPackValueType::Object));

  // RocksDBLogfileManagerState const s =
  // RocksDBLogfileManager::instance()->state();

  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(ctx->lastTick())));
  builder.add(
      "lastUncommittedLogTick",
      VPackValue(std::to_string(ctx->lastTick())));  // s.lastAssignedTick
  builder.add("totalEvents",
              VPackValue(ctx->lastTick()));  // s.numEvents + s.numEventsSync
  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();  // state

  std::string const tickString(std::to_string(tick));
  builder.add("tick", VPackValue(tickString));
  builder.close();  // Toplevel

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandCreateKeys() {
  std::string const& collection = _request->value("collection");
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  RocksDBReplicationContext* ctx = nullptr;

  //get batchId from url parameters
  bool found, busy;
  std::string batchId = _request->value("batchId", found);

  // find context
  if (found) {
    ctx = _manager->find(StringUtils::uint64(batchId), busy);
  }
  if (!found || busy || ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
    return;
  }
  RocksDBReplicationContextGuard(_manager, ctx);

  // to is ignored because the snapshot time is the latest point in time
 
  // TRI_voc_tick_t tickEnd = UINT64_MAX;
  // determine end tick for keys
  // std::string const& value = _request->value("to", found);
  // if (found) {
  //  tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  //}

  // arangodb::LogicalCollection* c = _vocbase->lookupCollection(collection);
  // arangodb::CollectionGuard guard(_vocbase, c->cid(), false);
  // arangodb::LogicalCollection* col = guard.collection();

  // bind collection to context - will initialize iterator
  int res = ctx->bindCollection(collection);
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("id", VPackValue(StringUtils::itoa(ctx->id())));
  result.add("count", VPackValue(ctx->count()));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandGetKeys() {
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

  //first suffix needs to be the batch id
  std::string const& id = suffixes[1];
  uint64_t batchId = arangodb::basics::StringUtils::uint64(id);

  // get context
  bool busy;
  RocksDBReplicationContext* ctx = _manager->find(batchId, busy);
  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified, expired or invalid in another way");
    return;
  }
  if (busy) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "RequestContext is busy");
    return;
  }

  //lock context
  RocksDBReplicationContextGuard(_manager, ctx);

  VPackBuilder b;
  ctx->dumpKeyChunks(b, chunkSize);
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandFetchKeys() {
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

  // chunk is supplied by old clients, low is an optimization
  // for rocksdb, because seeking should be cheaper
  std::string const& value2 = _request->value("chunk", found);
  size_t chunk = 0;
  if (found) {
    chunk = static_cast<size_t>(StringUtils::uint64(value2));
  }
  std::string const& lowKey = _request->value("low", found);

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

  std::string const& id = suffixes[1];

  uint64_t batchId = arangodb::basics::StringUtils::uint64(id);
  bool busy;
  RocksDBReplicationContext* ctx = _manager->find(batchId, busy);
  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified or not found");
    return;
  }

  if (busy) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batch is busy");
    return;
  }
  RocksDBReplicationContextGuard(_manager, ctx);

  auto trxContext = transaction::StandaloneContext::Create(_vocbase);
  VPackBuilder resultBuilder(trxContext->getVPackOptions());
  if (keys) {
    Result rv = ctx->dumpKeys(resultBuilder, chunk, static_cast<size_t>(chunkSize), lowKey);
    if (rv.fail()){
      generateError(rv);
      return;
    }
  } else {
    bool success;
    std::shared_ptr<VPackBuilder> parsedIds = parseVelocyPackBody(success);
    if (!success) {
      generateResult(rest::ResponseCode::BAD, VPackSlice());
      return;
    }
    Result rv = ctx->dumpDocuments(resultBuilder, chunk, static_cast<size_t>(chunkSize), lowKey, parsedIds->slice());
    if (rv.fail()){
      generateError(rv);
      return;
    }
  }

  generateResult(rest::ResponseCode::OK, resultBuilder.slice(), trxContext);
}

void RocksDBRestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  std::string const& id = suffixes[1];
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

void RocksDBRestReplicationHandler::handleCommandDump() {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "enter handleCommandDump";

  bool found = false;
  uint64_t contextId = 0;

  // contains dump options that might need to be inspected
  // VPackSlice options = _request->payload();

  // get collection Name
  std::string const& collection = _request->value("collection");
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // get contextId
  std::string const& contextIdString = _request->value("batchId", found);
  if (found) {
    contextId = StringUtils::uint64(contextIdString);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - request misses batchId");
    return;
  }

  // acquire context
  bool isBusy = false;
  RocksDBReplicationContext* context = _manager->find(contextId, isBusy);
  RocksDBReplicationContextGuard(_manager, context);

  if (context == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - unable to find context (it could be expired)");
    return;
  }

  if (isBusy) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - context is busy");
    return;
  }

  // print request
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "requested collection dump for collection '" << collection
      << "' using contextId '" << context->id() << "'";


  // TODO needs to generalized || velocypacks needs to support multiple slices
  // per response!
  auto response = dynamic_cast<HttpResponse*>(_response.get());
  StringBuffer dump(8192, false);

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
  }

  // do the work!
  auto result = context->dump(_vocbase, collection, dump, determineChunkSize());

  // generate the result
  if (dump.length() == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }

  response->setContentType(rest::ContentType::DUMP);
  // set headers
  _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                         (context->more() ? "true" : "false"));

  _response->setHeaderNC(
      TRI_REPLICATION_HEADER_LASTINCLUDED,
      StringUtils::itoa((dump.length() == 0) ? 0 : result.maxTick()));

  // transfer ownership of the buffer contents
  response->body().set(dump.stringBuffer());

  // avoid double freeing
  TRI_StealStringBuffer(dump.stringBuffer());
}
