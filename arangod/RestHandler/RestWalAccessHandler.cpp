////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestWalAccessHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Basics/tryEmplaceHelper.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "Replication/Syncer.h"
#include "Replication/common-defines.h"
#include "Replication/utilities.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/WalAccess.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

struct MyTypeHandler final : public VPackCustomTypeHandler {
  explicit MyTypeHandler(TRI_vocbase_t& vocbase) : resolver(vocbase) {}

  ~MyTypeHandler() = default;

  void dump(VPackSlice const& value, VPackDumper* dumper, VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  CollectionNameResolver resolver;
};

RestWalAccessHandler::RestWalAccessHandler(application_features::ApplicationServer& server,
                                           GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

bool RestWalAccessHandler::parseFilter(WalAccess::Filter& filter) {
  // determine start and end tick
  filter.tickStart = _request->parsedValue<uint64_t>("from", filter.tickStart);
  filter.tickLastScanned =
      _request->parsedValue<uint64_t>("lastScanned", filter.tickLastScanned);

  // determine end tick for dump
  filter.tickEnd = _request->parsedValue("to", filter.tickEnd);
  if (filter.tickStart > filter.tickEnd || filter.tickEnd == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return false;
  }

  bool global = _request->parsedValue("global", false);
  if (global) {
    if (!_vocbase.isSystem()) {
      generateError(
          rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
          "global tailing is only possible from within _system database");
      return false;
    }
  } else {
    // filter for database
    filter.vocbase = _vocbase.id();

    // extract collection
    bool found = false;
    std::string const& value2 = _request->value("collection", found);
    if (found) {
      auto c = _vocbase.lookupCollection(value2);

      if (c == nullptr) {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
        return false;
      }

      // filter for collection
      filter.collection = c->id();
    }
  }

  filter.includeSystem = _request->parsedValue("includeSystem", filter.includeSystem);
  filter.includeFoxxQueues = _request->parsedValue("includeFoxxQueues", false);

  // grab list of transactions from the body value
  if (_request->requestType() == arangodb::rest::RequestType::PUT) {
    filter.firstRegularTick =
        _request->parsedValue<uint64_t>("firstRegularTick", 0);

    VPackSlice slice;
    try {
      slice = _request->payload(true);
    } catch (...) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return false;
    }
    if (!slice.isArray()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting array");
      return false;
    }

    for (auto const& id : VPackArrayIterator(slice)) {
      if (!id.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "invalid body value. expecting array of ids");
        return false;
      }
      filter.transactionIds.emplace(StringUtils::uint64(id.copyString()));
    }
  }

  return true;
}

RestStatus RestWalAccessHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/wal' is not yet supported in a cluster");
    return RestStatus::DONE;
  }

  if (!_context.isAdminUser()) {
    generateError(ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET /_api/wal/[tail|range|lastTick|open-transactions]>");
    return RestStatus::DONE;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);  // Engine not loaded. Startup broken
  WalAccess const* wal = engine->walAccess();
  TRI_ASSERT(wal != nullptr);

  if (suffixes[0] == "range" && _request->requestType() == RequestType::GET) {
    handleCommandTickRange(wal);
  } else if (suffixes[0] == "lastTick" && _request->requestType() == RequestType::GET) {
    handleCommandLastTick(wal);
  } else if (suffixes[0] == "tail" && (_request->requestType() == RequestType::GET ||
                                       _request->requestType() == RequestType::PUT)) {
    handleCommandTail(wal);
  } else if (suffixes[0] == "open-transactions" &&
             _request->requestType() == RequestType::GET) {
    handleCommandDetermineOpenTransactions(wal);
  } else {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected GET /_api/wal/[tail|range|lastTick|open-transactions]>");
  }

  return RestStatus::DONE;
}

void RestWalAccessHandler::handleCommandTickRange(WalAccess const* wal) {
  std::pair<TRI_voc_tick_t, TRI_voc_tick_t> minMax;
  Result r = wal->tickRange(minMax);
  if (r.ok()) {
    /// {"tickMin":"123", "tickMax":"456", "version":"3.2", "serverId":"abc"}
    VPackBuilder result;
    result.openObject();
    result.add("time", VPackValue(utilities::timeString()));
    // "state" part
    result.add("tickMin", VPackValue(std::to_string(minMax.first)));
    result.add("tickMax", VPackValue(std::to_string(minMax.second)));
    {  // "server" part
      VPackObjectBuilder server(&result, "server", true);
      server->add("version", VPackValue(ARANGODB_VERSION));
      server->add("serverId", VPackValue(std::to_string(ServerIdFeature::getId().id())));
    }
    result.close();
    generateResult(rest::ResponseCode::OK, result.slice());
  } else {
    generateError(r);
  }
}

