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

#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ClusterMethods.h"
#include "Replication/InitialSyncer.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/replication-applier.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;


static Result restoreDataParser(char const* ptr, char const* pos,
                                std::string const& collectionName,
                                bool useRevision, std::string& key,
                                VPackBuilder& builder, VPackSlice& doc,
                                TRI_replication_operation_e& type) {
  builder.clear();

  try {
    VPackParser parser(builder);
    parser.parse(ptr, static_cast<size_t>(pos - ptr));
  } catch (VPackException const&) {
    // Could not parse the given string
    return Result{
      TRI_ERROR_HTTP_CORRUPTED_JSON,
      "received invalid JSON data for collection " + collectionName
    };
  } catch (std::exception const&) {
    // Could not even build the string
    return Result{
      TRI_ERROR_HTTP_CORRUPTED_JSON,
      "received invalid JSON data for collection " + collectionName
    };
  } catch (...) {
    return Result{TRI_ERROR_INTERNAL};
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    return Result{
      TRI_ERROR_HTTP_CORRUPTED_JSON,
      "received invalid JSON data for collection " + collectionName
    };
  }

  type = REPLICATION_INVALID;

  for (auto const& pair : VPackObjectIterator(slice, true)) {
    if (!pair.key.isString()) {
      return Result{
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        "received invalid JSON data for collection " + collectionName
      };
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
      }
    }

    else if (attributeName == "key") {
      if (key.empty()) {
        key = pair.value.copyString();
      }
    }
  }

  if (type == REPLICATION_MARKER_DOCUMENT && !doc.isObject()) {
    return Result{
      TRI_ERROR_HTTP_BAD_PARAMETER,
      "got document marker without contents"
    };
  }

  if (key.empty()) {
    return Result{
      TRI_ERROR_HTTP_BAD_PARAMETER,
      "received invalid JSON data for collection " + collectionName
    };
  }

  return Result{TRI_ERROR_NO_ERROR};
}

