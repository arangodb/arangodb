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
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Replication/common-defines.h"
#include "Replication/InitialSyncer.h"
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
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

struct MyTypeHandler final : public VPackCustomTypeHandler {
  explicit MyTypeHandler(TRI_vocbase_t* vocbase) : resolver(vocbase) {}

  ~MyTypeHandler() {}

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  CollectionNameResolver resolver;
};

RestWalAccessHandler::RestWalAccessHandler(GeneralRequest* request,
                                           GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

bool RestWalAccessHandler::parseFilter(WalAccess::Filter& filter) {
  bool found = false;
  std::string const& value1 = _request->value("global", found);
  if (found && StringUtils::boolean(value1)) {
    if (!_vocbase->isSystem()) {
      generateError(
          rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
          "global tailing is only possible from within _system database");
      return false;
    }
  } else {
    // filter for collection
    filter.vocbase = _vocbase->id();

    // extract collection
    std::string const& value2 = _request->value("collection", found);
    if (found) {
      LogicalCollection* c = _vocbase->lookupCollection(value2);
      if (c == nullptr) {
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
        return false;
      }
      filter.collection = c->cid();
    }
  }

  std::string const& value3 = _request->value("includeSystem", found);
  if (found) {
    filter.includeSystem = StringUtils::boolean(value3);
  }

  // grab list of transactions from the body value
  if (_request->requestType() == arangodb::rest::RequestType::PUT) {
    std::string const& value4 = _request->value("firstRegularTick", found);
    if (found) {
      filter.firstRegularTick =
          static_cast<TRI_voc_tick_t>(StringUtils::uint64(value4));
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
  if (ExecContext::CURRENT == nullptr ||
      !ExecContext::CURRENT->isAdminUser()) {
    generateError(ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  std::vector<std::string> suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET _api/wal/[tail|range|lastTick]>");
    return RestStatus::DONE;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);  // Engine not loaded. Startup broken
  WalAccess const* wal = engine->walAccess();
  TRI_ASSERT(wal != nullptr);

  if (suffixes[0] == "range" && _request->requestType() == RequestType::GET) {
    handleCommandTickRange(wal);
  } else if (suffixes[0] == "lastTick" &&
             _request->requestType() == RequestType::GET) {
    handleCommandLastTick(wal);
  } else if (suffixes[0] == "tail" &&
             (_request->requestType() == RequestType::GET ||
              _request->requestType() == RequestType::PUT)) {
    handleCommandTail(wal);
  } else if (suffixes[0] == "open-transactions" &&
             _request->requestType() == RequestType::GET) {
    handleCommandDetermineOpenTransactions(wal);
  } else {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected GET _api/wal/[tail|range|lastTick|open-transactions]>");
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
      server->add("serverId",
                  VPackValue(std::to_string(ServerIdFeature::getId())));
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
    server->add("serverId",
                VPackValue(std::to_string(ServerIdFeature::getId())));
  }
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestWalAccessHandler::handleCommandTail(WalAccess const* wal) {
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

  WalAccess::Filter filter;
  if (!parseFilter(filter)) {
    return;
  }

  size_t chunkSize = 1024 * 1024;  
  std::string const& value5 = _request->value("chunkSize", found);
  if (found) {
    chunkSize = static_cast<size_t>(StringUtils::uint64(value5));
    chunkSize = std::min((size_t)128 * 1024 * 1024, chunkSize);
  }
  
  // check if a barrier id was specified in request
  TRI_voc_tid_t barrierId = 0;
  std::string const& value3 = _request->value("barrier", found);
  
  if (found) {
    barrierId = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value3));
  }

  WalAccessResult result;
  std::map<TRI_voc_tick_t, MyTypeHandler> handlers;
  VPackOptions opts = VPackOptions::Defaults;
  auto prepOpts = [&handlers, &opts](TRI_vocbase_t* vocbase) {
    auto const& it = handlers.find(vocbase->id());
    if (it == handlers.end()) {
      auto const& res = handlers.emplace(vocbase->id(), MyTypeHandler(vocbase));
      opts.customTypeHandler = &(res.first->second);
    } else {
      opts.customTypeHandler = &(it->second);
    }
  };

  size_t length = 0;
  if (useVst) {
    result =
        wal->tail(tickStart, tickEnd, chunkSize, barrierId, filter,
                  [&](TRI_vocbase_t* vocbase, VPackSlice const& marker) {
                    length++;
                    if (vocbase != nullptr) {  // database drop has no vocbase
                      prepOpts(vocbase);
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
    result =
        wal->tail(tickStart, tickEnd, chunkSize, barrierId, filter,
                  [&](TRI_vocbase_t* vocbase, VPackSlice const& marker) {
                    length++;
                    if (vocbase != nullptr) {  // database drop has no vocbase
                      prepOpts(vocbase);
                    }
                    dumper.dump(marker);
                    buffer.appendChar('\n');
                    //LOG_TOPIC(INFO, Logger::FIXME) << marker.toJson(&opts);
                  });
  }

  if (result.fail()) {
    generateError(result);
    return;
  }

  // transfer ownership of the buffer contents
  _response->setContentType(rest::ContentType::DUMP);

  // set headers
  bool checkMore = result.lastIncludedTick() > 0 &&
                   result.lastIncludedTick() < result.latestTick();
  _response->setHeaderNC(StaticStrings::ReplicationHeaderCheckMore,
                         checkMore ? "true" : "false");
  _response->setHeaderNC(
      StaticStrings::ReplicationHeaderLastIncluded,
      StringUtils::itoa(result.lastIncludedTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderLastTick,
                         StringUtils::itoa(result.latestTick()));
  _response->setHeaderNC(StaticStrings::ReplicationHeaderActive, "true");
  _response->setHeaderNC(StaticStrings::ReplicationHeaderFromPresent,
                         result.fromTickIncluded() ? "true" : "false");

  if (length > 0) {
    _response->setResponseCode(rest::ResponseCode::OK);
    // add client
    bool found;
    std::string const& value = _request->value("serverId", found);

    TRI_server_id_t serverId = 0;
    if (found) {
      serverId = static_cast<TRI_server_id_t>(StringUtils::uint64(value));
    }

    DatabaseFeature::DATABASE->enumerateDatabases([&](TRI_vocbase_t* vocbase) {
      vocbase->updateReplicationClient(serverId, result.lastIncludedTick(), InitialSyncer::defaultBatchTimeout);
    });
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "WAL tailing after " << tickStart
      << ", lastIncludedTick " << result.lastIncludedTick()
      << ", fromTickIncluded " << result.fromTickIncluded();
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "No more data in WAL after " << tickStart;
    _response->setResponseCode(rest::ResponseCode::NO_CONTENT);
  }
}

void RestWalAccessHandler::handleCommandDetermineOpenTransactions(
    WalAccess const* wal) {
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
  if (!parseFilter(filter)) {
    return;
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openArray();
  WalAccessResult r =
      wal->openTransactions(minMax.first, minMax.second, filter,
                            [&](TRI_voc_tick_t tick, TRI_voc_tid_t tid) {
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
