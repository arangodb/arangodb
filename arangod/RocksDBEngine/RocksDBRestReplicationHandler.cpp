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
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Replication/utilities.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
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

void RocksDBRestReplicationHandler::handleCommandBatch() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new blocker
    
    bool parseSuccess = true;
    VPackSlice body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess || !body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }
    double ttl = VelocyPackHelper::getNumericValue<double>(body, "ttl", 0);
    std::string patchCount = VelocyPackHelper::getStringValue(body, "patchCount", "");

    bool found;
    std::string const& value = _request->value("serverId", found);
    TRI_server_id_t serverId = 0;

    if (found && !value.empty() && value != "none") {
      serverId = static_cast<TRI_server_id_t>(StringUtils::uint64(value));
    }

    // create transaction+snapshot, ttl will be 300 if `ttl == 0``
    auto* ctx = _manager->createContext(ttl, serverId);
    RocksDBReplicationContextGuard guard(_manager, ctx);
    
    if (!patchCount.empty()) {
      auto triple = ctx->bindCollectionIncremental(_vocbase, patchCount);
      Result res = std::get<0>(triple);
      if (res.fail()) {
        LOG_TOPIC(WARN, Logger::REPLICATION) << "Error during first phase of"
        << " collection count patching: " << res.errorMessage();
      }
    }

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(ctx->id())));  // id always string
    b.add("lastTick", VPackValue(std::to_string(ctx->snapshotTick())));
    b.close();

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

    // extract ttl. Context uses initial ttl from batch creation, if `ttl == 0`
    double ttl = VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    int res = _manager->extendLifetime(id, ttl);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(GeneralResponse::responseCode(res), res);
      return;
    }

    // add client
    bool found;
    std::string const& value = _request->value("serverId", found);
    if (!found) {
      LOG_TOPIC(DEBUG, Logger::REPLICATION)
          << "no serverId parameter found in request to "
          << _request->fullUrl();
    }

    TRI_server_id_t serverId = id; // just use context id as fallback
    if (!value.empty() && value != "none") {
      serverId = static_cast<TRI_server_id_t>(StringUtils::uint64(value));
    }

    // last tick value in context should not have changed compared to the
    // initial tick value used in the context (it's only updated on bind()
    // call, which is only executed when a batch is initially created)
    _vocbase.updateReplicationClient(serverId, ttl);

    resetResponse(rest::ResponseCode::NO_CONTENT);
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    bool found = _manager->remove(id);
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

// handled by the batch for rocksdb
void RocksDBRestReplicationHandler::handleCommandBarrier() {
  auto const type = _request->requestType();
  if (type == rest::RequestType::POST) {
    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    // always return a non-0 barrier id
    // it will be ignored by the client anyway for the RocksDB engine
    std::string const idString = std::to_string(TRI_NewTickServer());
    b.add("id", VPackValue(idString));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
  } else if (type == rest::RequestType::PUT ||
             type == rest::RequestType::DELETE_REQ) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else if (type == rest::RequestType::GET) {
    generateResult(rest::ResponseCode::OK, VPackSlice::emptyArraySlice());
  }
}

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

  // add client
  std::string const& value3 = _request->value("serverId", found);

  TRI_server_id_t serverId = 0;
  if (!found || (!value3.empty() && value3 != "none")) {
    serverId = static_cast<TRI_server_id_t>(StringUtils::uint64(value3));
  }

  bool includeSystem = _request->parsedValue("includeSystem", true);
  uint64_t chunkSize = _request->parsedValue<uint64_t>("chunkSize", 1024 * 1024);

  grantTemporaryRights();

  // extract collection
  TRI_voc_cid_t cid = 0;
  std::string const& value6 = _request->value("collection", found);
  if (found) {
    auto c = _vocbase.lookupCollection(value6);

    if (c == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      return;
    }

    cid = c->id();
  }

  auto trxContext = transaction::StandaloneContext::Create(_vocbase);
  VPackBuilder builder(trxContext->getVPackOptions());

  builder.openArray();

  auto result = tailWal(&_vocbase, tickStart, tickEnd, static_cast<size_t>(chunkSize),
                        includeSystem, cid, builder
  );

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
  TRI_ASSERT(length == 0 || result.maxTick() > 0);

  if (length == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }

  // transfer ownership of the buffer contents
  _response->setContentType(rest::ContentType::DUMP);

  // set headers
  _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                         checkMore ? "true" : "false");
  _response->setHeaderNC(
      StaticStrings::ReplicationHeaderLastIncluded,
      StringUtils::itoa((length == 0) ? 0 : result.maxTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick, StringUtils::itoa(latest));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastScanned, StringUtils::itoa(result.lastScannedTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderActive, "true"); // TODO remove
  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                         result.minTickIncluded() ? "true" : "false");

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
        //LOG_TOPIC(INFO, Logger::REPLICATION) << marker.toJson(trxContext->getVPackOptions());
      }
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
  _vocbase.updateReplicationClient(
    serverId,
    tickStart == 0 ? 0 : tickStart - 1,
    replutils::BatchInfo::DefaultTimeout
  );
}