void RestWalAccessHandler::handleCommandLastTick(WalAccess const* wal) {
  VPackBuilder result;
  result.openObject();
  result.add("time", VPackValue(utilities::timeString()));
  // "state" part
  result.add("tick", VPackValue(std::to_string(wal->lastTick())));
  {  // "server" part
    VPackObjectBuilder server(&result, "server", true);
    server->add("version", VPackValue(ARANGODB_VERSION));
    server->add("serverId", VPackValue(std::to_string(ServerIdFeature::getId().id())));
  }
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestWalAccessHandler::handleCommandTail(WalAccess const* wal) {
  // track the number of parallel invocations of the tailing API
  auto& rf = _vocbase.server().getFeature<ReplicationFeature>();
  // this may throw when too many threads are going into tailing
  rf.trackTailingStart();

  auto guard = scopeGuard([&rf]() { rf.trackTailingEnd(); });

  bool const useVst = (_request->transportType() == Endpoint::TransportType::VST);

  WalAccess::Filter filter;
  if (!parseFilter(filter)) {
    return;
  }

  // check for serverId
  ServerId const clientId{StringUtils::uint64(_request->value("serverId"))};
  SyncerId const syncerId = SyncerId::fromRequest(*_request);
  std::string const clientInfo = _request->value("clientInfo");

  // check if a barrier id was specified in request
  TRI_voc_tid_t barrierId =
      _request->parsedValue("barrier", static_cast<TRI_voc_tid_t>(0));

  ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());

  bool found = false;
  size_t chunkSize = 1024 * 1024;
  std::string const& value5 = _request->value("chunkSize", found);
  if (found) {
    chunkSize = static_cast<size_t>(StringUtils::uint64(value5));
    chunkSize = std::min((size_t)128 * 1024 * 1024, chunkSize);
  }

  WalAccessResult result;
  std::map<TRI_voc_tick_t, std::unique_ptr<MyTypeHandler>> handlers;
  VPackOptions opts = VPackOptions::Defaults;
  auto prepOpts = [&handlers, &opts](TRI_vocbase_t& vocbase) -> void {
    auto it = handlers.try_emplace(
      vocbase.id(),
      arangodb::lazyConstruct([&]{
       return std::make_unique<MyTypeHandler>(vocbase);
      })
    ).first;
    opts.customTypeHandler = it->second.get();
  };

  size_t length = 0;

  if (useVst) {
    result = wal->tail(filter, chunkSize, barrierId,
                       [&](TRI_vocbase_t* vocbase, VPackSlice const& marker) {
                         length++;

                         if (vocbase != nullptr) {  // database drop has no vocbase
                           prepOpts(*vocbase);
                         }

                         _response->addPayload(marker, &opts, true);
                       });
  } else {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    TRI_ASSERT(httpResponse);
    if (httpResponse == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }
    basics::StringBuffer& buffer = httpResponse->body();
    basics::VPackStringBufferAdapter adapter(buffer.stringBuffer());
    // note: we need the CustomTypeHandler here
    VPackDumper dumper(&adapter, &opts);
    result = wal->tail(filter, chunkSize, barrierId,
                       [&](TRI_vocbase_t* vocbase, VPackSlice const& marker) {
                         length++;

                         if (vocbase != nullptr) {  // database drop has no vocbase
                           prepOpts(*vocbase);
                         }

                         dumper.dump(marker);
                         buffer.appendChar('\n');
                         // LOG_TOPIC("cda47", INFO, Logger::REPLICATION) <<
                         // marker.toJson(&opts);
                       });
  }

  if (result.fail()) {
    generateError(std::move(result).result());
    return;
  }

  // transfer ownership of the buffer contents
  _response->setContentType(rest::ContentType::DUMP);

  TRI_ASSERT(result.latestTick() >= result.lastIncludedTick());
  TRI_ASSERT(result.latestTick() >= result.lastScannedTick());

  // set headers
  bool const checkMore = result.lastIncludedTick() > 0 &&
                         result.lastIncludedTick() < result.latestTick();
  _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                         checkMore ? "true" : "false");
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                         StringUtils::itoa(result.lastIncludedTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastScanned,
                         StringUtils::itoa(result.lastScannedTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick,
                         StringUtils::itoa(result.latestTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                         result.fromTickIncluded() ? "true" : "false");

  if (length > 0) {
    _response->setResponseCode(rest::ResponseCode::OK);
    LOG_TOPIC("078ad", DEBUG, Logger::REPLICATION)
        << "WAL tailing after " << filter.tickStart << ", lastIncludedTick "
        << result.lastIncludedTick() << ", fromTickIncluded "
        << result.fromTickIncluded();
  } else {
    LOG_TOPIC("29624", DEBUG, Logger::REPLICATION)
        << "No more data in WAL after " << filter.tickStart;
    _response->setResponseCode(rest::ResponseCode::NO_CONTENT);
  }

  DatabaseFeature::DATABASE->enumerateDatabases([&](TRI_vocbase_t& vocbase) -> void {
    vocbase.replicationClients().track(syncerId, clientId, clientInfo, filter.tickStart,
                                       replutils::BatchInfo::DefaultTimeout);
  });
}

void RestWalAccessHandler::handleCommandDetermineOpenTransactions(WalAccess const* wal) {
  // determine start and end tick

  std::pair<TRI_voc_tick_t, TRI_voc_tick_t> minMax;
  Result res = wal->tickRange(minMax);
  if (res.fail()) {
    generateError(res);
    return;
  }

  bool found = false;
  std::string const& value1 = _request->value("from", found);
  if (found) {
    minMax.first = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
  }
  // determine end tick for dump
  std::string const& value2 = _request->value("to", found);
  if (found) {
    minMax.second = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
  }
  if (minMax.first > minMax.second || minMax.second == 0) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  // check whether a database was specified
  WalAccess::Filter filter;
  filter.tickStart = minMax.first;
  filter.tickEnd = minMax.second;
  if (!parseFilter(filter)) {
    return;
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openArray();
  WalAccessResult r =
      wal->openTransactions(filter, [&](TRI_voc_tick_t tick, TRI_voc_tid_t tid) {
        builder.add(VPackValue(std::to_string(tid)));
      });
  builder.close();

  _response->setContentType(rest::ContentType::DUMP);
  if (res.fail()) {
    generateError(res);
  } else {
    auto cc = r.lastIncludedTick() != 0 ? ResponseCode::OK : ResponseCode::NO_CONTENT;
    generateResult(cc, std::move(buffer));

    _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                           r.fromTickIncluded() ? "true" : "false");
    _response->setHeaderNC(StaticStrings::ReplicationHeaderLastIncluded,
                           StringUtils::itoa(r.lastIncludedTick()));
  }
}