RestReplicationHandler::RestReplicationHandler(
    GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestReplicationHandler::~RestReplicationHandler() {}

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
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandMakeSlave() {
  bool success;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
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
  std::string const jwt = VelocyPackHelper::getStringValue(body, "jwt", "");
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
  config._jwt = jwt;
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
                         restrictType, false, false);

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
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator() {
  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request");
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
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    generateError(rest::ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN,
                  "shutting down server");
    return;
  }

  std::unique_ptr<ClusterCommResult> res;
  if (!useVst) {
    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
    if (httpRequest == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid request type");
    }

    // Send a synchronous request to that shard using ClusterComm:
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), *headers, 300.0);
  } else {
    // do we need to handle multiple payloads here - TODO
    // here we switch vorm vst to http?!
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

  if (!useVst) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
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

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<std::shared_ptr<LogicalCollection>> cols =
      ci->getCollections(dbName);

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(VPackValue("collections"));
  resultBuilder.openArray();
  for (auto const& c : cols) {
    // We want to check if the collection is usable and all followers
    // are in sync:
    auto shardMap = c->shardIds();
      // shardMap is an unordered_map from ShardId (string) to a vector of
      // servers (strings), wrapped in a shared_ptr
    auto cic = ci->getCollectionCurrent(dbName,
                                        basics::StringUtils::itoa(c->cid()));
    // Check all shards:
    bool isReady = true;
    for (auto const& p : *shardMap) {
      auto currentServerList = cic->servers(p.first  /* shardId */);
      if (!ClusterHelpers::compareServerLists(p.second, currentServerList)) {
        isReady = false;
      }
    }
    c->toVelocyPackForClusterInventory(resultBuilder, includeSystem, isReady);
  }
  resultBuilder.close();  // collections
  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  auto tickString = std::to_string(tick);
  resultBuilder.add("tick", VPackValue(tickString));
  resultBuilder.add("state", VPackValue("unused"));
  resultBuilder.close();  // base
  generateResult(rest::ResponseCode::OK, resultBuilder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreIndexes() {
  std::shared_ptr<VPackBuilder> parsedRequest;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr();
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
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData() {
  std::string const& colName = _request->value("collection");

  if (colName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  bool recycleIds = false;
  std::string const& value2 = _request->value("recycleIds");

  if (!value2.empty()) {
    recycleIds = StringUtils::boolean(value2);
  }

  Result res = processRestoreData(colName, recycleIds);

  if (res.fail()) {
    if (res.errorMessage().empty()) {
      generateError(GeneralResponse::responseCode(res.errorNumber()), res.errorNumber());
    } else {
      generateError(GeneralResponse::responseCode(res.errorNumber()), res.errorNumber(),
                    std::string(TRI_errno_string(res.errorNumber())) + ": " + res.errorMessage());
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
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreData(
    std::string const& colName, bool useRevision) {

  grantTemporaryRights();

  if (colName == "_users") {
    // We need to handle the _users in a special way
    return processRestoreUsersBatch(colName, useRevision);
  }
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), colName,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

  Result res = trx.begin();

  if (!res.ok()) {
    res.reset(res.errorNumber(), std::string("unable to start transaction (")
        + std::string(__FILE__) + std::string(":") + std::to_string(__LINE__)
        + std::string("): ") + res.errorMessage());
    return res;
  }

  res = processRestoreDataBatch(trx, colName, useRevision);
  res = trx.finish(res);

  return res;
}

Result RestReplicationHandler::parseBatch(
    std::string const& collectionName,
    bool useRevision,
    std::unordered_map<std::string, VPackValueLength>& latest,
    VPackBuilder& allMarkers) {

  VPackBuilder builder;
  allMarkers.clear();

  HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
  if (httpRequest == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
  }

  std::string const& bodyStr = httpRequest->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

  VPackValueLength currentPos = 0;

  // First parse and collect all markers, we assemble everything in one
  // large builder holding an array. We keep for each key the latest
  // entry.

  {
    VPackArrayBuilder guard(&allMarkers);
    std::string key;
    while (ptr < end) {
      char const* pos = strchr(ptr, '\n');

      if (pos == nullptr) {
        pos = end;
      } else {
        *((char*)pos) = '\0';
      }

      if (pos - ptr > 1) {
        // found something
        key.clear();
        VPackSlice doc;
        TRI_replication_operation_e type = REPLICATION_INVALID;

        Result res = restoreDataParser(ptr, pos, collectionName,
                                       useRevision, key, builder, doc, type);
        if (res.fail()) {
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

  return Result{TRI_ERROR_NO_ERROR};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of _users collection
/// Special case, we shall allow to import in this collection,
/// however it is not sharded by key and we need to replace by name instead of by key
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreUsersBatch(
    std::string const& collectionName, 
    bool useRevision) {
  std::unordered_map<std::string, VPackValueLength> latest;
  VPackBuilder allMarkers;

  parseBatch(collectionName, useRevision, latest, allMarkers);

  VPackSlice allMarkersSlice = allMarkers.slice();

  std::string aql("FOR u IN @restored UPSERT {name: u.name} INSERT u REPLACE u INTO @@collection OPTIONS {ignoreErrors: true, silent: true, waitForSync: false, isRestore: true}");

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject(); 
  bindVars->add("@collection", VPackValue(collectionName));
  bindVars->add(VPackValue("restored"));
  bindVars->openArray();

  // Now loop over the markers and insert the docs, ignoring _key and _id
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
      // In the _users case we silently remove the _key value.
      bindVars->openObject();
      for (auto const& it : VPackObjectIterator(doc)) {
        if (StringRef(it.key) != StaticStrings::KeyString && 
            StringRef(it.key) != StaticStrings::IdString) {
          bindVars->add(it.key);
          bindVars->add(it.value);
        }
      }
      bindVars->close();
    }
  }

  bindVars->close(); // restored
  bindVars->close(); // bindVars

  arangodb::aql::Query query(false, _vocbase, arangodb::aql::QueryString(aql),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);

  auto queryResult = query.execute(queryRegistry);
  return Result{queryResult.code};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreDataBatch(
    transaction::Methods& trx, std::string const& collectionName,
    bool useRevision) {

  VPackBuilder builder;

  std::unordered_map<std::string, VPackValueLength> latest;
  VPackBuilder allMarkers;

  parseBatch(collectionName, useRevision, latest, allMarkers);

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
        return Result{
          TRI_ERROR_REPLICATION_UNEXPECTED_MARKER,
          "unexpected marker type " + StringUtils::itoa(type)
        };
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
  if (ServerState::instance()->isCoordinator() &&
     collectionName == "_users") {
    // Special-case for _users, we need to remove the _key and _id from the marker
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
        // In the _users case we silently remove the _key value.
        builder.openObject();
        for (auto const& it : VPackObjectIterator(doc)) {
          if (StringRef(it.key) != StaticStrings::KeyString && 
              StringRef(it.key) != StaticStrings::IdString) {
            builder.add(it.key);
            builder.add(it.value);
          }
        }
        builder.close();
        builder.add(doc);
      }
    }
  } else {
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

void RestReplicationHandler::grantTemporaryRights() {
  AuthenticationFeature* auth =
      FeatureCacheFeature::instance()->authenticationFeature();
  if (auth->isActive() && ExecContext::CURRENT != nullptr) {
    AuthLevel level = auth->canUseDatabase(ExecContext::CURRENT->user(), _vocbase->name());
    if (level == AuthLevel::RW) {
      // If you have administrative access on this database,
      // we grant you everything for restore.
      ExecContext::CURRENT = nullptr;
    }
  }
}