/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
void RocksDBRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  generateResult(rest::ResponseCode::OK, VPackSlice::emptyArraySlice());
  // rocksdb only includes finished transactions in the WAL.
  _response->setContentType(rest::ContentType::DUMP);
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick, "0");
  // always true to satisfy continuous syncer
  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent, "true");
}

void RocksDBRestReplicationHandler::handleCommandInventory() {
  bool found;
  std::string batchId = _request->value("batchId", found);
  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
    return;
  }
  
  RocksDBReplicationContext* ctx = _manager->find(StringUtils::uint64(batchId));
  RocksDBReplicationContextGuard guard(_manager, ctx);
  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "context was not found");
    return;
  }

  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  // include system collections?
  bool includeSystem = _request->parsedValue("includeSystem", true);

  // produce inventory for all databases?
  bool isGlobal = false;
  getApplier(isGlobal);

  VPackBuilder builder;
  builder.openObject();

  // add collections and views
  Result res;
  if (isGlobal) {
    builder.add(VPackValue("databases"));
    res = ctx->getInventory(_vocbase, includeSystem, true, builder);
  } else {
    grantTemporaryRights();
    res = ctx->getInventory(_vocbase, includeSystem, false, builder);
    TRI_ASSERT(builder.hasKey("collections") &&
               builder.hasKey("views"));
  }

  if (res.fail()) {
    generateError(rest::ResponseCode::BAD, res.errorNumber(),
                  "inventory could not be created");
    return;
  }

  const std::string snapTick = std::to_string(ctx->snapshotTick());
  // <state>
  builder.add("state", VPackValue(VPackValueType::Object));
  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(snapTick));
  builder.add("lastUncommittedLogTick", VPackValue(snapTick));
  builder.add("totalEvents", VPackValue(ctx->snapshotTick()));
  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();  // </state>
  builder.add("tick", VPackValue(std::to_string(tick)));
  builder.close();  // Toplevel
  
  generateResult(rest::ResponseCode::OK, builder.slice());
}

/// @brief produce list of keys for a specific collection
void RocksDBRestReplicationHandler::handleCommandCreateKeys() {
  std::string const& collection = _request->value("collection");
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }
  // to is ignored because the snapshot time is the latest point in time

  RocksDBReplicationContext* ctx = nullptr;
  //get batchId from url parameters
  bool found;
  std::string batchId = _request->value("batchId", found);

  // find context
  if (found) {
    ctx = _manager->find(StringUtils::uint64(batchId));
  }
  RocksDBReplicationContextGuard guard(_manager, ctx);
  if (!found || ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
    return;
  }

  // bind collection to context - will initialize iterator
  Result res;
  TRI_voc_cid_t cid; uint64_t numDocs;
  std::tie(res, cid, numDocs) = ctx->bindCollectionIncremental(_vocbase, collection);

  if (res.fail()) {
    generateError(res);
    return;
  }
  
  // keysId = <batchId>-<cid>
  std::string keysId = StringUtils::itoa(ctx->id());
  keysId.push_back('-');
  keysId.append(StringUtils::itoa(cid));
  
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("id", VPackValue(keysId));
  result.add("count", VPackValue(numDocs));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

static std::pair<uint64_t, TRI_voc_cid_t>
  extractBatchAndCid(std::string const& input) {
  auto pos = input.find('-');
  if (pos != std::string::npos && input.size() > pos + 1 && pos > 1) {
    return std::make_pair(StringUtils::uint64(input.c_str(), pos),
                          StringUtils::uint64(input.substr(pos+1)));
  }
  return std::make_pair(0, 0);
}

/// @brief returns all key ranges
void RocksDBRestReplicationHandler::handleCommandGetKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/replication/keys/<keys-id>");
    return;
  }

  static uint64_t const DefaultChunkSize = 5000;

  // determine chunk size
  uint64_t chunkSize = _request->parsedValue("chunkSize", DefaultChunkSize);

  if (chunkSize < 100) {
    chunkSize = DefaultChunkSize;
  } else if (chunkSize > 20000) {
    chunkSize = 20000;
  }

  //first suffix needs to be the key id
  std::string const& keysId = suffixes[1]; // <batchId>-<cid>
  uint64_t batchId; TRI_voc_cid_t cid;
  std::tie(batchId, cid) = extractBatchAndCid(keysId);

  // get context
  RocksDBReplicationContext* ctx = _manager->find(batchId);
  //lock context
  RocksDBReplicationContextGuard guard(_manager, ctx);

  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified, expired or invalid in another way");
    return;
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  ctx->dumpKeyChunks(_vocbase, cid, builder, chunkSize);
  generateResult(rest::ResponseCode::OK, std::move(buffer));
}

/// @brief returns date for a key range
void RocksDBRestReplicationHandler::handleCommandFetchKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/replication/keys/<keys-id>");
    return;
  }

  static uint64_t const DefaultChunkSize = 5000;

  // determine chunk size
  uint64_t chunkSize = _request->parsedValue("chunkSize", DefaultChunkSize);
  if (chunkSize < 100) {
    chunkSize = DefaultChunkSize;
  } else if (chunkSize > 20000) {
    chunkSize = 20000;
  }

  // chunk is supplied by old clients, low is an optimization
  // for rocksdb, because seeking should be cheaper
  size_t chunk = static_cast<size_t>(_request->parsedValue("chunk", uint64_t(0)));

  bool found;
  std::string const& lowKey = _request->value("low", found);
  std::string const& value = _request->value("type", found);

  bool keys = true;
  if (value == "keys") {
    keys = true;
  } else if (value == "docs") {
    keys = false;
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid 'type' value");
    return;
  }

  //first suffix needs to be the key id
  std::string const& keysId = suffixes[1]; // <batchId>-<cid>
  uint64_t batchId; TRI_voc_cid_t cid;
  std::tie(batchId, cid) = extractBatchAndCid(keysId);

  RocksDBReplicationContext* ctx = _manager->find(batchId);
  RocksDBReplicationContextGuard guard(_manager, ctx);
  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified or not found");
    return;
  }
  
  auto transactionContext = transaction::StandaloneContext::Create(_vocbase);
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer, transactionContext->getVPackOptions());

  if (keys) {
    Result rv = ctx->dumpKeys(_vocbase, cid, builder, chunk,
                              static_cast<size_t>(chunkSize), lowKey);
    if (rv.fail()) {
      generateError(rv);
      return;
    }
  } else {
    
    size_t offsetInChunk = 0;
    size_t maxChunkSize = SIZE_MAX;
    std::string const& value2 = _request->value("offset", found);
    if (found) {
      offsetInChunk = static_cast<size_t>(StringUtils::uint64(value2));
      // "offset" was introduced with ArangoDB 3.3. if the client sends it,
      // it means we can adapt the result size dynamically and the client
      // may refetch data for the same chunk
      maxChunkSize = 8 * 1024 * 1024;
      // if a client does not send an "offset" parameter at all, we are
      // not sure if it supports this protocol (3.2 and before) or not
    }
    
    bool success = false;
    VPackSlice const parsedIds = this->parseVPackBody(success);
    if (!success) {
      generateResult(rest::ResponseCode::BAD, VPackSlice());
      return;
    }

    Result rv = ctx->dumpDocuments(_vocbase, cid, builder, chunk,
                                   static_cast<size_t>(chunkSize), offsetInChunk,
                                   maxChunkSize, lowKey, parsedIds);

    if (rv.fail()) {
      generateError(rv);
      return;
    }
  }

  generateResult(rest::ResponseCode::OK, std::move(buffer),
                 transactionContext);
}

void RocksDBRestReplicationHandler::handleCommandRemoveKeys() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/replication/keys/<keys-id>");
    return;
  }

  //first suffix needs to be the key id
  std::string const& keysId = suffixes[1]; // <batchId>-<cid>
  uint64_t batchId; TRI_voc_cid_t cid;
  std::tie(batchId, cid) = extractBatchAndCid(keysId);

  RocksDBReplicationContext* ctx = _manager->find(batchId);
  RocksDBReplicationContextGuard guard(_manager, ctx);
  if (ctx != nullptr) {
    ctx->releaseIterators(_vocbase, cid);
  }
  
  
  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add("id", VPackValue(keysId));  // id as a string
  resultBuilder.add(StaticStrings::Error, VPackValue(false));
  resultBuilder.add(StaticStrings::Code,
                    VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  resultBuilder.close();

  generateResult(rest::ResponseCode::ACCEPTED, resultBuilder.slice());
}

void RocksDBRestReplicationHandler::handleCommandDump() {
  LOG_TOPIC(TRACE, arangodb::Logger::REPLICATION) << "enter handleCommandDump";

  bool found = false;
  uint64_t contextId = 0;

  // contains dump options that might need to be inspected
  // VPackSlice options = _request->payload();

  // get collection Name
  std::string const& cname = _request->value("collection");
  if (cname.empty()) {
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
  RocksDBReplicationContext* ctx = _manager->find(contextId, /*ttl*/0);
  RocksDBReplicationContextGuard guard(_manager, ctx);

  if (ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - unable to find context (it could be expired)");
    return;
  }

  // print request
  LOG_TOPIC(TRACE, arangodb::Logger::REPLICATION)
      << "requested collection dump for collection '" << collection
      << "' using contextId '" << ctx->id() << "'";

  grantTemporaryRights();

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr &&
      !exec->canUseCollection(_vocbase.name(), cname, auth::Level::RO)) {
    generateError(rest::ResponseCode::FORBIDDEN,
                  TRI_ERROR_FORBIDDEN);
    return;
  }

  uint64_t chunkSize = determineChunkSize();
  size_t reserve = std::max<size_t>(chunkSize, 8192);

  RocksDBReplicationContext::DumpResult res(TRI_ERROR_NO_ERROR);
  if (request()->contentTypeResponse() == rest::ContentType::VPACK) {

    VPackBuffer<uint8_t> buffer;
    buffer.reserve(reserve); // avoid reallocs

    res = ctx->dumpVPack(_vocbase, cname, buffer, chunkSize);
    // generate the result
    if (res.fail()) {
      generateError(res);
    } else if (buffer.byteSize() == 0) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      resetResponse(rest::ResponseCode::OK);
      _response->setContentType(rest::ContentType::VPACK);
      _response->setPayload(std::move(buffer), true, VPackOptions::Options::Defaults,
                            /*resolveExternals*/ false);
    }

    // set headers
    _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                           (res.hasMore ? "true" : "false"));

    _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                           StringUtils::itoa(buffer.empty() ? 0 : res.includedTick));

  } else {
    auto response = dynamic_cast<HttpResponse*>(_response.get());
    StringBuffer dump(reserve, false);
    if (response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
    }

    // do the work!
    res = ctx->dumpJson(_vocbase, cname, dump, determineChunkSize());

    if (res.fail()) {
      if (res.is(TRI_ERROR_BAD_PARAMETER)) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "replication dump - " + res.errorMessage());
        return;
      }

      generateError(rest::ResponseCode::SERVER_ERROR, res.errorNumber(),
                    "replication dump - " + res.errorMessage());
      return;
    }

    // generate the result
    if (dump.length() == 0) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      resetResponse(rest::ResponseCode::OK);
    }

    response->setContentType(rest::ContentType::DUMP);
    // set headers
    _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                           (res.hasMore ? "true" : "false"));
    _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                           StringUtils::itoa((dump.length() == 0) ? 0 : res.includedTick));

    // transfer ownership of the buffer contents
    response->body().set(dump.stringBuffer());

    // avoid double freeing
    TRI_StealStringBuffer(dump.stringBuffer());
  }
}
